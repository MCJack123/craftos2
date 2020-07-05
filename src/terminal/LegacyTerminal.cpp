/*
 * LegacyTerminal.cpp
 * CraftOS-PC 2
 * 
 * This file implements the LegacyTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "LegacyTerminal.hpp"
#ifndef NO_PNG
#include <png++/png.hpp>
#endif
#include <assert.h>
#include "../favicon.h"
#include "../config.hpp"
#include "../gif.hpp"
#include "../os.hpp"
#include "../peripheral/monitor.hpp"

#define HARDWARE_RENDERER

extern "C" {
    struct font_image {
        unsigned int 	 width;
        unsigned int 	 height;
        unsigned int 	 bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
        unsigned char	 pixel_data[128 * 175 * 2 + 1];
    };
    extern struct font_image font_image;
}

extern void MySDL_GetDisplayDPI(int displayIndex, float* dpi, float* defaultDpi);

int LegacyTerminal::fontScale = 2;

LegacyTerminal::LegacyTerminal(std::string title): Terminal(51, 19) {
    locked.unlock();
    float dpi, defaultDpi;
    MySDL_GetDisplayDPI(0, &dpi, &defaultDpi);
    dpiScale = (dpi / defaultDpi) - floor(dpi / defaultDpi) > 0.5 ? ceil(dpi / defaultDpi) : floor(dpi / defaultDpi);
    if (config.customFontPath == "hdfont") {
        fontScale = 1;
        charScale = 1;
        charWidth = fontWidth * 2/fontScale * charScale;
        charHeight = fontHeight * 2/fontScale * charScale;
    } else if (!config.customFontPath.empty()) {
        fontScale = config.customFontScale;
        charScale = 2 / fontScale;
        charWidth = fontWidth * 2/fontScale * charScale;
        charHeight = fontHeight * 2/fontScale * charScale;
    }
    if (config.customCharScale > 0) {
        charScale = config.customCharScale;
        charWidth = fontWidth * 2/fontScale * charScale;
        charHeight = fontHeight * 2/fontScale * charScale;
    }
    win = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width*charWidth+(4 * charScale), height*charHeight+(4 * charScale), SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_INPUT_FOCUS);
    if (win == nullptr || win == NULL || win == (SDL_Window*)0) 
        throw window_exception("Failed to create window");
    id = SDL_GetWindowID(win);
#ifndef __APPLE__
    char * icon_pixels = new char[favicon_width * favicon_height * 4];
    memset(icon_pixels, 0xFF, favicon_width * favicon_height * 4);
    const char * icon_data = header_data;
    for (int i = 0; i < favicon_width * favicon_height; i++) HEADER_PIXEL(icon_data, (&icon_pixels[i*4]));
    SDL_Surface* icon = SDL_CreateRGBSurfaceFrom(icon_pixels, favicon_width, favicon_height, 32, favicon_width * 4, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    SDL_SetWindowIcon(win, icon);
    SDL_FreeSurface(icon);
    delete[] icon_pixels;
#endif
#ifdef HARDWARE_RENDERER
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#else
    ren = SDL_CreateSoftwareRenderer(SDL_GetWindowSurface(win));
    dpiScale = 1;
#endif
    if (ren == nullptr || ren == NULL || ren == (SDL_Renderer*)0) {
        SDL_DestroyWindow(win);
        throw window_exception("Failed to create renderer: " + std::string(SDL_GetError()));
    }
    SDL_Surface* old_bmp;
    if (config.customFontPath.empty()) 
        old_bmp = SDL_CreateRGBSurfaceWithFormatFrom((void*)font_image.pixel_data, font_image.width, font_image.height, font_image.bytes_per_pixel * 8, font_image.bytes_per_pixel * font_image.width, SDL_PIXELFORMAT_RGB565);
    else if (config.customFontPath == "hdfont") old_bmp = SDL_LoadBMP((getROMPath() + "/hdfont.bmp").c_str());
    else old_bmp = SDL_LoadBMP(config.customFontPath.c_str());
    if (old_bmp == nullptr || old_bmp == NULL || old_bmp == (SDL_Surface*)0) {
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        throw window_exception("Failed to load font");
    }
    bmp = SDL_ConvertSurfaceFormat(old_bmp, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(old_bmp);
    SDL_SetColorKey(bmp, SDL_TRUE, SDL_MapRGB(bmp->format, 0, 0, 0));
    font = SDL_CreateTextureFromSurface(ren, bmp);
    if (font == nullptr || font == NULL || font == (SDL_Texture*)0) {
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        throw window_exception("Failed to load texture from font");
    }
    renderTargets.push_back(this);
}

LegacyTerminal::~LegacyTerminal() {
    Terminal::renderTargetsLock.lock();
    std::lock_guard<std::mutex> locked_g(locked);
    for (auto it = renderTargets.begin(); it != renderTargets.end(); it++) {
        if (*it == this)
            it = renderTargets.erase(it);
        if (it == renderTargets.end()) break;
    }
    Terminal::renderTargetsLock.unlock();
    if (!overridden) {
        SDL_DestroyTexture(font);
        SDL_FreeSurface(bmp);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
    }
}

void LegacyTerminal::setPalette(Color * p) {
    for (int i = 0; i < 16; i++) palette[i] = p[i];
}

void LegacyTerminal::setCharScale(int scale) {
    if (scale < 1) scale = 1;
    charScale = scale;
    charWidth = fontWidth * fontScale * charScale;
    charHeight = fontHeight * fontScale * charScale;
    SDL_SetWindowSize(win, width*charWidth+(4 * charScale), height*charHeight+(4 * charScale));
}

extern bool operator!=(Color lhs, Color rhs);

bool LegacyTerminal::drawChar(char c, int x, int y, Color fg, Color bg, bool transparent) {
    SDL_Rect srcrect = getCharacterRect(c);
    SDL_Rect destrect = {
        x * charWidth * dpiScale + 2 * charScale * 2/fontScale * dpiScale, 
        y * charHeight * dpiScale + 2 * charScale * 2/fontScale * dpiScale, 
        fontWidth * 2/fontScale * charScale * dpiScale, 
        fontHeight * 2/fontScale * charScale * dpiScale
    };
    if (!transparent && bg != palette[15]) {
        if (gotResizeEvent) return false;
        if (SDL_SetRenderDrawColor(ren, bg.r, bg.g, bg.b, 0xFF) != 0) return false;
        if (gotResizeEvent) return false;
        if (SDL_RenderFillRect(ren, &destrect) != 0) return false;
    }
    if (c != ' ' && c != '\0') {
        if (gotResizeEvent) return false;
        if (SDL_SetTextureColorMod(font, fg.r, fg.g, fg.b) != 0) return false;
        if (gotResizeEvent) return false;
        if (SDL_RenderCopy(ren, font, &srcrect, &destrect) != 0) return false;
    }
    return true;
}

extern SDL_Rect * setRect(SDL_Rect * rect, int x, int y, int w, int h);

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

void LegacyTerminal::render() {
    std::lock_guard<std::mutex> locked_g(locked);
    if (gotResizeEvent) {
        gotResizeEvent = false;
        this->screen.resize(newWidth, newHeight, ' ');
        this->colors.resize(newWidth, newHeight, 0xF0);
        this->pixels.resize(newWidth * fontWidth, newHeight * fontHeight, 0x0F);
        this->width = newWidth;
        this->height = newHeight;
        SDL_DestroyRenderer(ren);
        //ren = SDL_CreateSoftwareRenderer(SDL_GetWindowSurface(win));
        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        font = SDL_CreateTextureFromSurface(ren, bmp);
        changed = true;
    }
    if (!changed && !shouldScreenshot && !shouldRecord) return;
    changed = false;
    if (SDL_SetRenderDrawColor(ren, palette[15].r, palette[15].g, palette[15].b, 0xFF) != 0) return;
    if (SDL_RenderClear(ren) != 0) return;
    SDL_Rect rect;
    if (mode != 0) {
        for (int y = 0; y < height * charHeight; y+=fontScale*charScale) {
            for (int x = 0; x < width * charWidth; x+=fontScale*charScale) {
                char c = pixels[y / fontScale / charScale][x / fontScale / charScale];
                if (SDL_SetRenderDrawColor(ren, palette[c].r, palette[c].g, palette[c].b, 0xFF) != 0) return;
                if (SDL_RenderFillRect(ren, setRect(&rect, x + (2 * fontScale * charScale), y + (2 * fontScale * charScale), fontScale * charScale, fontScale * charScale)) != 0) return;
            }
        }
    } else {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                /*
                SDL_SetRenderDrawColor(ren, palette[colors[y][x] >> 4].r, palette[colors[y][x] >> 4].g, palette[colors[y][x] >> 4].b, 0xFF);
                if (x == 0)
                    SDL_RenderFillRect(ren, setRect(&rect, 0, y * charHeight + (2 * fontScale * charScale), 2 * fontScale * charScale, charHeight));
                if (y == 0)
                    SDL_RenderFillRect(ren, setRect(&rect, x * charWidth + (2 * fontScale * charScale), 0, charWidth, 2 * fontScale * charScale));
                if (x + 1 == width)
                    SDL_RenderFillRect(ren, setRect(&rect, (x + 1) * charWidth + (2 * fontScale * charScale), y * charHeight + (2 * fontScale * charScale), 2 * fontScale * charScale, charHeight));
                if (y + 1 == height)
                    SDL_RenderFillRect(ren, setRect(&rect, x * charWidth + (2 * fontScale * charScale), (y + 1) * charHeight + (2 * fontScale * charScale), charWidth, 2 * fontScale * charScale));
                if (x == 0 && y == 0)
                    SDL_RenderFillRect(ren, setRect(&rect, 0, 0, 2 * fontScale * charScale, 2 * fontScale * charScale));
                if (x == 0 && y + 1 == height)
                    SDL_RenderFillRect(ren, setRect(&rect, 0, (y + 1) * charHeight + (2 * fontScale * charScale), 2 * fontScale * charScale, 2 * fontScale * charScale));
                if (x + 1 == width && y == 0)
                    SDL_RenderFillRect(ren, setRect(&rect, (x + 1) * charWidth + (2 * fontScale * charScale), 0, 2 * fontScale * charScale, 2 * fontScale * charScale));
                if (x + 1 == width && y + 1 == height)
                    SDL_RenderFillRect(ren, setRect(&rect, (x + 1) * charWidth + (2 * fontScale * charScale), (y + 1) * charHeight + (2 * fontScale * charScale), 2 * fontScale * charScale, 2 * fontScale * charScale));
                */
                if (gotResizeEvent) return;
                if (!drawChar(screen[y][x], x, y, palette[colors[y][x] & 0x0F], palette[colors[y][x] >> 4])) return;
            }
        }
		if (blinkX >= width) blinkX = width - 1;
		if (blinkY >= height) blinkY = height - 1;
		if (blinkX < 0) blinkX = 0;
		if (blinkY < 0) blinkY = 0;
        if (gotResizeEvent) return;
        if (blink) if (!drawChar('_', blinkX, blinkY, palette[0], palette[colors[blinkY][blinkX] >> 4], true)) return;
    }
    currentFPS++;
    if (lastSecond != time(0)) {
        lastSecond = time(0);
        lastFPS = currentFPS;
        currentFPS = 0;
    }
    if (/*showFPS*/ false) {
        // later
    }
    if (shouldScreenshot) {
        shouldScreenshot = false;
        int w, h;
        if (gotResizeEvent) return;
        if (SDL_GetRendererOutputSize(ren, &w, &h) != 0) return;
#ifdef PNGPP_PNG_HPP_INCLUDED
        png::solid_pixel_buffer<png::rgb_pixel> pixbuf(w, h);
        if (gotResizeEvent) return;
        if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_RGB24, (void*)&pixbuf.get_bytes()[0], w * 3) != 0) return;
        png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel> > img(w, h);
        img.set_pixbuf(pixbuf);
        img.write(screenshotPath);
