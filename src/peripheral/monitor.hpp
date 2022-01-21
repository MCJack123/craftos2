/*
 * peripheral/monitor.hpp
 * CraftOS-PC 2
 * 
 * This file defines the class for the monitor peripheral.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#ifndef PERIPHERAL_MONITOR_HPP
#define PERIPHERAL_MONITOR_HPP
#include <chrono>
#include <peripheral.hpp>
#include <Terminal.hpp>
#ifdef scroll
#undef scroll
#endif

class monitor : public peripheral {
private:
    unsigned char colors = 0xF0;
    std::chrono::high_resolution_clock::time_point last_blink = std::chrono::high_resolution_clock::now();
    int write(lua_State *L);
    int scroll(lua_State *L);
    int setCursorPos(lua_State *L);
    int setCursorBlink(lua_State *L);
    int getCursorPos(lua_State *L);
    int getCursorBlink(lua_State *L);
    int getSize(lua_State *L);
    int clear(lua_State *L);
    int clearLine(lua_State *L);
    int setTextColor(lua_State *L);
    int setBackgroundColor(lua_State *L);
    int isColor(lua_State *L);
    int getTextColor(lua_State *L);
    int getBackgroundColor(lua_State *L);
    int blit(lua_State *L);
    int getPaletteColor(lua_State *L);
    int setPaletteColor(lua_State *L);
    int setGraphicsMode(lua_State *L);
    int getGraphicsMode(lua_State *L);
    int setPixel(lua_State *L);
    int getPixel(lua_State *L);
    int setTextScale(lua_State *L);
    int getTextScale(lua_State *L);
    int drawPixels(lua_State *L);
    int getPixels(lua_State *L);
    int screenshot(lua_State *L);
    int setFrozen(lua_State *L);
    int getFrozen(lua_State *L);
public:
    Terminal * term;
    static library_t methods;
    static peripheral * init(lua_State *L, const char * side) {return new monitor(L, side);}
    static void deinit(peripheral * p) {delete (monitor*)p;}
    destructor getDestructor() const override {return deinit;}
    library_t getMethods() const override {return methods;}
    monitor(lua_State *L, const char * side);
    ~monitor();
    int call(lua_State *L, const char * method) override;
};
#endif
