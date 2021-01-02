/*
 * terminal/HardwareSDLTerminal.hpp
 * CraftOS-PC 2
 * 
 * This file defines the HardwareSDLTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#ifndef TERMINAL_HARDWARESDLTERMINAL_HPP
#define TERMINAL_HARDWARESDLTERMINAL_HPP
#include "SDLTerminal.hpp"

class HardwareSDLTerminal : public SDLTerminal {
public:
    static void init();
    static void quit();
    static bool pollEvents();
    HardwareSDLTerminal(std::string title);
    ~HardwareSDLTerminal() override;
    bool drawChar(unsigned char c, int x, int y, Color fg, Color bg, bool transparent = false) override;
    void render() override;
    bool resize(unsigned w, unsigned h) override;
    void setCharScale(int scale) override;
private:
#ifdef __EMSCRIPTEN__
    static SDL_Renderer *ren;
    static SDL_Texture *font;
#else
    SDL_Renderer *ren = NULL;
    SDL_Texture *font = NULL;
#endif
    SDL_Texture *pixtex = NULL;
};
#endif