#else
        SDL_Surface *sshot = SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
        if (gotResizeEvent) return;
        if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, sshot->pixels, sshot->pitch) != 0) return;
        SDL_Surface *conv = SDL_ConvertSurfaceFormat(sshot, SDL_PIXELFORMAT_RGB888, 0);
        SDL_FreeSurface(sshot);
        SDL_SaveBMP(conv, screenshotPath.c_str());
        SDL_FreeSurface(conv);
#endif
    }
    if (shouldRecord) {
        if (recordedFrames >= 150) stopRecording();
        else if (--frameWait < 1) {
            recorderMutex.lock();
            int w, h;
            if (SDL_GetRendererOutputSize(ren, &w, &h) != 0) return;
            SDL_Surface *sshot = SDL_CreateRGBSurface(0, w, h, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
            if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ABGR8888, sshot->pixels, sshot->pitch) != 0) return;
            uint32_t uw = static_cast<uint32_t>(w), uh = static_cast<uint32_t>(h);
            std::string rle = std::string((char*)&uw, 4) + std::string((char*)&uh, 4);
            uint32_t * px = ((uint32_t*)sshot->pixels);
            uint32_t data = px[0] & 0xFFFFFF;
            for (int y = 0; y < h; y++) {
                for (int x = 0; x < w; x++) {
                    uint32_t p = px[y*w+x];
                    if ((p & 0xFFFFFF) != (data & 0xFFFFFF) || (data & 0xFF000000) == 0xFF000000) {
                        rle += std::string((char*)&data, 4);
                        data = p & 0xFFFFFF;
                    } else data += 0x1000000;
                }
            }
            rle += std::string((char*)&data, 4);
            SDL_FreeSurface(sshot);
            recording.push_back(rle);
            recordedFrames++;
            frameWait = config.clockSpeed / 10;
            recorderMutex.unlock();
        }
        SDL_Surface* circle = SDL_CreateRGBSurfaceWithFormatFrom(circlePix, 10, 10, 32, 40, SDL_PIXELFORMAT_BGRA32);
        if (circle == NULL) { printf("Error: %s\n", SDL_GetError()); assert(false); }
        SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, circle);
        if (SDL_RenderCopy(ren, tex, NULL, setRect(&rect, (width * charWidth * dpiScale + 2 * charScale * fontScale * dpiScale) - 10, 2 * charScale * fontScale * dpiScale, 10, 10)) != 0) return;
        SDL_FreeSurface(circle);
    }
}

