/*
 * HardwareSDLTerminal.cpp
 * CraftOS-PC 2
 * 
 * This file implements the HardwareSDLTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#define CRAFTOSPC_INTERNAL
#include "HardwareSDLTerminal.hpp"
#ifndef NO_PNG
#include <png++/png.hpp>
#endif
#include <assert.h>
#include "../favicon.h"
#include "../config.hpp"
#include "../gif.hpp"
#include "../os.hpp"
#include "../peripheral/monitor.hpp"
#define rgb(color) ((color.r << 16) | (color.g << 8) | color.b)

extern "C" {
    struct font_image {
        unsigned int 	 width;
        unsigned int 	 height;
        unsigned int 	 bytes_per_pixel; /* 2:RGB16, 3:RGB, 4:RGBA */
        unsigned char	 pixel_data[128 * 175 * 2 + 1];
    };
    extern struct font_image font_image;
#ifdef __EMSCRIPTEN__
    extern void syncfs();
#endif
}

extern void MySDL_GetDisplayDPI(int displayIndex, float* dpi, float* defaultDpi);
extern std::string overrideHardwareDriver;

HardwareSDLTerminal::HardwareSDLTerminal(std::string title): SDLTerminal(title) {
    std::lock_guard<std::mutex> lock(locked); // try to prevent race condition (see explanation in render())
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED/* | SDL_RENDERER_PRESENTVSYNC*/);
    if (ren == nullptr || ren == NULL || ren == (SDL_Renderer*)0) {
        SDL_DestroyWindow(win);
        throw window_exception("Failed to create renderer: " + std::string(SDL_GetError()));
    }
    SDL_RendererInfo info;
    SDL_GetRendererInfo(ren, &info);
    if ((!overrideHardwareDriver.empty() && std::string(info.name) != overrideHardwareDriver) || 
        (overrideHardwareDriver.empty() && !config.preferredHardwareDriver.empty() && std::string(info.name) != config.preferredHardwareDriver))
        printf("Warning: Preferred driver %s not available, using %s instead.\n", (overrideHardwareDriver.empty() ? config.preferredHardwareDriver.c_str() : overrideHardwareDriver.c_str()), info.name);
    font = SDL_CreateTextureFromSurface(ren, bmp);
    if (font == nullptr || font == NULL || font == (SDL_Texture*)0) {
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        throw window_exception("Failed to load texture from font: " + std::string(SDL_GetError()));
    }
}

HardwareSDLTerminal::~HardwareSDLTerminal() {
    if (!overridden) {
        if (pixtex != NULL) SDL_DestroyTexture(pixtex);
        SDL_DestroyTexture(font);
        SDL_DestroyRenderer(ren);
    }
}

extern bool operator!=(Color lhs, Color rhs);

