/*
 * terminal/SDLTerminal.cpp
 * CraftOS-PC 2
 * 
 * This file implements the SDLTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include <sstream>
#include <configuration.hpp>
#include "RawTerminal.hpp"
#include "SDLTerminal.hpp"
#include "../gif.hpp"
#include "../main.hpp"
#include "../runtime.hpp"
#include "../termsupport.hpp"
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
std::unordered_multimap<SDL_EventType, std::pair<sdl_event_handler, void*> > SDLTerminal::eventHandlers;
/* export */ std::list<Terminal*> renderTargets;
/* export */ std::mutex renderTargetsLock;
#ifdef __EMSCRIPTEN__
/* export */ std::list<Terminal*>::iterator renderTarget = Terminal::renderTargets.end();
SDL_Window *SDLTerminal::win = NULL;
static int nextWindowID = 1;

extern "C" {
    void EMSCRIPTEN_KEEPALIVE nextRenderTarget() {
        if (++Terminal::renderTarget == Terminal::renderTargets.end()) Terminal::renderTarget = Terminal::renderTargets.begin();
        (*Terminal::renderTarget)->changed = true;
    }

    void EMSCRIPTEN_KEEPALIVE previousRenderTarget() {
        if (Terminal::renderTarget == Terminal::renderTargets.begin()) Terminal::renderTarget = Terminal::renderTargets.end();
        Terminal::renderTarget--;
        (*Terminal::renderTarget)->changed = true;
    }

    bool EMSCRIPTEN_KEEPALIVE selectRenderTarget(int id) {
        for (Terminal::renderTarget = Terminal::renderTargets.begin(); Terminal::renderTarget != Terminal::renderTargets.end(); Terminal::renderTarget++) if ((*Terminal::renderTarget)->id == id) break;
        (*Terminal::renderTarget)->changed = true;
        return Terminal::renderTarget != Terminal::renderTargets.end();
    }

    const char * EMSCRIPTEN_KEEPALIVE getRenderTargetName() {
        return (*Terminal::renderTarget)->title.c_str();
    }

    extern void syncfs();
}

void onWindowCreate(int id, const char * title) {EM_ASM({if (Module.windowEventListener !== undefined) Module.windowEventListener.onWindowCreate($0, $1);}, id, title);}
void onWindowDestroy(int id) {EM_ASM({if (Module.windowEventListener !== undefined) Module.windowEventListener.onWindowDestroy($0);}, id);}
#endif

SDLTerminal::SDLTerminal(std::string title): Terminal(config.defaultWidth, config.defaultHeight) {
    this->title = title;
#ifdef __EMSCRIPTEN__
    dpiScale = emscripten_get_device_pixel_ratio();
#endif
    if (config.customFontPath == "hdfont") {
        fontScale = 1;
        charScale = 1;
        charWidth = fontWidth * 2/fontScale * charScale;
        charHeight = fontHeight * 2/fontScale * charScale;
    } else if (!config.customFontPath.empty()) {
        fontScale = config.customFontScale;
        charScale = fontScale;
        charWidth = fontWidth * 2/fontScale * charScale;
        charHeight = fontHeight * 2/fontScale * charScale;
    }
    if (config.customCharScale > 0) {
        charScale = config.customCharScale;
        charWidth = fontWidth * 2/fontScale * charScale;
        charHeight = fontHeight * 2/fontScale * charScale;
    }
#if defined(__EMSCRIPTEN__) && !defined(NO_EMSCRIPTEN_HIDPI)
    if (win == NULL)
#endif
    win = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, (int)(width*charWidth*dpiScale+(4 * charScale * (2 / fontScale)*dpiScale)), (int)(height*charHeight*dpiScale+(4 * charScale * (2 / fontScale)*dpiScale)), SDL_WINDOW_SHOWN | 
#if !(defined(__EMSCRIPTEN__) && defined(NO_EMSCRIPTEN_HIDPI))
    SDL_WINDOW_ALLOW_HIGHDPI |
#endif
    SDL_WINDOW_RESIZABLE | SDL_WINDOW_INPUT_FOCUS);
    if (win == (SDL_Window*)0) {
        overridden = true;
        throw window_exception("Failed to create window: " + std::string(SDL_GetError()));
    }
#ifndef __EMSCRIPTEN__
    id = SDL_GetWindowID(win);
