/*
 * peripheral/monitor.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the monitor peripheral.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "monitor.hpp"
#include "../term.hpp"
#include "../cli.hpp"

extern int log2i(int);
extern unsigned char htoi(char c);
extern bool cli;

monitor::monitor(lua_State *L, const char * side) {
    if (cli) {
        term = new CLITerminalWindow("CraftOS Terminal: Monitor " + std::string(side));
    } else {
        term = (TerminalWindow*)queueTask([ ](void* side)->void* {
            return new TerminalWindow("CraftOS Terminal: Monitor " + std::string((const char*)side));
        }, (void*)side);
    }
    canBlink = false;
}

monitor::~monitor() {delete term;}

int monitor::write(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * str = lua_tostring(L, 1);
    for (int i = 0; i < strlen(str) && term->blinkX < term->width; i++, term->blinkX++) {
        term->screen[term->blinkY][term->blinkX] = str[i];
        term->colors[term->blinkY][term->blinkX] = colors;
    }
    return 0;
}

int monitor::scroll(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    int lines = lua_tointeger(L, 1);
    for (int i = lines; i < term->height; i++) {
        term->screen[i-lines] = term->screen[i];
        term->colors[i-lines] = term->colors[i];
    }
    for (int i = term->height; i < term->height + lines; i++) {
        term->screen[i-lines] = std::vector<char>(term->width, ' ');
        term->colors[i-lines] = std::vector<unsigned char>(term->width, colors);
    }
    return 0;
}

int monitor::setCursorPos(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    term->blinkX = lua_tointeger(L, 1) - 1;
    term->blinkY = lua_tointeger(L, 2) - 1;
    if (term->blinkX >= term->width) term->blinkX = term->width - 1;
    if (term->blinkY >= term->height) term->blinkY = term->height - 1;
    if (term->blinkX < 0) term->blinkX = 0;
    if (term->blinkY < 0) term->blinkY = 0;
    return 0;
}

int monitor::setCursorBlink(lua_State *L) {
    if (!lua_isboolean(L, 1)) bad_argument(L, "boolean", 1);
    canBlink = lua_toboolean(L, 1);
    return 0;
}

int monitor::getCursorPos(lua_State *L) {
    lua_pushinteger(L, term->blinkX + 1);
    lua_pushinteger(L, term->blinkY + 1);
    return 2;
}

int monitor::getCursorBlink(lua_State *L) {
    lua_pushboolean(L, canBlink);
    return 1;
}

int monitor::getSize(lua_State *L) {
    lua_pushinteger(L, term->width);
    lua_pushinteger(L, term->height);
    return 2;
}

int monitor::clear(lua_State *L) {
    if (term->isPixel) {
        term->pixels = std::vector<std::vector<char> >(term->height * term->charHeight, std::vector<char>(term->width * term->charWidth, 15));
    } else {
        term->screen = std::vector<std::vector<char> >(term->height, std::vector<char>(term->width, ' '));
        term->colors = std::vector<std::vector<unsigned char> >(term->height, std::vector<unsigned char>(term->width, colors));
    }
    return 0;
}

int monitor::clearLine(lua_State *L) {
    term->screen[term->blinkY] = std::vector<char>(term->width, ' ');
    term->colors[term->blinkY] = std::vector<unsigned char>(term->width, colors);
    return 0;
}

int monitor::setTextColor(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    int c = log2i(lua_tointeger(L, 1));
    colors = (colors & 0xf0) | c;
    return 0;
}

int monitor::setBackgroundColor(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    int c = log2i(lua_tointeger(L, 1));
    colors = (colors & 0x0f) | (c << 4);
    return 0;
}

int monitor::isColor(lua_State *L) {
    lua_pushboolean(L, true);
    return 1;
}

int monitor::getTextColor(lua_State *L) {
    lua_pushinteger(L, 1 << ((int)colors & 0x0f));
    return 1;
}

int monitor::getBackgroundColor(lua_State *L) {
    lua_pushinteger(L, 1 << ((int)colors >> 4));
    return 1;
}

int monitor::blit(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    if (!lua_isstring(L, 3)) bad_argument(L, "string", 3);
    const char * str = lua_tostring(L, 1);
    const char * fg = lua_tostring(L, 2);
    const char * bg = lua_tostring(L, 3);
    for (int i = 0; i < strlen(str) && term->blinkX < term->width; i++, term->blinkX++) {
        colors = htoi(bg[i]) << 4 | htoi(fg[i]);
        term->screen[term->blinkY][term->blinkX] = str[i];
        term->colors[term->blinkY][term->blinkX] = colors;
    }
    return 0;
}

int monitor::getPaletteColor(lua_State *L) {
    int color = log2i(lua_tointeger(L, 1));
    lua_pushnumber(L, term->palette[color].r/255.0);
    lua_pushnumber(L, term->palette[color].g/255.0);
    lua_pushnumber(L, term->palette[color].b/255.0);
    return 3;
}

int monitor::setPaletteColor(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    if (!lua_isnumber(L, 3)) bad_argument(L, "number", 3);
    if (!lua_isnumber(L, 4)) bad_argument(L, "number", 4);
    int color = log2i(lua_tointeger(L, 1));
    term->palette[color].r = (int)(lua_tonumber(L, 2) * 255);
    term->palette[color].g = (int)(lua_tonumber(L, 3) * 255);
    term->palette[color].b = (int)(lua_tonumber(L, 4) * 255);
    return 0;
}

int monitor::setGraphicsMode(lua_State *L) {
    if (!lua_isboolean(L, 1)) bad_argument(L, "boolean", 1);
    term->isPixel = lua_toboolean(L, 1);
    return 0;
}

int monitor::getGraphicsMode(lua_State *L) {
    lua_pushboolean(L, term->isPixel);
    return 1;
}

int monitor::setPixel(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    if (!lua_isnumber(L, 3)) bad_argument(L, "number", 3);
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    if (x >= term->width * term->fontWidth || y >= term->height * term->fontHeight || x < 0 || y < 0) return 0;
    term->pixels[y][x] = log2i(lua_tointeger(L, 3));
    return 0;
}

int monitor::getPixel(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    if (x >= term->width * term->fontWidth || y >= term->height * term->fontHeight || x < 0 || y < 0) return 0;
    lua_pushinteger(L, 2^term->pixels[lua_tointeger(L, 2)][lua_tointeger(L, 1)]);
    return 1;
}

int monitor::setTextScale(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    term->charScale = lua_tonumber(L, -1) * 2;
    queueTask([ ](void* term)->void*{((TerminalWindow*)term)->setCharScale(((TerminalWindow*)term)->charScale); return NULL;}, term);
    return 0;
}

int monitor::getTextScale(lua_State *L) {
    lua_pushnumber(L, term->charScale / 2.0);
    return 1;
}

int monitor::call(lua_State *L, const char * method) {
    std::string m(method);
    if (m == "write") return write(L);
    else if (m == "scroll") return scroll(L);
    else if (m == "setCursorPos") return setCursorPos(L);
    else if (m == "setCursorBlink") return setCursorBlink(L);
    else if (m == "getCursorPos") return getCursorPos(L);
    else if (m == "getSize") return getSize(L);
    else if (m == "clear") return clear(L);
    else if (m == "clearLine") return clearLine(L);
    else if (m == "setTextColour") return setTextColor(L);
    else if (m == "setTextColor") return setTextColor(L);
    else if (m == "setBackgroundColour") return setBackgroundColor(L);
    else if (m == "setBackgroundColor") return setBackgroundColor(L);
    else if (m == "isColour") return isColor(L);
    else if (m == "isColor") return isColor(L);
    else if (m == "getTextColour") return getTextColor(L);
    else if (m == "getTextColor") return getTextColor(L);
    else if (m == "getBackgroundColour") return getBackgroundColor(L);
    else if (m == "getBackgroundColor") return getBackgroundColor(L);
    else if (m == "blit") return blit(L);
    else if (m == "getPaletteColor") return getPaletteColor(L);
    else if (m == "getPaletteColour") return getPaletteColor(L);
    else if (m == "setPaletteColor") return setPaletteColor(L);
    else if (m == "setPaletteColour") return setPaletteColor(L);
    else if (m == "setGraphicsMode") return setGraphicsMode(L);
    else if (m == "getGraphicsMode") return getGraphicsMode(L);
    else if (m == "setPixel") return setPixel(L);
    else if (m == "getPixel") return getPixel(L);
    else if (m == "setTextScale") return setTextScale(L);
    else if (m == "getTextScale") return getTextScale(L);
    else return 0;
}

void monitor::update() {
    if (!canBlink) term->blink = false;
    else if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - last_blink).count() > 500) {
        term->blink = !term->blink;
        last_blink = std::chrono::high_resolution_clock::now();
    }
    term->render();
}

const char * monitor_keys[30] = {
    "write",
    "scroll",
    "setCursorPos",
    "setCursorBlink",
    "getCursorPos",
    "getCursorBlink",
    "getSize",
    "clear",
    "clearLine",
    "setTextColour",
    "setTextColor",
    "setBackgroundColour",
    "setBackgroundColor",
    "isColour",
    "isColor",
    "getTextColour",
    "getTextColor",
    "getBackgroundColour",
    "getBackgroundColor",
    "blit",
    "getPaletteColor",
    "getPaletteColour",
    "setPaletteColor",
    "setPaletteColour",
    "setGraphicsMode",
    "getGraphicsMode",
    "setPixel",
    "getPixel",
    "setTextScale",
    "getTextScale"
};

library_t monitor::methods = {"monitor", 30, monitor_keys, NULL, NULL, NULL};