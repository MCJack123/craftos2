/*
 * CLITerminalWindow.hpp
 * CraftOS-PC 2
 * 
 * This file defines the CLITerminalWindow class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#ifndef NO_CLI
#ifndef CLITERMINALWINDOW_HPP
#define CLITERMINALWINDOW_HPP
#include "TerminalWindow.hpp"
#include <string>
#include <ncurses.h>
#include <vector>
#include <set>

class CLITerminalWindow: public TerminalWindow {
    friend void mainLoop();
    std::string title;
    unsigned last_pair;
    unsigned short lastPaletteChecksum = 0;
    static std::set<unsigned>::iterator selectedWindow;
public:
    static void renderNavbar(std::string title);
    static void nextWindow();
    static void previousWindow();

    CLITerminalWindow(std::string title);
    ~CLITerminalWindow() override;
    void setPalette(Color * p) {}
    void setCharScale(int scale) {}
    bool drawChar(char c, int x, int y, Color fg, Color bg, bool transparent = false);
    void render() override;
    bool resize(int w, int h) {return false;}
    void getMouse(int *x, int *y);
    void screenshot(std::string path = "") {}
    void record(std::string path = "") {}
    void stopRecording() {}
    void toggleRecording() {}
    void showMessage(Uint32 flags, const char * title, const char * message);
    void toggleFullscreen() {}
};

extern void cliInit();
extern void cliClose();
#endif
#endif