bool HardwareSDLTerminal::drawChar(unsigned char c, int x, int y, Color fg, Color bg, bool transparent) {
    SDL_Rect srcrect = SDLTerminal::getCharacterRect(c);
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

void HardwareSDLTerminal::render() {
    if (width == 0 || height == 0) return; // don't render if we don't have a valid screen size
    // copy the screen data so we can let Lua keep going without waiting for the mutex
    std::unique_ptr<vector2d<unsigned char> > newscreen;
    std::unique_ptr<vector2d<unsigned char> > newcolors;
    std::unique_ptr<vector2d<unsigned char> > newpixels;
    Color newpalette[256];
    int newblinkX, newblinkY, newmode;
    bool newblink;
    {
        std::lock_guard<std::mutex> locked_g(locked);
        if (ren == NULL || font == NULL) return; // race condition since HardwareSDLTerminal() is called after SDLTerminal(), which adds the terminal to the render targets
                                                 // wait until the renderer and font are initialized before doing any rendering
        if (gotResizeEvent) {
            gotResizeEvent = false;
            this->screen.resize(newWidth, newHeight, ' ');
            this->colors.resize(newWidth, newHeight, 0xF0);
            this->pixels.resize(newWidth * fontWidth, newHeight * fontHeight, 0x0F);
            this->width = newWidth;
            this->height = newHeight;
            changed = true;
        }
        if (!changed && !shouldScreenshot && !shouldRecord) return;
        newscreen = std::unique_ptr<vector2d<unsigned char> >(new vector2d<unsigned char>(screen));
        newcolors = std::unique_ptr<vector2d<unsigned char> >(new vector2d<unsigned char>(colors));
        newpixels = std::unique_ptr<vector2d<unsigned char> >(new vector2d<unsigned char>(pixels));
        memcpy(newpalette, palette, sizeof(newpalette));
        newblinkX = blinkX, newblinkY = blinkY, newmode = mode;
        newblink = blink;
        changed = false;
    }
    std::lock_guard<std::mutex> rlock(renderlock);
    Color bgcolor = newmode == 0 ? newpalette[15] : defaultPalette[15];
    if (SDL_SetRenderDrawColor(ren, bgcolor.r, bgcolor.g, bgcolor.b, 0xFF) != 0) return;
    if (SDL_RenderClear(ren) != 0) return;
    if (pixtex != NULL) {
        SDL_DestroyTexture(pixtex);
        pixtex = NULL;
    }
    SDL_Rect rect;
    if (newmode != 0) {
        SDL_Surface * surf = SDL_CreateRGBSurfaceWithFormat(0, width * charWidth, height * charHeight, 24, SDL_PIXELFORMAT_RGB888);
        for (int y = 0; y < height * charHeight; y+=(2/fontScale)*charScale) {
            for (int x = 0; x < width * charWidth; x+=(2/fontScale)*charScale) {
                unsigned char c = (*newpixels)[y / (2/fontScale) / charScale][x / (2/fontScale) / charScale];
                /*if (SDL_SetRenderDrawColor(ren, palette[c].r, palette[c].g, palette[c].b, 0xFF) != 0) return;
                if (SDL_RenderFillRect(ren, setRect(&rect, x + (2 * (2/fontScale) * charScale), y + (2 * (2/fontScale) * charScale), (2 / fontScale) * charScale, (2 / fontScale) * charScale)) != 0) return;*/
                if (gotResizeEvent) return;
                if (SDL_FillRect(surf, setRect(&rect, x, y, (2 / fontScale) * charScale, (2 / fontScale) * charScale), rgb(newpalette[(int)c])) != 0) return;
            }
        }
        pixtex = SDL_CreateTextureFromSurface(ren, surf);
        SDL_FreeSurface(surf);
        SDL_RenderCopy(ren, pixtex, NULL, setRect(&rect, (2 * (2 / fontScale) * charScale), (2 * (2 / fontScale) * charScale), width * charWidth, height * charHeight));
    } else {
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (gotResizeEvent) return;
                if (!drawChar((*newscreen)[y][x], x, y, newpalette[(*newcolors)[y][x] & 0x0F], newpalette[(*newcolors)[y][x] >> 4])) return;
            }
        }
		if (newblinkX >= width) newblinkX = width - 1;
		if (newblinkY >= height) newblinkY = height - 1;
		if (newblinkX < 0) newblinkX = 0;
		if (newblinkY < 0) newblinkY = 0;
        if (gotResizeEvent) return;
        if (newblink) if (!drawChar('_', newblinkX, newblinkY, newpalette[0], newpalette[(*newcolors)[newblinkY][newblinkX] >> 4], true)) return;
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
        if (screenshotPath == "clipboard") {
            SDL_Surface * temp = SDL_CreateRGBSurfaceWithFormat(0, w, h, 24, SDL_PIXELFORMAT_RGB24);
            if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_RGB24, temp->pixels, temp->pitch) != 0) return;
            copyImage(temp);
            SDL_FreeSurface(temp);
        } else {
            png::solid_pixel_buffer<png::rgb_pixel> pixbuf(w, h);
            if (gotResizeEvent) return;
            if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_RGB24, (void*)&pixbuf.get_bytes()[0], w * 3) != 0) return;
            png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel> > img(w, h);
            img.set_pixbuf(pixbuf);
            img.write(screenshotPath);
        }