extern void convert_to_renderer_coordinates(SDL_Renderer *renderer, int *x, int *y);

void LegacyTerminal::getMouse(int *x, int *y) {
    SDL_GetMouseState(x, y);
    convert_to_renderer_coordinates(ren, x, y);
}

SDL_Rect LegacyTerminal::getCharacterRect(char c) {
    SDL_Rect retval;
    retval.w = fontWidth * 2/fontScale;
    retval.h = fontHeight * 2/fontScale;
    retval.x = ((fontWidth + 2) * 2/fontScale)*(c & 0x0F)+2/fontScale;
    retval.y = ((fontHeight + 2) * 2/fontScale)*(c >> 4)+2/fontScale;
    return retval;
}

bool LegacyTerminal::resize(int w, int h) {
    SDL_DestroyRenderer(ren);
#ifdef HARDWARE_RENDERER
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#else
    ren = SDL_CreateSoftwareRenderer(SDL_GetWindowSurface(win));
#endif
    font = SDL_CreateTextureFromSurface(ren, bmp);
    newWidth = (w - 4*fontScale*charScale) / charWidth;
    newHeight = (h - 4*fontScale*charScale) / charHeight;
    gotResizeEvent = (newWidth != width || newHeight != height);
    if (!gotResizeEvent) return false;
    while (gotResizeEvent) std::this_thread::yield();
    return true;
}

