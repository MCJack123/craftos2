/*
 * terminal/SDLTerminal.cpp
 * CraftOS-PC 2
 * 
 * This file implements the SDLTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#include <fstream>
#include <sstream>
#include <configuration.hpp>
#include "RawTerminal.hpp"
#include "SDLTerminal.hpp"
#include "../gif.hpp"
#include "../main.hpp"
#include "../runtime.hpp"
#include "../termsupport.hpp"
#ifndef NO_WEBP
#include <webp/mux.h>
#include <webp/encode.h>
#endif
#ifndef NO_PNG
#include <png++/png.hpp>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#define EMSCRIPTEN_ENABLED 1
#else
#define EMSCRIPTEN_ENABLED 0
#endif
#define rgb(color) (((color).r << 16) | ((color).g << 8) | (color).b)

extern "C" {
    struct font_image {
        unsigned int 	 width;
        unsigned int 	 height;
        unsigned int 	 bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
        unsigned char	 pixel_data[128 * 175 * 2 + 1];
    };
    extern struct font_image font_image;
    struct favicon {
        unsigned int 	 width;
        unsigned int 	 height;
        unsigned int 	 bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
        unsigned char	 pixel_data[32 * 32 * 4 + 1];
    };
    extern struct favicon favicon;
}

unsigned SDLTerminal::fontScale = 2;
Uint32 SDLTerminal::lastWindow = 0;
SDL_Surface* SDLTerminal::bmp = NULL;
SDL_Surface* SDLTerminal::origfont = NULL;
std::unordered_multimap<SDL_EventType, std::pair<sdl_event_handler, void*> > SDLTerminal::eventHandlers;
SDL_Window* SDLTerminal::singleWin = NULL;
static int nextWindowID = 1;
#ifdef __EMSCRIPTEN__

extern "C" {
    void EMSCRIPTEN_KEEPALIVE nextRenderTarget() {
        if (++renderTarget == renderTargets.end()) renderTarget = renderTargets.begin();
        (*renderTarget)->changed = true;
    }

    void EMSCRIPTEN_KEEPALIVE previousRenderTarget() {
        if (renderTarget == renderTargets.begin()) renderTarget = renderTargets.end();
        renderTarget--;
        (*renderTarget)->changed = true;
    }

    bool EMSCRIPTEN_KEEPALIVE selectRenderTarget(int id) {
        for (renderTarget = renderTargets.begin(); renderTarget != renderTargets.end(); renderTarget++) if ((*renderTarget)->id == id) break;
        (*renderTarget)->changed = true;
        return renderTarget != renderTargets.end();
    }

    const char * EMSCRIPTEN_KEEPALIVE getRenderTargetName() {
        return (*renderTarget)->title.c_str();
    }

    extern void emsyncfs();
}

void onWindowCreate(int id, const char * title) {EM_ASM({if (Module.windowEventListener !== undefined) Module.windowEventListener.onWindowCreate($0, $1);}, id, title);}
void onWindowDestroy(int id) {EM_ASM({if (Module.windowEventListener !== undefined) Module.windowEventListener.onWindowDestroy($0);}, id);}
#endif

#ifdef __IPHONEOS__
extern void updateCloseButton();
extern void iosSetSafeAreaConstraints(SDLTerminal * term);
static Uint32 textInputTimer(Uint32 interval, void* param) {
    queueTask([](void*win)->void*{iosSetSafeAreaConstraints((SDLTerminal*)win); return NULL;}, param, true);
    return 0;
}
#endif

SDLTerminal::SDLTerminal(std::string title): Terminal(config.defaultWidth, config.defaultHeight) {
    this->title = title;
#ifdef __EMSCRIPTEN__
    dpiScale = emscripten_get_device_pixel_ratio();
#endif
#ifdef __ANDROID__
    float dpi = 0;
    SDL_GetDisplayDPI(0, &dpi, NULL, NULL);
    if (dpi >= 150) dpiScale = dpi / 150;
#endif
    if (config.customCharScale > 0) {
        charScale = config.customCharScale;
        charWidth = fontWidth * charScale;
        charHeight = fontHeight * charScale;
    }

    if (singleWindowMode && singleWin != NULL) win = singleWin;
    else {
        win = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int)(width*charWidth*dpiScale+(4 * charScale * dpiScale)), (int)(height*charHeight*dpiScale+(4 * charScale * dpiScale)), SDL_WINDOW_SHOWN | 
#if !(defined(__EMSCRIPTEN__) && defined(NO_EMSCRIPTEN_HIDPI))
            SDL_WINDOW_ALLOW_HIGHDPI |
#endif
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_INPUT_FOCUS);
        if (singleWindowMode && singleWin == NULL) singleWin = win;
        if (win == (SDL_Window*)0) {
            overridden = true;
            throw window_exception("Failed to create window: " + std::string(SDL_GetError()));
        }
        if (std::string(SDL_GetCurrentVideoDriver()) == "KMSDRM" || std::string(SDL_GetCurrentVideoDriver()) == "KMSDRM_LEGACY") {
            // KMS requires the window to be fullscreen to work
            // We also set the resolution to the highest possible so users don't get stuck at 640x480 because it's the default
            int idx = SDL_GetWindowDisplayIndex(win);
            SDL_DisplayMode mode, max;
            SDL_GetCurrentDisplayMode(idx, &max);
            for (int i = 0; i < SDL_GetNumDisplayModes(idx); i++) {
                SDL_GetDisplayMode(idx, i, &mode);
                if (mode.w > max.w || mode.h > max.h || (mode.w == max.w && mode.h == max.h && (mode.refresh_rate > max.refresh_rate || SDL_BITSPERPIXEL(mode.format) > SDL_BITSPERPIXEL(max.format)))) max = mode;
            }
            fprintf(stderr, "Setting display mode to %dx%dx%d@%d\n", max.w, max.h, SDL_BITSPERPIXEL(max.format), max.refresh_rate);
            SDL_SetWindowDisplayMode(win, &max);
            SDL_SetWindowFullscreen(win, SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
#if defined(__ANDROID__) || defined(__IPHONEOS__)
        SDL_GetWindowSize(win, &realWidth, &realHeight);
        width = (realWidth - 4*charScale*dpiScale) / (charWidth*dpiScale);
        height = (realHeight - 4*charScale*dpiScale) / (charHeight*dpiScale);
        this->screen.resize(width, height, ' ');
        this->colors.resize(width, height, 0xF0);
        this->pixels.resize(width * fontWidth, height * fontHeight, 0x0F);
#else
        realWidth = (int)(width*charWidth*dpiScale+(4 * charScale * dpiScale));
        realHeight = (int)(height*charHeight*dpiScale+(4 * charScale * dpiScale));
#endif
#ifdef __IPHONEOS__
        SDL_AddTimer(100, textInputTimer, this);
#else
        SDL_StartTextInput();
#endif
    }
    if (singleWindowMode) {
        id = nextWindowID++;
#ifdef __EMSCRIPTEN__
        onWindowCreate(id, title.c_str());
#endif
    } else id = SDL_GetWindowID(win);
    lastWindow = id;
#if !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    SDL_Surface* icon = SDL_CreateRGBSurfaceFrom(favicon.pixel_data, (int)favicon.width, (int)favicon.height, (int)favicon.bytes_per_pixel * 8, (int)favicon.width * (int)favicon.bytes_per_pixel, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    SDL_SetWindowIcon(win, icon);
    SDL_FreeSurface(icon);
#endif
    {
        std::lock_guard<std::mutex> lock(renderTargetsLock);
        renderTargets.push_back(this);
        renderTarget = --renderTargets.end();
    }
    //SDL_GetWindowSurface(win);
    onActivate();
#ifdef __IPHONEOS__
    updateCloseButton();
#endif
}

SDLTerminal::~SDLTerminal() {
    if (shouldRecord) stopRecording();
#ifdef __EMSCRIPTEN__
    onWindowDestroy(id);
#endif
    if (singleWindowMode && *renderTarget == this) previousRenderTarget();
    {std::lock_guard<std::mutex> locked_g(renderlock);} {
        std::lock_guard<std::mutex> lock(renderTargetsLock);
        if (singleWindowMode) {
            const auto pos = currentWindowIDs.find(id);
            if (pos != currentWindowIDs.end()) currentWindowIDs.erase(pos);
        }
        for (auto it = renderTargets.begin(); it != renderTargets.end(); ++it) {
            if (*it == this)
                it = renderTargets.erase(it);
            if (it == renderTargets.end()) break;
        }
    }
    if (!overridden) {
        if (surf != NULL) SDL_FreeSurface(surf);
        if ((!singleWindowMode || renderTargets.size() == 0) && win != NULL) {SDL_DestroyWindow(win); singleWin = NULL;}
    }
#ifdef __IPHONEOS__
    updateCloseButton();
#endif
}

void SDLTerminal::setPalette(Color * p) {
    for (int i = 0; i < 16; i++) palette[i] = p[i];
}

void SDLTerminal::setCharScale(int scale) {
    if (scale < 1) scale = 1;
    {
        std::lock_guard<std::mutex> lock(locked);
        useOrigFont = scale % 2 == 1 && !config.customFontPath.empty();
        newWidth = width * charScale / scale;
        newHeight = height * charScale / scale;
        charScale = scale;
        charWidth = fontWidth * charScale;
        charHeight = fontHeight * charScale;
    }
    resize(newWidth, newHeight);
}

bool operator!=(Color lhs, Color rhs) {
    return lhs.r != rhs.r || lhs.g != rhs.g || lhs.b != rhs.b;
}

bool SDLTerminal::drawChar(unsigned char c, int x, int y, Color fg, Color bg, bool transparent) {
    SDL_Rect srcrect = getCharacterRect(c);
    SDL_Rect destrect = {
        (int)(x * charWidth * dpiScale + 2 * charScale * dpiScale), 
        (int)(y * charHeight * dpiScale + 2 * charScale * dpiScale), 
        (int)(fontWidth * charScale * dpiScale), 
        (int)(fontHeight * charScale * dpiScale)
    };
    SDL_Rect bgdestrect = destrect;
    if (config.standardsMode || config.extendMargins) {
        if (x == 0) bgdestrect.x -= (int)(2 * charScale * dpiScale);
        if (y == 0) bgdestrect.y -= (int)(2 * charScale * dpiScale);
        if (x == 0 || (unsigned)x == width - 1) bgdestrect.w += (int)(2 * charScale * dpiScale);
        if (y == 0 || (unsigned)y == height - 1) bgdestrect.h += (int)(2 * charScale * dpiScale);
        if ((unsigned)x == width - 1) bgdestrect.w += realWidth - (int)(width*charWidth*dpiScale+(4 * charScale * dpiScale));
        if ((unsigned)y == height - 1) bgdestrect.h += realHeight - (int)(height*charHeight*dpiScale+(4 * charScale * dpiScale));
    }
    if (!transparent && bg != palette[15]) {
        if (gotResizeEvent) return false;
        bg = grayscalify(bg);
        if (SDL_FillRect(surf, &bgdestrect, rgb(bg)) != 0) return false;
    }
    if (c != ' ' && c != '\0') {
        if (gotResizeEvent) return false;
        fg = grayscalify(fg);
        if (SDL_SetSurfaceColorMod(useOrigFont ? origfont : bmp, fg.r, fg.g, fg.b) != 0) return false;
        if (gotResizeEvent) return false;
        if (SDL_BlitScaled(useOrigFont ? origfont : bmp, &srcrect, surf, &destrect) != 0) return false;
    }
    return true;
}

static unsigned char circlePix[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 0, 0,
    0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255,
    0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255,
    0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255,
    0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255,
    0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

void SDLTerminal::render() {
    // copy the screen data so we can let Lua keep going without waiting for the mutex
    std::unique_ptr<vector2d<unsigned char> > newscreen;
    std::unique_ptr<vector2d<unsigned char> > newcolors;
    std::unique_ptr<vector2d<unsigned char> > newpixels;
    Color newpalette[256];
    unsigned newwidth, newheight, newcharWidth, newcharHeight, newcharScale;
    int newblinkX, newblinkY, newmode;
    bool newblink;
    unsigned char newcursorColor;
    {
        std::lock_guard<std::mutex> locked_g(locked);
        if (gotResizeEvent) {
            if (newWidth > 0 && newHeight > 0) {
                this->screen.resize(newWidth, newHeight, ' ');
                this->colors.resize(newWidth, newHeight, 0xF0);
                this->pixels.resize(newWidth * fontWidth, newHeight * fontHeight, 0x0F);
                changed = true;
            } else changed = false;
            this->width = newWidth;
            this->height = newHeight;
            gotResizeEvent = false;
        }
        if ((!changed && !shouldScreenshot && !shouldRecord) || width == 0 || height == 0) return;
        newscreen = std::make_unique<vector2d<unsigned char> >(screen);
        newcolors = std::make_unique<vector2d<unsigned char> >(colors);
        newpixels = std::make_unique<vector2d<unsigned char> >(pixels);
        memcpy(newpalette, palette, sizeof(newpalette));
        newblinkX = blinkX; newblinkY = blinkY; newmode = mode;
        newblink = blink;
        newcursorColor = cursorColor;
        newwidth = width; newheight = height; newcharWidth = charWidth; newcharHeight = charHeight; newcharScale = charScale;
        changed = false;
    }
    std::lock_guard<std::mutex> rlock(renderlock);
    int ww = 0, wh = 0;
    SDL_GetWindowSize(win, &ww, &wh);
    if (surf == NULL) surf = SDL_CreateRGBSurfaceWithFormat(0, ww, wh, 24, SDL_PIXELFORMAT_RGB888);
    if (surf == NULL) {
        fprintf(stderr, "Could not allocate rendering surface: %s\n", SDL_GetError());
        return;
    }
    SDL_Rect rect;
    if (gotResizeEvent || SDL_FillRect(surf, NULL, newmode == 0 ? rgb(newpalette[15]) : rgb(defaultPalette[15])) != 0) return;
    if (newmode != 0) {
        for (unsigned y = 0; y < newheight * newcharHeight * dpiScale; y+=newcharScale * dpiScale) {
            for (unsigned x = 0; x < newwidth * newcharWidth * dpiScale; x+=newcharScale * dpiScale) {
                unsigned char c = (*newpixels)[y / newcharScale / dpiScale][x / newcharScale / dpiScale];
                if (gotResizeEvent) return;
                if (SDL_FillRect(surf, setRect(&rect, (int)(x + 2 * newcharScale * dpiScale),
                                               (int)(y + 2 * newcharScale * dpiScale),
                                               (int)newcharScale * dpiScale,
                                               (int)newcharScale * dpiScale),
                                 rgb(newpalette[(int)c])) != 0) return;
            }
        }
    } else {
        for (unsigned y = 0; y < newheight; y++) for (unsigned x = 0; x < newwidth; x++) 
            if (gotResizeEvent || !drawChar((*newscreen)[y][x], (int)x, (int)y, newpalette[(*newcolors)[y][x] & 0x0F], newpalette[(*newcolors)[y][x] >> 4])) return;
        if (gotResizeEvent) return;
        if (newblink && newblinkX >= 0 && newblinkY >= 0 && (unsigned)newblinkX < newwidth && (unsigned)newblinkY < newheight) if (!drawChar('_', newblinkX, newblinkY, newpalette[newcursorColor], newpalette[(*newcolors)[newblinkY][newblinkX] >> 4], true)) return;
    }
    currentFPS++;
    if (lastSecond != time(0)) {
        lastSecond = time(0);
        lastFPS = currentFPS;
        currentFPS = 0;
    }
    if (config.showFPS) {
        // later?
    }
    if (shouldScreenshot && !screenshotPath.empty()) {
        shouldScreenshot = false;
        if (gotResizeEvent) return;
        SDL_Surface * temp = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGB24, 0);
        if (screenshotPath == "clipboard") {
            copyImage(temp, win);
        } else {
#ifndef NO_WEBP
            if (config.useWebP) {
                uint8_t * data = NULL;
                size_t size = WebPEncodeLosslessRGB((uint8_t*)temp->pixels, temp->w, temp->h, temp->pitch, &data);
                if (size) {
                    std::ofstream out(screenshotPath, std::ios::binary);
                    out.write((char*)data, size);
                    out.close();
                    WebPFree(data);
                }
            } else {
#endif
#ifndef NO_PNG
                png::solid_pixel_buffer<png::rgb_pixel> pixbuf(temp->w, temp->h);
                for (int i = 0; i < temp->h; i++)
                    memcpy((void*)&pixbuf.get_bytes()[i * temp->w * 3], (char*)temp->pixels + (i * temp->pitch), temp->w * 3);
                png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel> > img(temp->w, temp->h);
                img.set_pixbuf(pixbuf);
                std::ofstream out(screenshotPath, std::ios::binary);
                img.write_stream(out);
                out.close();
#else
                SDL_SaveBMP(temp, screenshotPath.c_str());
#endif
#ifndef NO_WEBP
            }
#endif
        }
        SDL_FreeSurface(temp);
#ifdef __EMSCRIPTEN__
        queueTask([](void*)->void*{emsyncfs(); return NULL;}, NULL, true);
#endif
    }
    if (shouldRecord) {
        if (recordedFrames >= config.maxRecordingTime * config.recordingFPS) stopRecording();
        else if (--frameWait < 1) {
            std::lock_guard<std::mutex> recorderlock(recorderMutex);
#ifndef NO_WEBP
            if (isRecordingWebP) {
                if (recorderHandle == NULL) {
                    WebPAnimEncoderOptions enc_options;
                    WebPAnimEncoderOptionsInit(&enc_options);
                    recorderHandle = WebPAnimEncoderNew(surf->w, surf->h, &enc_options);
                }
                SDL_Surface * temp = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_BGRA32, 0);
                WebPConfig config;
                WebPConfigInit(&config);
                config.lossless = true;
                WebPPicture frame;
                WebPPictureInit(&frame);
                frame.width = temp->w;
                frame.height = temp->h;
                frame.use_argb = true;
                frame.argb = (uint32_t*)temp->pixels;
                frame.argb_stride = temp->pitch / 4;
                WebPAnimEncoderAdd((WebPAnimEncoder*)recorderHandle, &frame, (1000 / ::config.recordingFPS) * recordedFrames, &config);
                SDL_FreeSurface(temp);
            } else {
#endif
                if (recorderHandle == NULL) {
                    GifWriter * g = new GifWriter;
#ifdef _WIN32
                    g->f = _wfopen(recordingPath.native().c_str(), L"wb");
#else
                    g->f = fopen(recordingPath.native().c_str(), "wb");
#endif
                    GifBegin(g, NULL, surf->w, surf->h, 100 / config.recordingFPS);
                    recorderHandle = g;
                }
                SDL_Surface * temp = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGBA32, 0);
                uint32_t pal[256];
                for (int i = 0; i < 256; i++) pal[i] = newpalette[i].r | (newpalette[i].g << 8) | (newpalette[i].b << 16);
                GifWriteFrame((GifWriter*)recorderHandle, (uint8_t*)temp->pixels, temp->w, temp->h, 100 / config.recordingFPS, newmode == 2 ? 8 : 5, false, pal);
                SDL_FreeSurface(temp);
#ifndef NO_WEBP
            }
#endif
            recordedFrames++;
            frameWait = ::config.clockSpeed / ::config.recordingFPS;
            if (gotResizeEvent) return;
        }
        SDL_Surface* circle = SDL_CreateRGBSurfaceWithFormatFrom(circlePix, 10, 10, 32, 40, SDL_PIXELFORMAT_BGRA32);
        if (circle == NULL) { fprintf(stderr, "Error creating circle: %s\n", SDL_GetError()); }
        if (gotResizeEvent) return;
        if (SDL_BlitSurface(circle, NULL, surf, setRect(&rect, (int)(newwidth * newcharWidth * dpiScale + 2 * newcharScale * dpiScale) - 10, (int)(2 * newcharScale * dpiScale), 10, 10)) != 0) return;
        SDL_FreeSurface(circle);
    }
}

void SDLTerminal::getMouse(int *x, int *y) {
    SDL_GetMouseState(x, y);
}

SDL_Rect SDLTerminal::getCharacterRect(unsigned char c) {
    SDL_Rect retval;
    if (useOrigFont) {
        retval.w = (int)(fontWidth);
        retval.h = (int)(fontHeight);
        retval.x = (int)(((fontWidth + 2))*(c & 0x0F)+1);
        retval.y = (int)(((fontHeight + 2))*(c >> 4)+1);
    } else {
        retval.w = (int)(fontWidth * 2/fontScale);
        retval.h = (int)(fontHeight * 2/fontScale);
        retval.x = (int)(((fontWidth + 2) * 2/fontScale)*(c & 0x0F)+2/fontScale);
        retval.y = (int)(((fontHeight + 2) * 2/fontScale)*(c >> 4)+2/fontScale);
    }
    return retval;
}

bool SDLTerminal::resize(unsigned w, unsigned h) {
    if (this->shouldRecord) return false;
    {
        std::lock_guard<std::mutex> lock2(locked);
        newWidth = w;
        newHeight = h;
#ifndef __IPHONEOS__
        if (config.snapToSize && !fullscreen && !(SDL_GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED)) queueTask([this, w, h](void*)->void*{SDL_SetWindowSize((SDL_Window*)win, (int)(w*charWidth*dpiScale+(4 * charScale * dpiScale)), (int)(h*charHeight*dpiScale+(4 * charScale * dpiScale))); return NULL;}, NULL);
#endif
        SDL_GetWindowSize(win, &realWidth, &realHeight);
        gotResizeEvent = (newWidth != width || newHeight != height);
        if (!gotResizeEvent) return false;
        {
            std::lock_guard<std::mutex> lock(renderlock);
            SDL_FreeSurface(surf);
            surf = NULL;
            changed = true;
        }
    }
    while (gotResizeEvent && (!singleWindowMode || *renderTarget == this)) std::this_thread::yield(); // this should probably be a condition variable
    return true;
}

bool SDLTerminal::resizeWholeWindow(int w, int h) {
    const bool r = resize(w, h);
    if (!r) return r;
    queueTask([this](void*)->void*{SDL_SetWindowSize(win, (int)(width*charWidth*dpiScale+(4 * charScale * dpiScale)), (int)(height*charHeight*dpiScale+(4 * charScale*dpiScale))); return NULL;}, NULL);
    return r;
}

void SDLTerminal::screenshot(std::string path) {
    if (!path.empty()) screenshotPath = path_t(path, path == "clipboard" ? path_t::format::generic_format : path_t::format::auto_format);
    else {
        time_t now = time(0);
        struct tm * nowt = localtime(&now);
        screenshotPath = getBasePath() / "screenshots";
        std::error_code e;
        fs::create_directories(screenshotPath, e);
        if (e) return;
        char tstr[24];
        strftime(tstr, 24, "%F_%H.%M.%S", nowt);
        tstr[23] = '\0';
#ifndef NO_WEBP
        if (config.useWebP) screenshotPath /= std::string(tstr) + ".webp"; else
#endif
#ifdef NO_PNG
        screenshotPath /= std::string(tstr) + ".bmp";
#else
        screenshotPath /= std::string(tstr) + ".png";
#endif
    }
    shouldScreenshot = true;
}

void SDLTerminal::record(std::string path) {
    shouldRecord = true;
    recordedFrames = 0;
    frameWait = 0;
    if (!path.empty()) recordingPath = path;
    else {
        time_t now = time(0);
        struct tm * nowt = localtime(&now);
        recordingPath = getBasePath() / "screenshots";
        std::error_code e;
        fs::create_directories(recordingPath, e);
        if (e) return;
        char tstr[20];
        strftime(tstr, 20, "%F_%H.%M.%S", nowt);
        isRecordingWebP = config.useWebP;
#ifndef NO_WEBP
        if (isRecordingWebP) recordingPath /= std::string(tstr) + ".webp"; else
#endif
        recordingPath /= std::string(tstr) + ".gif";
    }
    recorderHandle = NULL;
    changed = true;
}

void SDLTerminal::stopRecording() {
    shouldRecord = false;
    std::lock_guard<std::mutex> lock(recorderMutex);
    if (recorderHandle == NULL) return;
#ifndef NO_WEBP
    if (isRecordingWebP) {
        WebPAnimEncoderAdd((WebPAnimEncoder*)recorderHandle, NULL, (1000 / ::config.recordingFPS) * recordedFrames, NULL);
        WebPData webp_data;
        WebPDataInit(&webp_data);
        WebPAnimEncoderAssemble((WebPAnimEncoder*)recorderHandle, &webp_data);
        std::ofstream out(recordingPath.c_str(), std::ios::binary);
        out.write((char*)webp_data.bytes, webp_data.size);
        out.close();
        WebPAnimEncoderDelete((WebPAnimEncoder*)recorderHandle);
    } else {
#endif
        GifEnd((GifWriter*)recorderHandle);
#ifndef NO_WEBP
    }
#endif
    recorderHandle = NULL;
#ifdef __EMSCRIPTEN__
    queueTask([](void*)->void*{emsyncfs(); return NULL;}, NULL, true);
#endif
    changed = true;
}

void SDLTerminal::showMessage(Uint32 flags, const char * title, const char * message) {SDL_ShowSimpleMessageBox(flags, title, message, win);}

void SDLTerminal::toggleFullscreen() {
    fullscreen = !fullscreen;
    if (fullscreen) queueTask([ ](void* param)->void*{SDL_SetWindowFullscreen((SDL_Window*)param, SDL_WINDOW_FULLSCREEN_DESKTOP); return NULL;}, win);
    else queueTask([ ](void* param)->void*{SDL_SetWindowFullscreen((SDL_Window*)param, 0); return NULL;}, win);
    if (!fullscreen) {
        SDL_Event e;
        int w, h;
        SDL_GetWindowSize(win, &w, &h);
        e.type = SDL_WINDOWEVENT;
        e.window.windowID = SDL_GetWindowID(win);
        e.window.event = SDL_WINDOWEVENT_RESIZED;
        e.window.data1 = w;
        e.window.data2 = h;
        SDL_PushEvent(&e);
    }
}

void SDLTerminal::setLabel(std::string label) {
    title = label;
    queueTask([label](void*win)->void*{SDL_SetWindowTitle((SDL_Window*)win, label.c_str()); return NULL;}, win, true);
}

void SDLTerminal::onActivate() {
    queueTask([this](void*win)->void*{SDL_SetWindowTitle((SDL_Window*)win, title.c_str()); return NULL;}, win, true);
}

void SDLTerminal::init() {
    SDL_SetHint(SDL_HINT_RENDER_DIRECT3D_THREADSAFE, "1");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    //SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
#ifdef __EMSCRIPTEN__
    SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 8)
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
#if SDL_VERSION_ATLEAST(2, 0, 10)
    //SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        throw std::runtime_error("Could not initialize SDL: " + std::string(SDL_GetError()));
    }
    SDL_EventState(SDL_DROPTEXT, SDL_FALSE); // prevent memory leaks from dropping text (not supported)
    task_event_type = SDL_RegisterEvents(2);
    render_event_type = task_event_type + 1;
    renderThread = new std::thread(termRenderLoop);
    setThreadName(*renderThread, "Render Thread");
    SDL_Surface* old_bmp;
    std::string bmp_path = "built-in file";
#ifndef STANDALONE_ROM
    if (config.customFontPath == "hdfont") {
        bmp_path = (getROMPath() / "hdfont.bmp").string();
        fontScale = 1;
    } else 
#endif
    if (!config.customFontPath.empty()) {
        bmp_path = config.customFontPath;
        fontScale = config.customFontScale;
    }
    if (config.customFontPath.empty()) 
        old_bmp = SDL_CreateRGBSurfaceWithFormatFrom((void*)font_image.pixel_data, (int)font_image.width, (int)font_image.height, (int)font_image.bytes_per_pixel * 8, (int)font_image.bytes_per_pixel * (int)font_image.width, SDL_PIXELFORMAT_RGB565);
    else old_bmp = SDL_LoadBMP(bmp_path.c_str());
    if (old_bmp == (SDL_Surface*)0) {
        throw std::runtime_error("Failed to load font: " + std::string(SDL_GetError()));
    }
    bmp = SDL_ConvertSurfaceFormat(old_bmp, SDL_PIXELFORMAT_RGBA32, 0);
    if (bmp == (SDL_Surface*)0) {
        SDL_FreeSurface(old_bmp);
        throw std::runtime_error("Failed to convert font: " + std::string(SDL_GetError()));
    }
    SDL_FreeSurface(old_bmp);
    SDL_SetColorKey(bmp, SDL_TRUE, SDL_MapRGB(bmp->format, 0, 0, 0));
    if (config.customFontPath.empty()) origfont = bmp;
    else {
        old_bmp = SDL_CreateRGBSurfaceWithFormatFrom((void*)font_image.pixel_data, (int)font_image.width, (int)font_image.height, (int)font_image.bytes_per_pixel * 8, (int)font_image.bytes_per_pixel * (int)font_image.width, SDL_PIXELFORMAT_RGB565);
        origfont = SDL_ConvertSurfaceFormat(old_bmp, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(old_bmp);
        SDL_SetColorKey(origfont, SDL_TRUE, SDL_MapRGB(origfont->format, 0, 0, 0));
    }
}

void SDLTerminal::quit() {
    renderThread->join();
    delete renderThread;
    SDL_FreeSurface(bmp);
    if (bmp != origfont) SDL_FreeSurface(origfont);
    SDL_Quit();
}

static SDL_TouchID touchDevice = -1;

bool SDLTerminal::pollEvents() {
    SDL_Event e;
#ifdef __EMSCRIPTEN__
    if (SDL_PollEvent(&e)) {
#else
    if (SDL_WaitEvent(&e)) {
#endif
        if (singleWindowMode) {
            // Transform window IDs in single window mode
            // All events with windowID have it in the same place (except drop & touch)! We don't have to dance around checking each individual struct in the union.
            // (TODO: Fix the **NASTY** code below to do what we do here.)
            switch (e.type) {
            case SDL_WINDOWEVENT: case SDL_KEYDOWN: case SDL_KEYUP: case SDL_TEXTINPUT: case SDL_TEXTEDITING:
            case SDL_MOUSEMOTION: case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: case SDL_MOUSEWHEEL: case SDL_USEREVENT:
                e.window.windowID = (*renderTarget)->id;
                break;
#if SDL_VERSION_ATLEAST(2, 0, 12)
            case SDL_FINGERUP: case SDL_FINGERDOWN: case SDL_FINGERMOTION:
                e.tfinger.windowID = (*renderTarget)->id;
                break;
#endif
            case SDL_DROPBEGIN: case SDL_DROPCOMPLETE: case SDL_DROPFILE: case SDL_DROPTEXT:
                e.drop.windowID = (*renderTarget)->id;
                break;
            }
        }
        if (e.type == task_event_type) pumpTaskQueue();
        else if (e.type == render_event_type) {
            if (singleWindowMode) {
                SDLTerminal * sdlterm = dynamic_cast<SDLTerminal*>(*renderTarget);
                if (sdlterm != NULL) {
                    std::lock_guard<std::mutex> lock(sdlterm->renderlock);
                    if (sdlterm->surf != NULL && !(sdlterm->width == 0 || sdlterm->height == 0)) {
                        SDL_BlitSurface(sdlterm->surf, NULL, SDL_GetWindowSurface(sdlterm->win), NULL);
                        SDL_UpdateWindowSurface(sdlterm->win);
                    }
                }
            } else {
                for (Terminal* term : renderTargets) {
                    SDLTerminal * sdlterm = dynamic_cast<SDLTerminal*>(term);
                    if (sdlterm != NULL) {
                        std::lock_guard<std::mutex> lock(sdlterm->renderlock);
                        if (sdlterm->surf != NULL && !(sdlterm->width == 0 || sdlterm->height == 0)) {
                            SDL_BlitSurface(sdlterm->surf, NULL, SDL_GetWindowSurface(sdlterm->win), NULL);
                            SDL_UpdateWindowSurface(sdlterm->win);
                        }
                    }
                }
            }
        } else if (singleWindowMode && e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
            // Send the resize event to ALL windows, including monitors
            for (Computer * c : *computers) {
                e.window.windowID = c->term->id;
                std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
                c->termEventQueue.push(e);
                c->event_lock.notify_all();
                std::lock_guard<std::mutex> lock2(c->peripherals_mutex);
                for (const std::pair<std::string, peripheral*> p : c->peripherals) {
                    monitor * m = dynamic_cast<monitor*>(p.second);
                    if (m != NULL) {
                        e.window.windowID = m->term->id;
                        c->termEventQueue.push(e);
                        c->event_lock.notify_all();
                    }
                }
            }
        } else {
            if (rawClient) {
                sendRawEvent(e);
            } else if (!computers.locked()) {
                LockGuard lockc(computers);
                bool stop = false;
                if (eventHandlers.find((SDL_EventType)e.type) != eventHandlers.end()) {
                    Computer * comp = NULL;
                    Terminal * term = NULL;
                    for (Computer * c : *computers) {
                        if (c->term->id == lastWindow) {
                            comp = c;
                            term = c->term;
                            break;
                        } else {
                            monitor * m = findMonitorFromWindowID(c, lastWindow, NULL);
                            if (m != NULL) {
                                comp = c;
                                term = m->term;
                                break;
                            }
                        }
                    }
                    for (const auto& h : Range<std::unordered_multimap<SDL_EventType, std::pair<sdl_event_handler, void*> >::iterator>(eventHandlers.equal_range((SDL_EventType)e.type))) {
                        stop = h.second.first(&e, comp, term, h.second.second) || stop;
                    }
                }
                if (!stop) {
                    for (Computer * c : *computers) {
                        if (((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && checkWindowID(c, e.key.windowID)) ||
                            ((e.type == SDL_DROPFILE || e.type == SDL_DROPTEXT || e.type == SDL_DROPBEGIN || e.type == SDL_DROPCOMPLETE) && checkWindowID(c, e.drop.windowID)) ||
                            ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && checkWindowID(c, e.button.windowID) && (touchDevice == -1 || SDL_GetNumTouchFingers(touchDevice) < 2)) ||
                            (e.type == SDL_MOUSEMOTION && checkWindowID(c, e.motion.windowID) && (touchDevice == -1 || SDL_GetNumTouchFingers(touchDevice) < 2)) ||
                            (e.type == SDL_MOUSEWHEEL && checkWindowID(c, e.wheel.windowID)) ||
                            (e.type == SDL_TEXTINPUT && checkWindowID(c, e.text.windowID)) ||
#if SDL_VERSION_ATLEAST(2, 0, 12)
                            ((e.type == SDL_FINGERDOWN || e.type == SDL_FINGERUP || e.type == SDL_FINGERMOTION) && checkWindowID(c, e.tfinger.windowID)) ||
#endif
                            (e.type == SDL_WINDOWEVENT && checkWindowID(c, e.window.windowID)) ||
                            e.type == SDL_QUIT) {
                            std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
                            c->termEventQueue.push(e);
                            c->event_lock.notify_all();
                            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE && e.window.windowID == c->term->id) {
                                if (c->requestedExit && c->L) {
                                    SDL_MessageBoxData msg;
                                    msg.flags = SDL_MESSAGEBOX_INFORMATION;
                                    msg.title = "Computer Unresponsive";
                                    msg.message = "The computer appears to be unresponsive. Would you like to force the computer to shut down? All unsaved data will be lost.";
                                    SDL_MessageBoxButtonData buttons[2] = {
                                        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Cancel"},
                                        {0, 1, "Shut Down"}
                                    };
                                    msg.buttons = buttons;
                                    msg.numbuttons = 2;
                                    msg.window = ((SDLTerminal*)c->term)->win;
                                    int id = 0;
                                    SDL_ShowMessageBox(&msg, &id);
                                    if (id == 1 && c->L) {
                                        // Forcefully halt the Lua state
                                        c->running = 0;
                                        lua_halt(c->L);
                                    }
                                } else c->requestedExit = true;
                            }
                        }
                    }
                }
                if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) lastWindow = e.window.windowID;
                else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_LEFT && (e.key.keysym.mod & KMOD_ALT) && (e.key.keysym.mod & KMOD_SYSMOD) && !(e.key.keysym.mod & KMOD_SHIFT)) previousRenderTarget();
                else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RIGHT && (e.key.keysym.mod & KMOD_ALT) && (e.key.keysym.mod & KMOD_SYSMOD) && !(e.key.keysym.mod & KMOD_SHIFT)) nextRenderTarget();
#ifdef __IPHONEOS__
                else if (e.type == SDL_FINGERUP || e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION) touchDevice = e.tfinger.touchId;
#else
                /*else if (e.type == SDL_MULTIGESTURE && e.mgesture.numFingers == 2) {
                    if (e.mgesture.dDist < -0.001 && !SDL_IsTextInputActive()) SDL_StartTextInput();
                    else if (e.mgesture.dDist > 0.001 && SDL_IsTextInputActive()) SDL_StopTextInput();
                }*/
#endif
                for (Terminal * t : orphanedTerminals) {
                    if ((e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE && e.window.windowID == t->id) || e.type == SDL_QUIT) {
                        orphanedTerminals.erase(t);
                        t->factory->deleteTerminal(t);
                        break;
                    }
                }
            }
            if (e.type == SDL_QUIT) return true;
        }
    }
    return false;
}
