/*
 * RawTerminal.hpp
 * CraftOS-PC 2
 * 
 * This file defines the RawTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef TERMINAL_RAWTERMINAL_HPP
#define TERMINAL_RAWTERMINAL_HPP
#include <Terminal.hpp>
#include <thread>
#include <set>
#include <SDL2/SDL.h>

class RawTerminal: public Terminal {
    static std::set<unsigned> currentIDs;
public:
    static void init();
    static void quit();
    static void showGlobalMessage(uint32_t flags, const char * title, const char * message);
    RawTerminal(std::string title);
    ~RawTerminal() override;
    void render() override;
    bool resize(int w, int h) override;
    void showMessage(uint32_t flags, const char * title, const char * message) override;
    void setLabel(std::string label) override;
};

extern void sendRawEvent(SDL_Event e);

#endif