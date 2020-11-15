/*
 * terminal/SDLTerminal.hpp
 * CraftOS-PC 2
 * 
 * This file defines the SDLTerminal class, which is the default renderer.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef TERMINAL_SDLTERMINAL_HPP
#define TERMINAL_SDLTERMINAL_HPP
#include <ctime>
#include <mutex>
#include <string>
#include <vector>
#include <SDL2/SDL.h>
#include <Terminal.hpp>
#include "../platform.hpp"

inline SDL_Rect * setRect(SDL_Rect * rect, int x, int y, int w, int h) {
    rect->x = x;
    rect->y = y;
    rect->w = w;
    rect->h = h;
    return rect;
}

class SDLTerminal: public Terminal {
    friend void mainLoop();
    friend int termPanic(lua_State *L);
    friend int runRenderer();
protected:
    bool shouldScreenshot = false;
    bool shouldRecord = false;
    bool fullscreen = false;
    path_t screenshotPath;
    path_t recordingPath;
    int recordedFrames = 0;
    int frameWait = 0;
    std::vector<std::string> recording;
    std::mutex recorderMutex;
    std::mutex renderlock;
    bool overridden = false;
public:
    static int fontScale;
    int charScale = 2;
    int dpiScale = 1;
    int charWidth = fontWidth * 2/fontScale * charScale;
    int charHeight = fontHeight * 2/fontScale * charScale;
    int lastFPS = 0;
    int currentFPS = 0;
    int lastSecond = time(0);
    std::chrono::system_clock::time_point lastScreenshotTime;
    unsigned char cursorColor = 0;

    static void init();
    static void quit();
    static bool pollEvents();
    SDLTerminal(std::string title);
    ~SDLTerminal() override;
    void setPalette(Color * p);
    void setCharScale(int scale);
    bool drawChar(unsigned char c, int x, int y, Color fg, Color bg, bool transparent = false);
    void render() override;
    bool resize(int w, int h) override;
    void getMouse(int *x, int *y);
    void screenshot(std::string path = ""); // asynchronous; captures on next render
    void record(std::string path = ""); // asynchronous; captures on next render
    void stopRecording();
    void toggleRecording() { if (shouldRecord) stopRecording(); else record(); }
    void showMessage(uint32_t flags, const char * title, const char * message) override;
    void toggleFullscreen();
    void setLabel(std::string label) override;
    virtual bool resizeWholeWindow(int w, int h);

#ifdef __EMSCRIPTEN__
    static SDL_Window *win;
#else
    SDL_Window *win;
#endif
protected:
    SDL_Surface *surf = NULL;
    SDL_Surface *bmp;

    static SDL_Rect getCharacterRect(unsigned char c);
};
#endif