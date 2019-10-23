/*
 * TerminalWindow.hpp
 * CraftOS-PC 2
 * 
 * This file defines the TerminalWindow class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

class TerminalWindow;
#ifndef TERMINALWINDOW_HPP
#define TERMINALWINDOW_HPP
#ifdef _WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#include <string>
#include <vector>
#include <ctime>
#include <atomic>
#include <mutex>
#include "platform.hpp"

typedef struct color {
    Uint8 r;
    Uint8 g;
    Uint8 b;
} Color;

extern Color defaultPalette[16];

class window_exception: public std::exception {
    std::string err;
public:
    virtual const char * what() const throw() {return std::string("Could not create new terminal: " + err).c_str();}
    window_exception(std::string error): err(error) {}
    window_exception(): err("Unknown error") {}
};

class TerminalWindow {
    friend void mainLoop();
public:
    unsigned id;
    int width;
    int height;
    static const int fontWidth = 6;
    static const int fontHeight = 9;
    static std::list<TerminalWindow*> renderTargets;
    static std::mutex renderTargetsLock;
protected:
    static int fontScale;
    bool shouldScreenshot = false;
    bool shouldRecord = false;
    bool gotResizeEvent = false;
    bool fullscreen = false;
    int newWidth, newHeight;
    std::string screenshotPath;
    std::string recordingPath;
    int recordedFrames = 0;
    int frameWait = 0;
    std::vector<std::string> recording;
    std::mutex recorderMutex;
    bool overridden = false;
    TerminalWindow(int w, int h);
public:
    std::mutex locked;
    int charScale = 2;
    int dpiScale = 1;
    int charWidth = fontWidth * 2/fontScale * charScale;
    int charHeight = fontHeight * 2/fontScale * charScale;
    std::vector<std::vector<char> > screen;
    std::vector<std::vector<unsigned char> > colors;
    std::vector<std::vector<unsigned char> > pixels;
    volatile int mode = 0;
    Color palette[256];
    Color background = {0x1f, 0x1f, 0x1f};
    int blinkX = 0;
    int blinkY = 0;
    bool blink = true;
    bool canBlink = true;
    std::chrono::high_resolution_clock::time_point last_blink = std::chrono::high_resolution_clock::now();
    int lastFPS = 0;
    int currentFPS = 0;
    int lastSecond = time(0);

    TerminalWindow(std::string title);
    virtual ~TerminalWindow();
    void setPalette(Color * p);
    void setCharScale(int scale);
    bool drawChar(char c, int x, int y, Color fg, Color bg, bool transparent = false);
    virtual void render();
    bool resize(int w, int h);
    void getMouse(int *x, int *y);
    void screenshot(std::string path = ""); // asynchronous; captures on next render
    void record(std::string path = ""); // asynchronous; captures on next render
    void stopRecording();
    void toggleRecording() { if (shouldRecord) stopRecording(); else record(); }
    void showMessage(Uint32 flags, const char * title, const char * message);
    void toggleFullscreen();

private:
    SDL_Window *win;
    SDL_Surface *surf = NULL;
    SDL_Surface *bmp;

    static SDL_Rect getCharacterRect(char c);
};
#endif