#else
    id = nextWindowID++;
    onWindowCreate(id, title.c_str());
#endif
    lastWindow = id;
#if !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
    SDL_Surface* icon = SDL_CreateRGBSurfaceFrom(favicon.pixel_data, (int)favicon.width, (int)favicon.height, (int)favicon.bytes_per_pixel * 8, (int)favicon.width * (int)favicon.bytes_per_pixel, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    SDL_SetWindowIcon(win, icon);
    SDL_FreeSurface(icon);
#endif
    SDL_Surface* old_bmp;
    if (config.customFontPath.empty()) 
        old_bmp = SDL_CreateRGBSurfaceWithFormatFrom((void*)font_image.pixel_data, (int)font_image.width, (int)font_image.height, (int)font_image.bytes_per_pixel * 8, (int)font_image.bytes_per_pixel * (int)font_image.width, SDL_PIXELFORMAT_RGB565);
#ifndef STANDALONE_ROM
    else if (config.customFontPath == "hdfont") old_bmp = SDL_LoadBMP(astr(getROMPath() + WS("/hdfont.bmp")).c_str());
#endif
    else old_bmp = SDL_LoadBMP(config.customFontPath.c_str());
    if (old_bmp == (SDL_Surface*)0) {
#ifndef __EMSCRIPTEN__
        SDL_DestroyWindow(win);
#endif
        overridden = true;
        throw window_exception("Failed to load font: " + std::string(SDL_GetError()));
    }
    bmp = SDL_ConvertSurfaceFormat(old_bmp, SDL_PIXELFORMAT_RGBA32, 0);
    if (bmp == (SDL_Surface*)0) {
#ifndef __EMSCRIPTEN__
        SDL_DestroyWindow(win);
#endif
        overridden = true;
        throw window_exception("Failed to convert font: " + std::string(SDL_GetError()));
    }
    SDL_FreeSurface(old_bmp);
    SDL_SetColorKey(bmp, SDL_TRUE, SDL_MapRGB(bmp->format, 0, 0, 0));
    renderTargets.push_back(this);
#ifdef __EMSCRIPTEN__
    if (renderTargets.size() == 1) renderTarget = renderTargets.begin();
#endif
}

SDLTerminal::~SDLTerminal() {
    if (shouldRecord) stopRecording();
#ifdef __EMSCRIPTEN__
    onWindowDestroy(id);
#endif
    renderTargetsLock.lock();
    std::lock_guard<std::mutex> locked_g(renderlock);
    for (auto it = renderTargets.begin(); it != renderTargets.end(); ++it) {
        if (*it == this)
            it = renderTargets.erase(it);
        if (it == renderTargets.end()) break;
    }
    renderTargetsLock.unlock();
    if (!overridden) {
        if (surf != NULL) SDL_FreeSurface(surf);
        SDL_FreeSurface(bmp);
#ifndef __EMSCRIPTEN__
        SDL_DestroyWindow(win);
#endif
    }
}

void SDLTerminal::setPalette(Color * p) {
    for (int i = 0; i < 16; i++) palette[i] = p[i];
}

void SDLTerminal::setCharScale(int scale) {
    if (scale < 1) scale = 1;
    charScale = scale;
    charWidth = fontWidth * (2/fontScale) * charScale;
    charHeight = fontHeight * (2/fontScale) * charScale;
    SDL_SetWindowSize(win, (int)(width*charWidth+(4 * charScale)), (int)(height*charHeight+(4 * charScale)));
}

bool operator!=(Color lhs, Color rhs) {
    return lhs.r != rhs.r || lhs.g != rhs.g || lhs.b != rhs.b;
}

