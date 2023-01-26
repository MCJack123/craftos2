/*
 * terminal/HardwareSDLTerminal.hpp
 * CraftOS-PC 2
 * 
 * This file defines the HardwareSDLTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2023 JackMacWindows.
 */

#ifndef TERMINAL_HARDWARESDLTERMINAL_HPP
#define TERMINAL_HARDWARESDLTERMINAL_HPP
#include "SDLTerminal.hpp"

class HardwareSDLTerminal : public SDLTerminal {
    friend class debug_adapter;
public:
    static void init();
    static void quit();
    static bool pollEvents();
    HardwareSDLTerminal(std::string title);
    ~HardwareSDLTerminal() override;
    bool drawChar(unsigned char c, int x, int y, Color fg, Color bg, bool transparent = false) override;
    void render() override;
    bool resize(unsigned w, unsigned h) override;
private:
    SDL_Renderer *ren = NULL;
    SDL_Texture *font = NULL;
    SDL_Texture *pixtex = NULL;
    static SDL_Renderer *singleRen;
    static SDL_Texture *singleFont;
    static SDL_Texture *singlePixtex;
};
#endif