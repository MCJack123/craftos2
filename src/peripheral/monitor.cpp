/*
 * peripheral/monitor.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the monitor peripheral.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include "monitor.hpp"
#include "../runtime.hpp"
#include "../terminal/SDLTerminal.hpp"
#include "../terminal/CLITerminal.hpp"
#include "../terminal/RawTerminal.hpp"
#include "../terminal/TRoRTerminal.hpp"
#include "../terminal/HardwareSDLTerminal.hpp"

monitor::monitor(lua_State *L, const char * side) {
#ifndef NO_CLI
    if (selectedRenderer == 2) term = new CLITerminal("CraftOS Terminal: Monitor " + std::string(side));
    else
#endif
    if (selectedRenderer == 3) term = new RawTerminal("CraftOS Terminal: Monitor " + std::string(side));
    else if (selectedRenderer == 4) term = new TRoRTerminal("CraftOS Terminal: Monitor " + std::string(side));
    else if (selectedRenderer == 0) {
        term = (SDLTerminal*)queueTask([ ](void* side)->void* {
            return new SDLTerminal("CraftOS Terminal: Monitor " + std::string((const char*)side));
        }, (void*)side);
    } else if (selectedRenderer == 5) {
        term = (HardwareSDLTerminal*)queueTask([ ](void* side)->void* {
            return new HardwareSDLTerminal("CraftOS Terminal: Monitor " + std::string((const char*)side));
        }, (void*)side);
    } else throw std::invalid_argument("Monitors are not available in headless mode");
    term->canBlink = false;
}

monitor::~monitor() {delete term;}

int monitor::write(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 4) printf("TW:%d;%s\n", term->id, luaL_checkstring(L, 1));
    if (term->blinkY < 0 || (term->blinkX >= 0 && (unsigned)term->blinkX >= term->width) || (unsigned)term->blinkY >= term->height) return 0;
    size_t str_sz;
    const char * str = luaL_checklstring(L, 1, &str_sz);
    std::lock_guard<std::mutex> lock(term->locked);
    for (unsigned i = 0; i < str_sz && (term->blinkX < 0 || (unsigned)term->blinkX < term->width); i++, term->blinkX++) {
        if (term->blinkX >= 0) {
            term->screen[term->blinkY][term->blinkX] = str[i];
            term->colors[term->blinkY][term->blinkX] = colors;
        }
    }
    term->changed = true;
    return 0;
}

int monitor::scroll(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 4) printf("TS:%d;%d\n", term->id, (int)luaL_checkinteger(L, 1));
    const int lines = (int)luaL_checkinteger(L, 1);
    std::lock_guard<std::mutex> lock(term->locked);
    if (lines > 0 ? (unsigned)lines >= term->height : (unsigned)-lines >= term->height) {
        // scrolling more than the height is equivalent to clearing the screen
        memset(term->screen.data(), ' ', term->height * term->width);
        memset(term->colors.data(), colors, term->height * term->width);
    } else if (lines > 0) {
        memmove(term->screen.data(), term->screen.data() + lines * term->width, (term->height - lines) * term->width);
        memset(term->screen.data() + (term->height - lines) * term->width, ' ', lines * term->width);
        memmove(term->colors.data(), term->colors.data() + lines * term->width, (term->height - lines) * term->width);
        memset(term->colors.data() + (term->height - lines) * term->width, colors, lines * term->width);
    } else if (lines < 0) {
        memmove(term->screen.data() - lines * term->width, term->screen.data(), (term->height + lines) * term->width);
        memset(term->screen.data(), ' ', -lines * term->width);
        memmove(term->colors.data() - lines * term->width, term->colors.data(), (term->height + lines) * term->width);
        memset(term->colors.data(), colors, -lines * term->width);
    }
    term->changed = true;
    return 0;
}

int monitor::setCursorPos(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 4) printf("TC:%d;%d,%d\n", term->id, (int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
    const int x = (int)luaL_checkinteger(L, 1);
    const int y = (int)luaL_checkinteger(L, 2);
    std::lock_guard<std::mutex> lock(term->locked);
    term->blinkX = x - 1;
    term->blinkY = y - 1;
    return 0;
}

int monitor::setCursorBlink(lua_State *L) {
    lastCFunction = __func__;
    luaL_checktype(L, 1, LUA_TBOOLEAN);
    std::lock_guard<std::mutex> lock(term->locked);
    term->canBlink = lua_toboolean(L, 1);
    if (selectedRenderer == 4) printf("TB:%d;%s\n", term->id, lua_toboolean(L, 1) ? "true" : "false");
    return 0;
}

int monitor::getCursorPos(lua_State *L) {
    lastCFunction = __func__;
    lua_pushinteger(L, (lua_Integer)term->blinkX + 1);
    lua_pushinteger(L, (lua_Integer)term->blinkY + 1);
    return 2;
}

int monitor::getCursorBlink(lua_State *L) {
    lastCFunction = __func__;
    lua_pushboolean(L, term->canBlink);
    return 1;
}

int monitor::getSize(lua_State *L) {
    lastCFunction = __func__;
    lua_pushinteger(L, term->width);
    lua_pushinteger(L, term->height);
    return 2;
}

int monitor::clear(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 4) printf("TE:%d;\n", term->id);
    std::lock_guard<std::mutex> lock(term->locked);
    if (term->mode > 0) {
        memset(term->pixels.data(), 0x0F, term->width * Terminal::fontWidth * term->height * Terminal::fontHeight);
    } else {
        memset(term->screen.data(), ' ', term->height * term->width);
        memset(term->colors.data(), colors, term->height * term->width);
    }
    term->changed = true;
    return 0;
}

int monitor::clearLine(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 4) printf("TL:%d;\n", term->id);
    if (term->blinkY < 0 || (unsigned)term->blinkY >= term->height) return 0;
    std::lock_guard<std::mutex> lock(term->locked);
    memset(term->screen.data() + (term->blinkY * term->width), ' ', term->width);
    memset(term->colors.data() + (term->blinkY * term->width), colors, term->width);
    term->changed = true;
    return 0;
}

int monitor::setTextColor(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 4 && luaL_checkinteger(L, 1) >= 0 && luaL_checkinteger(L, 1) < 16)
        printf("TF:%d;%c\n", term->id, ("0123456789abcdef")[lua_tointeger(L, 1)]);
    const int c = log2i((int)luaL_checkinteger(L, 1));
    if (c < 0 || c > 15) return luaL_error(L, "bad argument #1 (invalid color %d)", c);
    colors = (colors & 0xf0) | c;
    if (dynamic_cast<SDLTerminal*>(term) != NULL) dynamic_cast<SDLTerminal*>(term)->cursorColor = c;
    return 0;
}

int monitor::setBackgroundColor(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 4 && luaL_checkinteger(L, 1) >= 0 && luaL_checkinteger(L, 1) < 16)
        printf("TK:%d;%c\n", term->id, ("0123456789abcdef")[lua_tointeger(L, 1)]);
    const int c = log2i((int)luaL_checkinteger(L, 1));
    if (c < 0 || c > 15) return luaL_error(L, "bad argument #1 (invalid color %d)", c);
    colors = (colors & 0x0f) | (c << 4);
    return 0;
}

int monitor::isColor(lua_State *L) {
    lastCFunction = __func__;
    lua_pushboolean(L, true);
    return 1;
}

int monitor::getTextColor(lua_State *L) {
    lastCFunction = __func__;
    lua_pushinteger(L, (lua_Integer)1 << ((int)colors & 0x0f));
    return 1;
}

int monitor::getBackgroundColor(lua_State *L) {
    lastCFunction = __func__;
    lua_pushinteger(L, (lua_Integer)1 << ((int)colors >> 4));
    return 1;
}

int monitor::blit(lua_State *L) {
    lastCFunction = __func__;
    size_t str_sz, fg_sz, bg_sz;
    const char * str = luaL_checklstring(L, 1, &str_sz);
    const char * fg = luaL_checklstring(L, 2, &fg_sz);
    const char * bg = luaL_checklstring(L, 3, &bg_sz);
    if (str_sz != fg_sz || fg_sz != bg_sz) luaL_error(L, "Arguments must be the same length");
    if (term->blinkY < 0 || (term->blinkX >= 0 && (unsigned)term->blinkX >= term->width) || (unsigned)term->blinkY >= term->height) return 0;
    std::lock_guard<std::mutex> lock(term->locked);
    for (unsigned i = 0; i < str_sz && (term->blinkX < 0 || (unsigned)term->blinkX < term->width); i++, term->blinkX++) {
        colors = htoi(bg[i]) << 4 | htoi(fg[i]);
        if (dynamic_cast<SDLTerminal*>(term) != NULL) dynamic_cast<SDLTerminal*>(term)->cursorColor = htoi(fg[i]);
        if (selectedRenderer == 4)
            printf("TF:%d;%c\nTK:%d;%c\nTW:%d;%c\n", term->id, ("0123456789abcdef")[colors & 0xf], term->id, ("0123456789abcdef")[colors >> 4], term->id, str[i]);
        term->screen[term->blinkY][term->blinkX] = str[i];
        term->colors[term->blinkY][term->blinkX] = colors;
    }
    term->changed = true;
    return 0;
}

int monitor::getPaletteColor(lua_State *L) {
    lastCFunction = __func__;
    int color;
    if (term->mode == 2) color = (int)luaL_checkinteger(L, 1);
    else color = log2i((int)luaL_checkinteger(L, 1));
    if (color < 0 || color > 255) return luaL_error(L, "bad argument #1 (invalid color %d)", color);
    lua_pushnumber(L, term->palette[color].r/255.0);
    lua_pushnumber(L, term->palette[color].g/255.0);
    lua_pushnumber(L, term->palette[color].b/255.0);
    return 3;
}

int monitor::setPaletteColor(lua_State *L) {
    lastCFunction = __func__;
    luaL_checkinteger(L, 2);
    if (!lua_isnoneornil(L, 3)) {
        luaL_checkinteger(L, 3);
        luaL_checkinteger(L, 4);
    }
    int color;
    if (term->mode == 2) color = (int)luaL_checkinteger(L, 1);
    else color = log2i((int)luaL_checkinteger(L, 1));
    if (color < 0 || color > 255) return luaL_error(L, "bad argument #1 (invalid color %d)", color);
    std::lock_guard<std::mutex> lock(term->locked);
    if (lua_isnoneornil(L, 3)) {
        const unsigned int rgb = (int)lua_tointeger(L, 2);
        term->palette[color].r = rgb >> 16 & 0xFF;
        term->palette[color].g = rgb >> 8 & 0xFF;
        term->palette[color].b = rgb & 0xFF;
    } else {
        term->palette[color].r = (int)(lua_tonumber(L, 2) * 255);
        term->palette[color].g = (int)(lua_tonumber(L, 3) * 255);
        term->palette[color].b = (int)(lua_tonumber(L, 4) * 255);
    }
    if (selectedRenderer == 4 && color < 16) 
        printf("TM:%d;%d,%f,%f,%f\n", term->id, color, term->palette[color].r / 255.0, term->palette[color].g / 255.0, term->palette[color].b / 255.0);
    term->changed = true;
    return 0;
}

int monitor::setGraphicsMode(lua_State *L) {
    lastCFunction = __func__;
    if (!lua_isnumber(L, 1) && !lua_isboolean(L, 1)) luaL_typerror(L, 1, "number");
    if (selectedRenderer == 1 || selectedRenderer == 2) return 0;
    if (lua_isnumber(L, 1) && (lua_tointeger(L, 1) < 0 || lua_tointeger(L, 1) > 2)) return luaL_error(L, "bad argument %1 (invalid mode %d)", lua_tointeger(L, 1));
    std::lock_guard<std::mutex> lock(term->locked);
    term->mode = lua_isboolean(L, 1) ? (lua_toboolean(L, 1) ? 1 : 0) : (int)lua_tointeger(L, 1);
    term->changed = true;
    return 0;
}

int monitor::getGraphicsMode(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1 || selectedRenderer == 2) {
        lua_pushboolean(L, false);
        return 1;
    }
    if (term->mode == 0) lua_pushboolean(L, false);
    else lua_pushinteger(L, term->mode);
    return 1;
}

int monitor::setPixel(lua_State *L) {
    lastCFunction = __func__;
    luaL_checkinteger(L, 3);
    if (selectedRenderer == 1 || selectedRenderer == 2) return 0;
    const int x = (int)luaL_checkinteger(L, 1);
    const int y = (int)luaL_checkinteger(L, 2);
    std::lock_guard<std::mutex> lock(term->locked);
    const int color = term->mode == 1 ? log2i((int)lua_tointeger(L, 3)) : (int)lua_tointeger(L, 3);
    if (x < 0 || y < 0 || (unsigned)x >= term->width * 6 || (unsigned)y >= term->height * 9) return 0;
    if (color < 0 || color > (term->mode == 2 ? 255 : 15)) return luaL_error(L, "bad argument #3 (invalid color %d)", color);
    term->pixels[y][x] = color;
    term->changed = true;
    return 0;
}

int monitor::getPixel(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1 || selectedRenderer == 2) return 0;
    const int x = (int)luaL_checkinteger(L, 1);
    const int y = (int)luaL_checkinteger(L, 2);
    if (x < 0 || y < 0 || (unsigned)x >= term->width * Terminal::fontWidth || (unsigned)y >= term->height * Terminal::fontHeight) lua_pushnil(L);
    else if (term->mode == 1) lua_pushinteger(L, 1 << term->pixels[y][x]);
    else if (term->mode == 2) lua_pushinteger(L, term->pixels[y][x]);
    else return 0;
    return 1;
}

int monitor::setTextScale(lua_State *L) {
    lastCFunction = __func__;
    luaL_checkinteger(L, 1);
    SDLTerminal * sdlterm = dynamic_cast<SDLTerminal*>(term);
    if (sdlterm != NULL) {
        std::lock_guard<std::mutex> lock(sdlterm->locked);
        sdlterm->charScale = (unsigned)lua_tonumber(L, -1) * 2;
        queueTask([ ](void* term)->void*{((SDLTerminal*)term)->setCharScale(((SDLTerminal*)term)->charScale); return NULL;}, sdlterm);
        sdlterm->changed = true;
    }
    return 0;
}

int monitor::getTextScale(lua_State *L) {
    lastCFunction = __func__;
    SDLTerminal * sdlterm = dynamic_cast<SDLTerminal*>(term);
    if (sdlterm != NULL) lua_pushnumber(L, sdlterm->charScale / 2.0);
    else lua_pushnumber(L, 1.0);
    return 1;
}

int monitor::drawPixels(lua_State *L) {
    lastCFunction = __func__;
    const int init_x = (int)luaL_checkinteger(L, 1), init_y = (int)luaL_checkinteger(L, 2);
    luaL_checktype(L, 3, LUA_TTABLE);
    std::lock_guard<std::mutex> lock(term->locked);
    if (init_x < 0 || init_y < 0) luaL_error(L, "Invalid initial position");
    for (unsigned y = 1; y <= lua_objlen(L, 3) && init_y + y - 1 < (unsigned)term->height * Terminal::fontHeight; y++) {
        lua_pushinteger(L, y);
        lua_gettable(L, 3); 
        if (lua_isstring(L, -1)) {
            size_t str_sz;
            const char * str = lua_tolstring(L, -1, &str_sz);
            if (init_x + str_sz - 1 < (size_t)term->width * Terminal::fontWidth)
                memcpy(&term->pixels[init_y+y-1][init_x], str, str_sz);
        } else if (lua_istable(L, -1)) {
            for (unsigned x = 1; x <= lua_objlen(L, -1) && init_x + x - 1 < (unsigned)term->width * Terminal::fontWidth; x++) {
                lua_pushinteger(L, x);
                lua_gettable(L, -2);
                if (lua_isnumber(L, -1) && lua_tointeger(L, -1) >= 0) {
                    if (term->mode == 1) term->pixels[init_y + y - 1][init_x + x - 1] = (unsigned char)((unsigned)log2(lua_tointeger(L, -1)) % 256);
                    else term->pixels[init_y + y - 1][init_x + x - 1] = (unsigned char)(lua_tointeger(L, -1) % 256);
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }
    term->changed = true;
    return 0;
}

// LD: bruh this is just a copy of term_getPixels you lazy butthead
int monitor::getPixels(lua_State* L) {
    lastCFunction = __func__;

    const int init_x = (int) luaL_checkinteger(L, 1),
        init_y = (int) luaL_checkinteger(L, 2),
        end_w = (int) luaL_checkinteger(L, 3),
        end_h = (int) luaL_checkinteger(L, 4);

    if (end_w < 0 || end_h < 0) {
        return luaL_error(L, "Invalid size");
    }

    std::lock_guard<std::mutex> lock(term->locked);

    lua_createtable(L, 0, end_h);

    for (int h = 0; h < end_h; h++) {
        lua_pushnumber(L, h + 1);
        lua_createtable(L, end_w, 0);

        for (int w = 0; w < end_w; w++) {
            lua_pushnumber(L, w + 1);

            const int x = init_x + w,
                y = init_y + h;

            if (x < 0 || y < 0
                || (unsigned) x >= term->width * Terminal::fontWidth
                || (unsigned) y >= term->height * Terminal::fontHeight)
                lua_pushnil(L);
            else if (term->mode == 2)
                lua_pushinteger(L, term->pixels[y][x]);
            else
                lua_pushinteger(L, 1 << term->pixels[y][x]);

            lua_settable(L, -3);
        }

        lua_settable(L, -3);
    }

    return 1;
}

int monitor::fillPixels(lua_State* L) {
    lastCFunction = __func__;

    const int init_x = (int) luaL_checkinteger(L, 1),
              init_y = (int) luaL_checkinteger(L, 2),
              end_w = (int) luaL_checkinteger(L, 3),
              end_h = (int) luaL_checkinteger(L, 4),
              color = (int) luaL_checkinteger(L, 5);

    if (end_w < 0) return luaL_argerror(L, 3, "width must be positive");
    else if (end_h < 0) return luaL_argerror(L, 4, "height must be positive");
    else if (color < 0) return 0;
    else if (color >= term->mode == 2 ? 256 : 16)
        return luaL_argerror(L, 5, "color index out of bounds");

    std::lock_guard<std::mutex> lock(term->locked);

    for (int h = 0; h < end_h; h++) {
        const int y = init_y + h;

        if (y < 0 || y >= term->height * Terminal::fontHeight) continue;

        for (int w = 0; w < end_w; w++) {
            const int x = init_x + w;

            if (x >= 0 && x < term->width * Terminal::fontWidth) {
                term->pixels[y][x] = term->mode == 2 ? color : log2i(color);
            }
        }
    }

    return 0;
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
    else if (m == "drawPixels") return drawPixels(L);
    else if (m == "getPixels") return getPixels(L);
    else if (m == "fillPixels") return fillPixels(L);
    else return 0;
}

static luaL_Reg monitor_reg[] = {
    {"write", NULL},
    {"scroll", NULL},
    {"setCursorPos", NULL},
    {"setCursorBlink", NULL},
    {"getCursorPos", NULL},
    {"getCursorBlink", NULL},
    {"getSize", NULL},
    {"clear", NULL},
    {"clearLine", NULL},
    {"setTextColour", NULL},
    {"setTextColor", NULL},
    {"setBackgroundColour", NULL},
    {"setBackgroundColor", NULL},
    {"isColour", NULL},
    {"isColor", NULL},
    {"getTextColour", NULL},
    {"getTextColor", NULL},
    {"getBackgroundColour", NULL},
    {"getBackgroundColor", NULL},
    {"blit", NULL},
    {"getPaletteColor", NULL},
    {"getPaletteColour", NULL},
    {"setPaletteColor", NULL},
    {"setPaletteColour", NULL},
    {"setGraphicsMode", NULL},
    {"getGraphicsMode", NULL},
    {"setPixel", NULL},
    {"getPixel", NULL},
    {"setTextScale", NULL},
    {"getTextScale", NULL},
    {"drawPixels", NULL},
    {"getPixels", NULL},
    {"fillPixels", NULL},
    {NULL, NULL}
};

library_t monitor::methods = {"monitor", monitor_reg, nullptr, nullptr};