bool SDLTerminal::drawChar(unsigned char c, int x, int y, Color fg, Color bg, bool transparent) {
    SDL_Rect srcrect = getCharacterRect(c);
    SDL_Rect destrect = {
        (int)(x * charWidth * dpiScale + 2 * charScale * 2/fontScale * dpiScale), 
        (int)(y * charHeight * dpiScale + 2 * charScale * 2/fontScale * dpiScale), 
        (int)(fontWidth * 2/fontScale * charScale * dpiScale), 
        (int)(fontHeight * 2/fontScale * charScale * dpiScale)
    };
    SDL_Rect bgdestrect = destrect;
    if (config.standardsMode) {
        if (x == 0) bgdestrect.x -= (int)(2 * charScale * 2 / fontScale * dpiScale);
        if (y == 0) bgdestrect.y -= (int)(2 * charScale * 2 / fontScale * dpiScale);
        if (x == 0 || (unsigned)x == width - 1) bgdestrect.w += (int)(2 * charScale * 2 / fontScale * dpiScale);
        if (y == 0 || (unsigned)y == height - 1) bgdestrect.h += (int)(2 * charScale * 2 / fontScale * dpiScale);
    }
    if (!transparent && bg != palette[15]) {
        if (gotResizeEvent) return false;
        bg = grayscalify(bg);
        if (SDL_FillRect(surf, &bgdestrect, rgb(bg)) != 0) return false;
    }
    if (c != ' ' && c != '\0') {
        if (gotResizeEvent) return false;
        fg = grayscalify(fg);
        if (SDL_SetSurfaceColorMod(bmp, fg.r, fg.g, fg.b) != 0) return false;
        if (gotResizeEvent) return false;
        if (SDL_BlitScaled(bmp, &srcrect, surf, &destrect) != 0) return false;
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
    unsigned newwidth, newheight, newcharWidth, newcharHeight, newfontScale, newcharScale;
    int newblinkX, newblinkY, newmode;
    bool newblink;
    unsigned char newcursorColor;
    {
        std::lock_guard<std::mutex> locked_g(locked);
        if (gotResizeEvent) {
            gotResizeEvent = false;
            this->screen.resize(newWidth, newHeight, ' ');
            this->colors.resize(newWidth, newHeight, 0xF0);
            this->pixels.resize(newWidth * fontWidth, newHeight * fontHeight, 0x0F);
            this->pixelBuffer.resize(newWidth * fontWidth, newHeight * fontHeight, 0x0F);
            this->width = newWidth;
            this->height = newHeight;
            changed = true;
        }
        if (!changed && !shouldScreenshot && !shouldRecord) return;
        newscreen = std::make_unique<vector2d<unsigned char> >(screen);
        newcolors = std::make_unique<vector2d<unsigned char> >(colors);
        newpixels = std::make_unique<vector2d<unsigned char> >(pixels);
        memcpy(newpalette, palette, sizeof(newpalette));
        newblinkX = blinkX; newblinkY = blinkY; newmode = mode;
        newblink = blink;
        newcursorColor = cursorColor;
        newwidth = width; newheight = height; newcharWidth = charWidth; newcharHeight = charHeight; newfontScale = fontScale; newcharScale = charScale;
        changed = false;
    }
    std::lock_guard<std::mutex> rlock(renderlock);
    int ww = 0, wh = 0;
    SDL_GetWindowSize(win, &ww, &wh);
    if (surf != NULL) SDL_FreeSurface(surf);
    surf = SDL_CreateRGBSurfaceWithFormat(0, ww, wh, 24, SDL_PIXELFORMAT_RGB888);
    if (surf == NULL) {
        fprintf(stderr, "Could not allocate rendering surface: %s\n", SDL_GetError());
        return;
    }
    SDL_Rect rect;
    if (gotResizeEvent || SDL_FillRect(surf, NULL, newmode == 0 ? rgb(newpalette[15]) : rgb(defaultPalette[15])) != 0) return;
    if (newmode != 0) {
        for (unsigned y = 0; y < newheight * newcharHeight; y+=(2/ newfontScale)* newcharScale) {
            for (unsigned x = 0; x < newwidth * newcharWidth; x+=(2/ newfontScale)* newcharScale) {
                unsigned char c = (*newpixels)[y / (2/newfontScale) / newcharScale][x / (2/ newfontScale) / newcharScale];
                if (gotResizeEvent) return;
                if (SDL_FillRect(surf, setRect(&rect, (int)(x + (2 * (2 / newfontScale) * newcharScale)),
                                               (int)(y + (2 * (2 / newfontScale) * newcharScale)),
                                               (int)((2 / newfontScale) * newcharScale),
                                               (int)((2 / newfontScale) * newcharScale)),
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
#ifdef PNGPP_PNG_HPP_INCLUDED
        SDL_Surface * temp = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGB24, 0);
        if (screenshotPath == WS("clipboard")) {
            copyImage(temp);
        } else {
            png::solid_pixel_buffer<png::rgb_pixel> pixbuf(temp->w, temp->h);
            for (int i = 0; i < temp->h; i++)
                memcpy((void*)&pixbuf.get_bytes()[i * temp->w * 3], (char*)temp->pixels + (i * temp->pitch), temp->w * 3);
            png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel> > img(temp->w, temp->h);
            img.set_pixbuf(pixbuf);
            std::ofstream out(screenshotPath, std::ios::binary);
            img.write_stream(out);
            out.close();
        }
        SDL_FreeSurface(temp);
#else
        SDL_Surface *conv = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_RGB888, 0);
        SDL_SaveBMP(conv, screenshotPath.c_str());
        SDL_FreeSurface(conv);
#endif
#ifdef __EMSCRIPTEN__
        queueTask([](void*)->void*{syncfs(); return NULL;}, NULL, true);
#endif
    }
    if (shouldRecord) {
        if (recordedFrames >= config.maxRecordingTime * config.recordingFPS) stopRecording();
        else if (--frameWait < 1) {
            recorderMutex.lock();
            uint32_t uw = static_cast<uint32_t>(surf->w), uh = static_cast<uint32_t>(surf->h);
            std::string rle = std::string((char*)&uw, 4) + std::string((char*)&uh, 4);
            SDL_Surface * temp = SDL_ConvertSurfaceFormat(surf, SDL_PIXELFORMAT_ABGR8888, 0);
            uint32_t * px = ((uint32_t*)temp->pixels);
            uint32_t data = px[0] & 0xFFFFFF;
            for (int y = 0; y < surf->h; y++) {
                for (int x = 0; x < surf->w; x++) {
                    uint32_t p = px[y*surf->w+x];
                    if ((p & 0xFFFFFF) != (data & 0xFFFFFF) || (data & 0xFF000000) == 0xFF000000) {
                        rle += std::string((char*)&data, 4);
                        data = p & 0xFFFFFF;
                    } else data += 0x1000000;
                }
            }
            rle += std::string((char*)&data, 4);
            SDL_FreeSurface(temp);
            recording.push_back(rle);
            recordedFrames++;
            frameWait = config.clockSpeed / config.recordingFPS;
            recorderMutex.unlock();
            if (gotResizeEvent) return;
        }
        SDL_Surface* circle = SDL_CreateRGBSurfaceWithFormatFrom(circlePix, 10, 10, 32, 40, SDL_PIXELFORMAT_BGRA32);
        if (circle == NULL) { fprintf(stderr, "Error creating circle: %s\n", SDL_GetError()); }
        if (gotResizeEvent) return;
        if (SDL_BlitSurface(circle, NULL, surf, setRect(&rect, (int)(newwidth * newcharWidth * dpiScale + 2 * newcharScale * (2/ newfontScale) * dpiScale) - 10, (int)(2 * newcharScale * (2/ newfontScale) * dpiScale), 10, 10)) != 0) return;
        SDL_FreeSurface(circle);
    }
}

void SDLTerminal::getMouse(int *x, int *y) {
    SDL_GetMouseState(x, y);
}

SDL_Rect SDLTerminal::getCharacterRect(unsigned char c) {
    SDL_Rect retval;
    retval.w = (int)(fontWidth * 2/fontScale);
    retval.h = (int)(fontHeight * 2/fontScale);
    retval.x = (int)(((fontWidth + 2) * 2/fontScale)*(c & 0x0F)+2/fontScale);
    retval.y = (int)(((fontHeight + 2) * 2/fontScale)*(c >> 4)+2/fontScale);
    return retval;
}

bool SDLTerminal::resize(unsigned w, unsigned h) {
    newWidth = w;
    newHeight = h;
    gotResizeEvent = (newWidth != width || newHeight != height);
    if (!gotResizeEvent) return false;
    while (gotResizeEvent) std::this_thread::yield();
    return true;
}

bool SDLTerminal::resizeWholeWindow(int w, int h) {
    const bool r = resize(w, h);
    if (!r) return r;
    queueTask([this](void*)->void*{SDL_SetWindowSize(win, (int)(width*charWidth*dpiScale+(4 * charScale * (2 / fontScale)*dpiScale)), (int)(height*charHeight*dpiScale+(4 * charScale * (2 / fontScale)*dpiScale))); return NULL;}, NULL);
    return r;
}

void SDLTerminal::screenshot(std::string path) {
    if (!path.empty()) screenshotPath = wstr(path);
    else {
        time_t now = time(0);
        struct tm * nowt = localtime(&now);
        screenshotPath = getBasePath();
#ifdef WIN32
        screenshotPath += WS("\\screenshots\\");
#else
        screenshotPath += WS("/screenshots/");
#endif
        createDirectory(screenshotPath);
        char tstr[24];
        strftime(tstr, 24, "%F_%H.%M.%S", nowt);
        tstr[23] = '\0';
#ifdef NO_PNG
        screenshotPath += wstr(std::string(tstr)) + WS(".bmp");
#else
        screenshotPath += wstr(std::string(tstr)) + WS(".png");
#endif
    }
    shouldScreenshot = true;
}

void SDLTerminal::record(std::string path) {
    shouldRecord = true;
    recordedFrames = 0;
    frameWait = 0;
    if (!path.empty()) recordingPath = wstr(path);
    else {
        time_t now = time(0);
        struct tm * nowt = localtime(&now);
        recordingPath = getBasePath();
#ifdef WIN32
        recordingPath += WS("\\screenshots\\");
#else
        recordingPath += WS("/screenshots/");
#endif
        createDirectory(recordingPath);
        char tstr[20];
        strftime(tstr, 20, "%F_%H.%M.%S", nowt);
        recordingPath += wstr(std::string(tstr)) + WS(".gif");
    }
    changed = true;
}

static uint32_t *memset_int(uint32_t *ptr, uint32_t value, size_t num) {
    for (size_t i = 0; i < num; i++) memcpy(&ptr[i], &value, 4);
    return &ptr[num];
}

void SDLTerminal::stopRecording() {
    shouldRecord = false;
    recorderMutex.lock();
    if (recording.empty()) { recorderMutex.unlock(); return; }
    GifWriter g;
    g.f = platform_fopen(recordingPath.c_str(), "wb");
    GifBegin(&g, NULL, reinterpret_cast<uint32_t*>(&recording[0][0])[0], reinterpret_cast<uint32_t*>(&recording[0][0])[1], 100 / config.recordingFPS);
    for (std::string s : recording) {
        const uint32_t w = reinterpret_cast<uint32_t*>(&s[0])[0], h = reinterpret_cast<uint32_t*>(&s[0])[1];
        uint32_t* ipixels = new uint32_t[w * h];
        uint32_t* lp = ipixels;
        for (unsigned i = 2; i*4 < s.size(); i++) {
#ifdef __APPLE__
            // macOS has memset_pattern4, which is much more efficient than my C implementation, so use that if available
            uint32_t c = ((uint32_t*)&s[0])[i] & 0xFFFFFF;
            memset_pattern4(lp, &c, min(((((uint32_t*)&s[0])[i] & 0xFF000000) >> 24) + 1, (unsigned int)((w * h) - (lp - ipixels))) * 4);
            lp += min(((((uint32_t*)&s[0])[i] & 0xFF000000) >> 24) + 1, (unsigned int)((w * h) - (lp - ipixels)));
#else
            const uint32_t c = reinterpret_cast<uint32_t*>(&s[0])[i];
            lp = memset_int(lp, c & 0xFFFFFF, min(((c & 0xFF000000) >> 24) + 1, (unsigned int)((w*h) - (lp - ipixels))));
#endif
        }
        GifWriteFrame(&g, (uint8_t*)ipixels, w, h, 100 / config.recordingFPS);
        delete[] ipixels;
    }
    GifEnd(&g);
    recording.clear();
    recorderMutex.unlock();
#ifdef __EMSCRIPTEN__
    queueTask([](void*)->void*{syncfs(); return NULL;}, NULL, true);
#endif
    changed = true;
}

void SDLTerminal::showMessage(Uint32 flags, const char * title, const char * message) {SDL_ShowSimpleMessageBox(flags, title, message, win);}

void SDLTerminal::toggleFullscreen() {
    fullscreen = !fullscreen;
    if (fullscreen) queueTask([ ](void* param)->void*{SDL_SetWindowFullscreen((SDL_Window*)param, SDL_WINDOW_FULLSCREEN_DESKTOP); return NULL;}, win);
    else queueTask([ ](void* param)->void*{SDL_SetWindowFullscreen((SDL_Window*)param, 0); return NULL;}, win);
}

void SDLTerminal::setLabel(std::string label) {
    title = label;
    queueTask([label](void*win)->void*{SDL_SetWindowTitle((SDL_Window*)win, label.c_str()); return NULL;}, win, true);
}

void SDLTerminal::init() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_SetHint(SDL_HINT_RENDER_DIRECT3D_THREADSAFE, "1");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
#if SDL_VERSION_ATLEAST(2, 0, 8)
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
    task_event_type = SDL_RegisterEvents(2);
    render_event_type = task_event_type + 1;
    renderThread = new std::thread(termRenderLoop);
    setThreadName(*renderThread, "Render Thread");
}

void SDLTerminal::quit() {
    renderThread->join();
    delete renderThread;
    SDL_Quit();
}

#ifdef __EMSCRIPTEN__
#define checkWindowID(c, wid) (c->term == *SDLTerminal::renderTarget || findMonitorFromWindowID(c, (*SDLTerminal::renderTarget)->id, tmps) != NULL)
#else
#define checkWindowID(c, wid) ((wid) == (c)->term->id || findMonitorFromWindowID((c), (wid), tmps) != NULL)
#endif

bool SDLTerminal::pollEvents() {
    SDL_Event e;
    std::string tmps;
#ifdef __EMSCRIPTEN__
    if (SDL_PollEvent(&e)) {
#else
    if (SDL_WaitEvent(&e)) {
#endif
        if (e.type == task_event_type) {
            while (!taskQueue->empty()) {
                auto v = taskQueue->front();
                void* retval = std::get<1>(v)(std::get<2>(v));
                if (!std::get<3>(v)) {
                    LockGuard lock2(taskQueueReturns);
                    (*taskQueueReturns)[std::get<0>(v)] = retval;
                }
                taskQueue->pop();
            }
        } else if (e.type == render_event_type) {
#ifdef __EMSCRIPTEN__
            SDLTerminal* term = dynamic_cast<SDLTerminal*>(*SDLTerminal::renderTarget);
            if (term != NULL) {
                std::lock_guard<std::mutex> lock(term->locked);
                if (term->surf != NULL) {
                    SDL_BlitSurface(term->surf, NULL, SDL_GetWindowSurface(SDLTerminal::win), NULL);
                    SDL_UpdateWindowSurface(SDLTerminal::win);
                    SDL_FreeSurface(term->surf);
                    term->surf = NULL;
                }
            }
#else
            for (Terminal* term : renderTargets) {
                SDLTerminal * sdlterm = dynamic_cast<SDLTerminal*>(term);
                if (sdlterm != NULL) {
                    std::lock_guard<std::mutex> lock(sdlterm->renderlock);
                    if (sdlterm->surf != NULL) {
                        SDL_BlitSurface(sdlterm->surf, NULL, SDL_GetWindowSurface(sdlterm->win), NULL);
                        SDL_UpdateWindowSurface(sdlterm->win);
                        SDL_FreeSurface(sdlterm->surf);
                        sdlterm->surf = NULL;
                    }
                }
            }
#endif
        } else {
            if (rawClient) {
                sendRawEvent(e);
            } else {
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
                            monitor * m = findMonitorFromWindowID(c, lastWindow, tmps);
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
                            ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && checkWindowID(c, e.button.windowID)) ||
                            (e.type == SDL_MOUSEMOTION && checkWindowID(c, e.motion.windowID)) ||
                            (e.type == SDL_MOUSEWHEEL && checkWindowID(c, e.wheel.windowID)) ||
                            (e.type == SDL_TEXTINPUT && checkWindowID(c, e.text.windowID)) ||
                            (e.type == SDL_WINDOWEVENT && checkWindowID(c, e.window.windowID)) ||
                            e.type == SDL_QUIT) {
                            std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
                            c->termEventQueue.push(e);
                            c->event_lock.notify_all();
                            if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE && e.window.windowID == c->term->id) {
                                if (c->requestedExit) {
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
                                    if (id == 1) {
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
                for (Terminal * t : orphanedTerminals) {
                    if ((e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE && e.window.windowID == t->id) || e.type == SDL_QUIT) {
                        orphanedTerminals.erase(t);
                        delete t;
                        break;
                    }
                }
            }
            if (e.type == SDL_QUIT) return true;
        }
    }
    return false;
}
