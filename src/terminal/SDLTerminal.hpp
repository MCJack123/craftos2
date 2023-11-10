/*
 * terminal/SDLTerminal.hpp
 * CraftOS-PC 2
 * 
 * This file defines the SDLTerminal class, which is the default renderer.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2023 JackMacWindows.
 */

#ifndef TERMINAL_SDLTERMINAL_HPP
#define TERMINAL_SDLTERMINAL_HPP
#include <ctime>
#include <mutex>
#include <string>
#include <vector>
#include <Computer.hpp>
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
    friend int runRenderer(const std::function<std::string()>& read, const std::function<void(const std::string&)>& write);
    friend class HardwareSDLTerminal;
protected:
    bool shouldScreenshot = false;
    bool shouldRecord = false;
    bool fullscreen = false;
    path_t screenshotPath;
    path_t recordingPath;
    int recordedFrames = 0;
    int frameWait = 0;
    void * recorderHandle = NULL;
    std::mutex recorderMutex;
    std::mutex renderlock;
    bool overridden = false;
    int realWidth = 620;
    int realHeight = 350;
public:
    static unsigned fontScale;
    unsigned charScale = 2;
    unsigned dpiScale = 1;
    unsigned charWidth = fontWidth * charScale;
    unsigned charHeight = fontHeight * charScale;
    int lastFPS = 0;
    int currentFPS = 0;
    time_t lastSecond = time(0);
    std::chrono::system_clock::time_point lastScreenshotTime;
    unsigned char cursorColor = 0;
    bool useOrigFont = false;
    bool isOnTop = false;
    bool isRecordingWebP = false;
    std::mutex mouseMoveLock;
    std::vector<std::pair<SDL_FingerID, std::pair<int, int>>> fingers;
    int nFingers = 0;

    static void init();
    static void quit();
    static bool pollEvents();
    SDLTerminal(std::string title);
    ~SDLTerminal() override;
    void setPalette(Color * p);
    virtual void setCharScale(int scale);
    virtual bool drawChar(unsigned char c, int x, int y, Color fg, Color bg, bool transparent = false);
    void render() override;
    bool resize(unsigned w, unsigned h) override;
    void getMouse(int *x, int *y);
    void screenshot(std::string path = ""); // asynchronous; captures on next render
    void record(std::string path = ""); // asynchronous; captures on next render
    void stopRecording();
    void toggleRecording() { if (shouldRecord) stopRecording(); else record(); }
    void showMessage(uint32_t flags, const char * title, const char * message) override;
    void toggleFullscreen();
    void setLabel(std::string label) override;
    void onActivate() override;
    virtual bool resizeWholeWindow(int w, int h);

    SDL_Window *win;
    static SDL_Window *singleWin;
    static std::unordered_multimap<SDL_EventType, std::pair<sdl_event_handler, void*> > eventHandlers;
protected:
    friend void registerSDLEvent(SDL_EventType type, const sdl_event_handler& handler, void* userdata);
    friend int main(int argc, char*argv[]);
    SDL_Surface *surf = NULL;
    static SDL_Surface *bmp;
    static SDL_Surface *origfont;
    static Uint32 lastWindow;

    SDL_Rect getCharacterRect(unsigned char c);
};
#endif
