/*
 * terminal/CLITerminal.hpp
 * CraftOS-PC 2
 * 
 * This file defines the CLITerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#ifndef NO_CLI
#ifndef TERMINAL_CLITERMINAL_HPP
#define TERMINAL_CLITERMINAL_HPP
#include <set>
#include <string>
#include <Terminal.hpp>
#undef scroll

class CLITerminal: public Terminal {
    friend void mainLoop();
    friend void pressControl(int sig);
    friend void pressAlt(int sig);
    unsigned last_pair;
    static unsigned short lastPaletteChecksum;
    static std::set<unsigned>::iterator selectedWindow;
    static std::set<unsigned> currentIDs;
public:
    static void init();
    static void quit();
    static bool pollEvents();
    static void renderNavbar(std::string title);
    static void nextWindow();
    static void previousWindow();
    static bool stopRender;
    static bool forceRender;

    CLITerminal(std::string title);
    ~CLITerminal() override;
    bool drawChar(char c, int x, int y, Color fg, Color bg, bool transparent = false);
    void render() override;
    bool resize(unsigned w, unsigned h) override;
    void getMouse(int *x, int *y);
    void showMessage(uint32_t flags, const char * title, const char * message) override;
    void setLabel(std::string label) override;
};

#endif
#endif