void LegacyTerminal::screenshot(std::string path) {
    shouldScreenshot = true;
    if (path != "") screenshotPath = path;
    else {
        time_t now = time(0);
        struct tm * nowt = localtime(&now);
        screenshotPath = getBasePath();
#ifdef WIN32
        screenshotPath += "\\screenshots\\";
#else
        screenshotPath += "/screenshots/";
#endif
        createDirectory(screenshotPath.c_str());
        char * tstr = new char[24];
        strftime(tstr, 24, "%F_%H.%M.%S", nowt);
#ifdef NO_PNG
        screenshotPath += std::string(tstr) + ".bmp";
#else
        screenshotPath += std::string(tstr) + ".png";
#endif
        delete[] tstr;
    }
}

void LegacyTerminal::record(std::string path) {
    shouldRecord = true;
    recordedFrames = 0;
    frameWait = 0;
    if (path != "") recordingPath = path;
    else {
        time_t now = time(0);
        struct tm * nowt = localtime(&now);
        recordingPath = getBasePath();
#ifdef WIN32
        recordingPath += "\\screenshots\\";
#else
        recordingPath += "/screenshots/";
#endif
        createDirectory(recordingPath.c_str());
        char * tstr = new char[20];
        strftime(tstr, 24, "%F_%H.%M.%S", nowt);
        recordingPath += std::string(tstr) + ".gif";
        delete[] tstr;
    }
}

