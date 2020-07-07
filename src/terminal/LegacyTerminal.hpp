/*
 * LegacyTerminal.hpp
 * CraftOS-PC 2
 * 
 * This file defines the LegacyTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

class LegacyTerminal;
#ifndef LEGACYTERMINAL_HPP
#define LEGACYTERMINAL_HPP
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
#include "SDLTerminal.hpp"
#include "../platform.hpp"

class LegacyTerminal : public SDLTerminal {
public:
    static void init();
    static void quit();
	static bool pollEvents();
    LegacyTerminal(std::string title);
    ~LegacyTerminal() override;
    bool drawChar(unsigned char c, int x, int y, Color fg, Color bg, bool transparent = false);
    void render() override;
    bool resize(int w, int h) override;
private:
    SDL_Renderer *ren = NULL;
    SDL_Texture *font = NULL;
    SDL_Texture *pixtex = NULL;
};
#endif