#ifndef PERIPHERAL_MONITOR_HPP
#define PERIPHERAL_MONITOR_HPP
#include "peripheral.h"
#include "../TerminalWindow.hpp"
#include <chrono>

class monitor : public peripheral {
private:
    unsigned char colors = 0xF0;
    bool canBlink = true;
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
public:
    TerminalWindow term;
    static library_t methods;
    library_t getMethods() {return methods;}
    monitor(lua_State *L, const char * side);
    int call(lua_State *L, const char * method);
    void update();
};
#endif