extern uint32_t *memset_int(uint32_t *ptr, uint32_t value, size_t num);

void LegacyTerminal::stopRecording() {
    shouldRecord = false;
    recorderMutex.lock();
    if (recording.size() < 1) return;
    GifWriter g;
    GifBegin(&g, recordingPath.c_str(), ((uint32_t*)(&recording[0][0]))[0], ((uint32_t*)(&recording[0][0]))[1], 10);
    for (std::string s : recording) {
        uint32_t w = ((uint32_t*)&s[0])[0], h = ((uint32_t*)&s[0])[1];
        uint32_t* ipixels = new uint32_t[w * h];
        uint32_t* lp = ipixels;
        for (int i = 2; i*4 < s.size(); i++) {
            uint32_t c = ((uint32_t*)&s[0])[i];
            lp = memset_int(lp, c & 0xFFFFFF, ((c & 0xFF000000) >> 24) + 1);
        }
        GifWriteFrame(&g, (uint8_t*)ipixels, w, h, 10);
        delete[] ipixels;
    }
    GifEnd(&g);
    recording.clear();
    recorderMutex.unlock();
}

void LegacyTerminal::showMessage(Uint32 flags, const char * title, const char * message) {SDL_ShowSimpleMessageBox(flags, title, message, win);}

void LegacyTerminal::toggleFullscreen() {
    fullscreen = !fullscreen;
    if (fullscreen) queueTask([ ](void* param)->void*{SDL_SetWindowFullscreen((SDL_Window*)param, SDL_WINDOW_FULLSCREEN_DESKTOP); return NULL;}, win);
    else queueTask([ ](void* param)->void*{SDL_SetWindowFullscreen((SDL_Window*)param, 0); return NULL;}, win);
}

void LegacyTerminal::setLabel(std::string label) {
    title = label;
    queueTask([label](void*win)->void*{SDL_SetWindowTitle((SDL_Window*)win, label.c_str()); return NULL;}, win, true);
}

extern Uint32 task_event_type, render_event_type;
extern std::thread * renderThread;
extern void termRenderLoop();

void LegacyTerminal::init() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_SetHint(SDL_HINT_RENDER_DIRECT3D_THREADSAFE, "1");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
#if SDL_VERSION_ATLEAST(2, 0, 8)
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
    task_event_type = SDL_RegisterEvents(2);
    render_event_type = task_event_type + 1;
    renderThread = new std::thread(termRenderLoop);
}