#else
        SDL_Surface *sshot = SDL_CreateRGBSurface(0, w, h, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
        if (gotResizeEvent) return;
        if (SDL_RenderReadPixels(ren, NULL, SDL_PIXELFORMAT_ARGB8888, sshot->pixels, sshot->pitch) != 0) return;
        SDL_Surface *conv = SDL_ConvertSurfaceFormat(sshot, SDL_PIXELFORMAT_RGB888, 0);
        SDL_FreeSurface(sshot);
        SDL_SaveBMP(conv, screenshotPath.c_str());
        SDL_FreeSurface(conv);
#endif
#ifdef __EMSCRIPTEN__
        queueTask([](void*)->void* {syncfs(); return NULL; }, NULL, true);
#endif
    }
    if (shouldRecord) {
        if (recordedFrames >= config.maxRecordingTime * config.recordingFPS) stopRecording();
        else if (--frameWait < 1) {
            std::lock_guard<std::mutex> lock(recorderMutex);
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
            frameWait = config.clockSpeed / config.recordingFPS;
            if (gotResizeEvent) return;
        }
        SDL_Surface* circle = SDL_CreateRGBSurfaceWithFormatFrom(circlePix, 10, 10, 32, 40, SDL_PIXELFORMAT_BGRA32);
        if (circle == NULL) { printf("Error: %s\n", SDL_GetError()); assert(false); }
        SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, circle);
        if (SDL_RenderCopy(ren, tex, NULL, setRect(&rect, (width * charWidth * dpiScale + 2 * charScale * fontScale * dpiScale) - 10, 2 * charScale * fontScale * dpiScale, 10, 10)) != 0) return;
        SDL_FreeSurface(circle);
    }
}

extern void convert_to_renderer_coordinates(SDL_Renderer *renderer, int *x, int *y);

bool HardwareSDLTerminal::resize(int w, int h) {
    SDL_DestroyRenderer(ren);
    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    font = SDL_CreateTextureFromSurface(ren, bmp);
    newWidth = w;
    newHeight = h;
    gotResizeEvent = (newWidth != width || newHeight != height);
    if (!gotResizeEvent) return false;
    while (gotResizeEvent) std::this_thread::yield();
    return true;
}

extern uint32_t *memset_int(uint32_t *ptr, uint32_t value, size_t num);

extern Uint32 task_event_type, render_event_type;
extern std::thread * renderThread;
extern void termRenderLoop();

void HardwareSDLTerminal::init() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_SetHint(SDL_HINT_RENDER_DIRECT3D_THREADSAFE, "1");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
#if SDL_VERSION_ATLEAST(2, 0, 8)
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (!overrideHardwareDriver.empty()) SDL_SetHint(SDL_HINT_RENDER_DRIVER, overrideHardwareDriver.c_str());
    else if (!config.preferredHardwareDriver.empty()) SDL_SetHint(SDL_HINT_RENDER_DRIVER, config.preferredHardwareDriver.c_str());
    task_event_type = SDL_RegisterEvents(2);
    render_event_type = task_event_type + 1;
    renderThread = new std::thread(termRenderLoop);
    setThreadName(*renderThread, "Render Thread");
}

void HardwareSDLTerminal::quit() {
    renderThread->join();
    delete renderThread;
    SDL_Quit();
}

extern ProtectedObject<std::queue< std::tuple<int, std::function<void*(void*)>, void*, bool> > > taskQueue;
extern ProtectedObject<std::unordered_map<int, void*> > taskQueueReturns;
extern monitor * findMonitorFromWindowID(Computer *comp, unsigned id, std::string& sideReturn);

extern bool rawClient;
extern void sendRawEvent(SDL_Event e);

#ifdef __EMSCRIPTEN__
#define checkWindowID(c, wid) (c->term == *HardwareSDLTerminal::renderTarget || findMonitorFromWindowID(c, (*HardwareSDLTerminal::renderTarget)->id, tmps) != NULL)
#else
#define checkWindowID(c, wid) (wid == c->term->id || findMonitorFromWindowID(c, wid, tmps) != NULL)
#endif

bool HardwareSDLTerminal::pollEvents() {
	SDL_Event e;
	std::string tmps;
#ifdef __EMSCRIPTEN__
	if (SDL_PollEvent(&e)) {
#else
	if (SDL_WaitEvent(&e)) {
#endif
		if (e.type == task_event_type) {
			while (taskQueue->size() > 0) {
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
			HardwareSDLTerminal* term = dynamic_cast<HardwareSDLTerminal*>(*HardwareSDLTerminal::renderTarget);
            if (term != NULL) {
                std::lock_guard<std::mutex> lock(term->renderlock);
                SDL_RenderPresent(sdlterm->ren);
                SDL_UpdateWindowSurface(sdlterm->win);
            }
#else
			for (Terminal* term : Terminal::renderTargets) {
				HardwareSDLTerminal * sdlterm = dynamic_cast<HardwareSDLTerminal*>(term);
				if (sdlterm != NULL) {
					std::lock_guard<std::mutex> lock(sdlterm->renderlock);
					SDL_RenderPresent(sdlterm->ren);
                    SDL_UpdateWindowSurface(sdlterm->win);
				}
			}
#endif
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