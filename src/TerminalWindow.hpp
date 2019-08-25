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
#ifdef WIN32
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

static Color defaultPalette[16] = {
    {0xf0, 0xf0, 0xf0},
    {0xf2, 0xb2, 0x33},
    {0xe5, 0x7f, 0xd8},
    {0x99, 0xb2, 0xf2},
    {0xde, 0xde, 0x6c},
    {0x7f, 0xcc, 0x19},
    {0xf2, 0xb2, 0xcc},
    {0x4c, 0x4c, 0x4c},
    {0x99, 0x99, 0x99},
    {0x4c, 0x99, 0xb2},
    {0xb2, 0x66, 0xe5},
    {0x33, 0x66, 0xcc},
    {0x7f, 0x66, 0x4c},
    {0x57, 0xa6, 0x4e},
    {0xcc, 0x4c, 0x4c},
    {0x11, 0x11, 0x11}
};

class window_exception: public std::exception {
    std::string err;
public:
    virtual const char * what() const throw() {
        char * retval = (char*)malloc(strlen("Could not create new terminal: ") + err.length() + 1);
        strcpy(retval, "Could not create new terminal: ");
        strcat(retval, err.c_str());
        return retval;
    }
    window_exception(std::string error): err(error) {}
    window_exception(): err("Unknown error") {}
};

class TerminalWindow {
public:
    int id;
    int width = 51;
    int height = 19;
    static const int fontWidth = 6;
    static const int fontHeight = 9;
private:
    static const int fontScale = 1;
    bool shouldScreenshot = false;
    bool shouldRecord = false;
    bool gotResizeEvent = false;
    int newWidth, newHeight;
    std::string screenshotPath;
    std::string recordingPath;
    int recordedFrames = 0;
    int frameWait = 0;
    std::vector<std::string> recording;
    std::mutex recorderMutex;
public:
    std::atomic_bool locked;
    int charScale = 2;
    int dpiScale = 1;
    int charWidth = fontWidth * fontScale * charScale;
    int charHeight = fontHeight * fontScale * charScale;
    std::vector<std::vector<char> > screen;
    std::vector<std::vector<unsigned char> > colors;
    std::vector<std::vector<char> > pixels;
    bool isPixel = false;
    Color palette[16];
    Color background = {0x1f, 0x1f, 0x1f};
    int blinkX = 0;
    int blinkY = 0;
    bool blink = true;
    int lastFPS = 0;
    int currentFPS = 0;
    int lastSecond = time(0);

    TerminalWindow(std::string title);
    ~TerminalWindow();
    void setPalette(Color * p);
    void setCharScale(int scale);
    bool drawChar(char c, int x, int y, Color fg, Color bg, bool transparent = false);
    bool drawCharSurface(SDL_Surface* surf, char c, int x, int y, Color fg, Color bg, bool transparent = false);
    void render();
    bool resize(int w, int h);
    void getMouse(int *x, int *y);
    void screenshot(std::string path = ""); // asynchronous; captures on next render
    void record(std::string path = ""); // asynchronous; captures on next render
    void stopRecording();
    void toggleRecording() { if (shouldRecord) stopRecording(); else record(); }

private:
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Surface *bmp;
    SDL_Texture *font;

    static SDL_Rect getCharacterRect(char c);
};
#endif