void LegacyTerminal::quit() {
    renderThread->join();
    delete renderThread;
    SDL_Quit();
}

extern std::queue< std::tuple<int, std::function<void*(void*)>, void*, bool> > taskQueue;
extern std::unordered_map<int, void*> taskQueueReturns;
extern std::mutex taskQueueReturnsMutex;
extern monitor * findMonitorFromWindowID(Computer *comp, unsigned id, std::string& sideReturn);

extern bool rawClient;
extern void sendRawEvent(SDL_Event e);

#ifdef __EMSCRIPTEN__
#define checkWindowID(c, wid) (c->term == *LegacyTerminal::renderTarget || findMonitorFromWindowID(c, (*LegacyTerminal::renderTarget)->id, tmps) != NULL)
#else
#define checkWindowID(c, wid) (wid == c->term->id || findMonitorFromWindowID(c, wid, tmps) != NULL)
#endif

bool LegacyTerminal::pollEvents() {
	SDL_Event e;
	std::string tmps;
#ifdef __EMSCRIPTEN__
	if (SDL_PollEvent(&e)) {
#else
	if (SDL_WaitEvent(&e)) {
#endif
		if (e.type == task_event_type) {
			while (taskQueue.size() > 0) {
				auto v = taskQueue.front();
				void* retval = std::get<1>(v)(std::get<2>(v));
				if (!std::get<3>(v)) {
                    std::lock_guard<std::mutex> lock2(taskQueueReturnsMutex);
                    taskQueueReturns[std::get<0>(v)] = retval;
                }
				taskQueue.pop();
			}
		} else if (e.type == render_event_type) {
            std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
#ifdef __EMSCRIPTEN__
			LegacyTerminal* term = dynamic_cast<LegacyTerminal*>(*LegacyTerminal::renderTarget);
			std::lock_guard<std::mutex> lock(term->locked);
			if (term->surf != NULL) {
				SDL_BlitSurface(term->surf, NULL, SDL_GetWindowSurface(LegacyTerminal::win), NULL);
				SDL_UpdateWindowSurface(LegacyTerminal::win);
				SDL_FreeSurface(term->surf);
				term->surf = NULL;
			}
#else
			for (Terminal* term : Terminal::renderTargets) {
				LegacyTerminal * sdlterm = dynamic_cast<LegacyTerminal*>(term);
				if (sdlterm != NULL) {
					std::lock_guard<std::mutex> lock(sdlterm->locked);
					SDL_RenderPresent(sdlterm->ren);
                    SDL_UpdateWindowSurface(sdlterm->win);
				}
			}
#endif
            printf("Drawing took %lld us\n", std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count());
		} else {
            if (rawClient) {
                sendRawEvent(e);
            } else {
                for (Computer * c : computers) {
                    if (((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && checkWindowID(c, e.key.windowID)) ||
                        ((e.type == SDL_DROPFILE || e.type == SDL_DROPTEXT || e.type == SDL_DROPBEGIN || e.type == SDL_DROPCOMPLETE) && checkWindowID(c, e.drop.windowID)) ||
                        ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && checkWindowID(c, e.button.windowID)) ||
                        (e.type == SDL_MOUSEMOTION && checkWindowID(c, e.motion.windowID)) ||
                        (e.type == SDL_MOUSEWHEEL && checkWindowID(c, e.wheel.windowID)) ||
                        (e.type == SDL_TEXTINPUT && checkWindowID(c, e.text.windowID)) ||
                        (e.type == SDL_WINDOWEVENT && checkWindowID(c, e.window.windowID)) ||
                        e.type == SDL_QUIT) {
                        c->termEventQueue.push(e);
                        c->event_lock.notify_all();
                    }
                }
            }
			if (e.type == SDL_QUIT) return true;
		}
	}
	return false;
}