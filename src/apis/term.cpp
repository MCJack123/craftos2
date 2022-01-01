/*
 * apis/term.cpp
 * CraftOS-PC 2
 *
 * This file implements the methods for the term API.
 *
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#include <Computer.hpp>
#include <configuration.hpp>
#include "../terminal/SDLTerminal.hpp"
#include "../runtime.hpp"
#include "../util.hpp"

static int headlessCursorX = 1, headlessCursorY = 1;
static bool can_blink_headless = true;

static int term_write(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1) {
        printf("%s", luaL_checkstring(L, 1));
        headlessCursorX += lua_strlen(L, 1);
        return 0;
    } else if (selectedRenderer == 4) printf("TW:%d;%s\n", get_comp(L)->term->id, luaL_checkstring(L, 1));
    Computer * computer = get_comp(L);
    Terminal * term = computer->term;
    size_t str_sz = 0;
    const char * str = luaL_checklstring(L, 1, &str_sz);
#ifdef TESTING
    printf("%s\n", str);
#endif
    std::lock_guard<std::mutex> locked_g(term->locked);
    if (term->blinkY < 0 || (term->blinkX >= 0 && (unsigned)term->blinkX >= term->width) || (unsigned)term->blinkY >= term->height) return 0;
    for (size_t i = 0; i < str_sz && (term->blinkX < 0 || (unsigned)term->blinkX < term->width); i++, term->blinkX++) {
        if (term->blinkX >= 0) {
            term->screen[term->blinkY][term->blinkX] = str[i];
            term->colors[term->blinkY][term->blinkX] = computer->colors;
        }
    }
    term->changed = true;
    return 0;
}

static int term_scroll(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1) {
        for (int i = 0; i < luaL_checkinteger(L, 1); i++) printf("\n");
        return 0;
    } else if (selectedRenderer == 4) printf("TS:%d;%d\n", get_comp(L)->term->id, (int)luaL_checkinteger(L, 1));
    Computer * computer = get_comp(L);
    Terminal * term = computer->term;
    const lua_Integer lines = luaL_checkinteger(L, 1);
    std::lock_guard<std::mutex> locked_g(term->locked);
    if (lines > 0 ? (unsigned)lines >= term->height : (unsigned)-lines >= term->height) {
        // scrolling more than the height is equivalent to clearing the screen
        memset(term->screen.data(), ' ', term->height * term->width);
        memset(term->colors.data(), computer->colors, term->height * term->width);
    } else if (lines > 0) {
        memmove(term->screen.data(), term->screen.data() + lines * term->width, (term->height - lines) * term->width);
        memset(term->screen.data() + (term->height - lines) * term->width, ' ', lines * term->width);
        memmove(term->colors.data(), term->colors.data() + lines * term->width, (term->height - lines) * term->width);
        memset(term->colors.data() + (term->height - lines) * term->width, computer->colors, lines * term->width);
    } else if (lines < 0) {
        memmove(term->screen.data() - lines * term->width, term->screen.data(), (term->height + lines) * term->width);
        memset(term->screen.data(), ' ', -lines * term->width);
        memmove(term->colors.data() - lines * term->width, term->colors.data(), (term->height + lines) * term->width);
        memset(term->colors.data(), computer->colors, -lines * term->width);
    }
    term->changed = true;
    return 0;
}

static int term_setCursorPos(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1) {
        if (luaL_checkinteger(L, 1) < headlessCursorX) printf("\r");
        else if (lua_tointeger(L, 1) > headlessCursorX) for (int i = headlessCursorX; i < lua_tointeger(L, 1); i++) printf(" ");
        if (luaL_checkinteger(L, 2) != headlessCursorY) printf("\n");
        headlessCursorX = (int)lua_tointeger(L, 1);
        headlessCursorY = (int)lua_tointeger(L, 2);
        fflush(stdout);
        return 0;
    } else if (selectedRenderer == 4) printf("TC:%d;%d,%d\n", get_comp(L)->term->id, (int)luaL_checkinteger(L, 1), (int)luaL_checkinteger(L, 2));
    luaL_checkinteger(L, 1);
    luaL_checkinteger(L, 2);
    Computer * computer = get_comp(L);
    Terminal * term = computer->term;
    std::lock_guard<std::mutex> locked_g(term->locked);
    term->blinkX = (int)lua_tointeger(L, 1) - 1;
    term->blinkY = (int)lua_tointeger(L, 2) - 1;
    term->changed = true;
    return 0;
}

static int term_setCursorBlink(lua_State *L) {
    lastCFunction = __func__;
    if (!lua_isboolean(L, 1)) luaL_typerror(L, 1, "boolean");
    if (selectedRenderer != 1) {
        Terminal * term = get_comp(L)->term;
        std::lock_guard<std::mutex> lock(term->locked);
        term->canBlink = lua_toboolean(L, 1);
        term->changed = true;
    } else can_blink_headless = lua_toboolean(L, 1);
    if (selectedRenderer == 4) printf("TB:%d;%s\n", get_comp(L)->term->id, lua_toboolean(L, 1) ? "true" : "false");
    return 0;
}

static int term_getCursorPos(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1) {
        lua_pushinteger(L, headlessCursorX);
        lua_pushinteger(L, headlessCursorY);
        return 2;
    }
    Computer * computer = get_comp(L);
    Terminal * term = computer->term;
    lua_pushinteger(L, (lua_Integer)term->blinkX + 1);
    lua_pushinteger(L, (lua_Integer)term->blinkY + 1);
    return 2;
}

static int term_getCursorBlink(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1) lua_pushboolean(L, can_blink_headless);
    else lua_pushboolean(L, get_comp(L)->term->canBlink);
    return 1;
}

static int term_getSize(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1) {
        lua_pushinteger(L, 51);
        lua_pushinteger(L, 19);
        return 2;
    }
    Computer * computer = get_comp(L);
    Terminal * term = computer->term;
    std::lock_guard<std::mutex> lock(term->locked);
    if ((lua_isboolean(L, 1) && lua_toboolean(L, 1)) || (lua_isnumber(L, 1) && lua_tonumber(L, 1) > 0)) {
        lua_pushinteger(L, term->width * Terminal::fontWidth);
        lua_pushinteger(L, term->height * Terminal::fontHeight);
    } else if (lua_isnoneornil(L, 1) || lua_isboolean(L, 1) || (lua_isnumber(L, 1) && lua_tonumber(L, 1) == 0)) {
        lua_pushinteger(L, term->width);
        lua_pushinteger(L, term->height);
    } else luaL_typerror(L, 1, "boolean or number");
    return 2;
}

static int term_clear(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1) {
        for (int i = 0; i < 30; i++) printf("\n");
        return 0;
    } else if (selectedRenderer == 4) printf("TE:%d;\n", get_comp(L)->term->id);
    Computer * computer = get_comp(L);
    Terminal * term = computer->term;
    std::lock_guard<std::mutex> locked_g(term->locked);
    if (term->mode > 0) {
        memset(term->pixels.data(), 0x0F, term->width * Terminal::fontWidth * term->height * Terminal::fontHeight);
    } else {
        memset(term->screen.data(), ' ', term->height * term->width);
        memset(term->colors.data(), computer->colors, term->height * term->width);
    }
    term->changed = true;
    return 0;
}

static int term_clearLine(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1) {
        printf("\r");
        for (int i = 0; i < 100; i++) printf(" ");
        printf("\r");
        return 0;
    } else if (selectedRenderer == 4) printf("TL:%d;\n", get_comp(L)->term->id);
    Computer * computer = get_comp(L);
    Terminal * term = computer->term;
    if (term->blinkY < 0 || (unsigned)term->blinkY >= term->height) return 0;
    std::lock_guard<std::mutex> locked_g(term->locked);
    memset(term->screen.data() + (term->blinkY * term->width), ' ', term->width);
    memset(term->colors.data() + (term->blinkY * term->width), computer->colors, term->width);
    term->changed = true;
    return 0;
}

static int term_setTextColor(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 4 && luaL_checkinteger(L, 1) >= 0 && luaL_checkinteger(L, 1) < 16)
        printf("TF:%d;%c\n", get_comp(L)->term->id, ("0123456789abcdef")[lua_tointeger(L, 1)]);
    Computer * computer = get_comp(L);
    const unsigned int c = log2i((int)luaL_checkinteger(L, 1));
    if (c > 15) return luaL_error(L, "bad argument #1 (invalid color %d)", c);
    //if ((computer->config->isColor || computer->isDebugger) || ((c & 7) - 1) >= 6) {
    computer->colors = (computer->colors & 0xf0) | (unsigned char)c;
    if (computer->term != NULL && dynamic_cast<SDLTerminal*>(computer->term) != NULL) {
        std::lock_guard<std::mutex> lock(computer->term->locked);
        dynamic_cast<SDLTerminal*>(computer->term)->cursorColor = (char)c;
    }
    //}
    return 0;
}

static int term_setBackgroundColor(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 4 && luaL_checkinteger(L, 1) >= 0 && luaL_checkinteger(L, 1) < 16)
        printf("TK:%d;%c\n", get_comp(L)->term->id, ("0123456789abcdef")[lua_tointeger(L, 1)]);
    Computer * computer = get_comp(L);
    const unsigned int c = log2i((int)luaL_checkinteger(L, 1));
    if (c > 15) return luaL_error(L, "bad argument #1 (invalid color %d)", c);
    //if ((computer->config->isColor || computer->isDebugger) || ((c & 7) - 1) >= 6)
    computer->colors = (computer->colors & 0x0f) | (unsigned char)(c << 4);
    return 0;
}

static int term_isColor(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1) {
        lua_pushboolean(L, true);
        return 1;
    }
    lua_pushboolean(L, (get_comp(L)->config->isColor || get_comp(L)->isDebugger));
    return 1;
}

static int term_getTextColor(lua_State *L) {
    lastCFunction = __func__;
    lua_pushinteger(L, (lua_Integer)1 << (get_comp(L)->colors & 0x0f));
    return 1;
}

static int term_getBackgroundColor(lua_State *L) {
    lastCFunction = __func__;
    lua_pushinteger(L, (lua_Integer)1 << (get_comp(L)->colors >> 4));
    return 1;
}

static int term_blit(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1) {
        printf("%s", lua_tostring(L, 1));
        headlessCursorX += lua_strlen(L, 1);
        return 0;
    }
    Computer * computer = get_comp(L);
    Terminal * term = computer->term;
    if (term == NULL) return 0;
    size_t str_sz, fg_sz, bg_sz;
    const char * str = luaL_checklstring(L, 1, &str_sz);
    const char * fg = luaL_checklstring(L, 2, &fg_sz);
    const char * bg = luaL_checklstring(L, 3, &bg_sz);
    if (str_sz != fg_sz || fg_sz != bg_sz) luaL_error(L, "Arguments must be the same length");
    std::lock_guard<std::mutex> locked_g(term->locked);
    if (term->blinkY < 0 || (term->blinkX >= 0 && (unsigned)term->blinkX >= term->width) || (unsigned)term->blinkY >= term->height) return 0;
    for (unsigned i = 0; i < str_sz && (term->blinkX < 0 || (unsigned)term->blinkX < term->width); i++, term->blinkX++) {
        if (term->blinkX >= 0) {
            computer->colors = (unsigned char)(htoi(bg[i], 15) << 4) | htoi(fg[i], 0);
            if (dynamic_cast<SDLTerminal*>(computer->term) != NULL) dynamic_cast<SDLTerminal*>(computer->term)->cursorColor = htoi(fg[i], 0);
            if (selectedRenderer == 4)
                printf("TF:%d;%c\nTK:%d;%c\nTW:%d;%c\n", term->id, ("0123456789abcdef")[computer->colors & 0xf], term->id, ("0123456789abcdef")[computer->colors >> 4], term->id, str[i]);
            term->screen[term->blinkY][term->blinkX] = str[i];
            term->colors[term->blinkY][term->blinkX] = computer->colors;
        }
    }
    term->changed = true;
    return 0;
}

static int term_getPaletteColor(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1) {
        if (luaL_checkinteger(L, 1) == 0x1) {
            lua_pushnumber(L, 0xF0 / 255.0);
            lua_pushnumber(L, 0xF0 / 255.0);
            lua_pushnumber(L, 0xF0 / 255.0);
        } else {
            lua_pushnumber(L, 0x19 / 255.0);
            lua_pushnumber(L, 0x19 / 255.0);
            lua_pushnumber(L, 0x19 / 255.0);
        }
        return 3;
    }
    Computer * computer = get_comp(L);
    Terminal * term = computer->term;
    int color;
    if (term->mode == 2) color = (int)luaL_checkinteger(L, 1);
    else color = log2i((int)luaL_checkinteger(L, 1));
    if (color < 0 || color > 255) return luaL_error(L, "bad argument #1 (invalid color %d)", color);
    lua_pushnumber(L, term->palette[color].r / 255.0);
    lua_pushnumber(L, term->palette[color].g / 255.0);
    lua_pushnumber(L, term->palette[color].b / 255.0);
    return 3;
}

static int term_setPaletteColor(lua_State *L) {
    lastCFunction = __func__;
    Computer * computer = get_comp(L);
    if (selectedRenderer == 1 || !(computer->config->isColor || computer->isDebugger)) return 0;
    Terminal * term = computer->term;
    int color;
    if (term->mode == 2) color = (int)luaL_checkinteger(L, 1);
    else color = log2i((int)luaL_checkinteger(L, 1));
    if (color < 0 || color > 255) return luaL_error(L, "bad argument #1 (invalid color %d)", color);
    std::lock_guard<std::mutex> lock(term->locked);
    if (lua_isnoneornil(L, 3)) {
        const unsigned int rgb = (unsigned int)luaL_checkinteger(L, 2);
        term->palette[color].r = rgb >> 16 & 0xFF;
        term->palette[color].g = rgb >> 8 & 0xFF;
        term->palette[color].b = rgb & 0xFF;
    } else {
        term->palette[color].r = (uint8_t)(luaL_checknumber(L, 2) * 255);
        term->palette[color].g = (uint8_t)(luaL_checknumber(L, 3) * 255);
        term->palette[color].b = (uint8_t)(luaL_checknumber(L, 4) * 255);
    }
    if (selectedRenderer == 4 && color < 16)
        printf("TM:%d;%d,%f,%f,%f\n", term->id, color, term->palette[color].r / 255.0, term->palette[color].g / 255.0, term->palette[color].b / 255.0);
    term->changed = true;
    return 0;
}

static int term_setGraphicsMode(lua_State *L) {
    lastCFunction = __func__;
    if (!lua_isboolean(L, 1) && !lua_isnumber(L, 1)) luaL_typerror(L, 1, "boolean or number");
    Computer * computer = get_comp(L);
    if (selectedRenderer == 1 || selectedRenderer == 2 || !(computer->config->isColor || computer->isDebugger)) return 0;
    if (lua_isnumber(L, 1) && (lua_tointeger(L, 1) < 0 || lua_tointeger(L, 1) > 2)) return luaL_error(L, "bad argument %1 (invalid mode %d)", lua_tointeger(L, 1));
    std::lock_guard<std::mutex> lock(computer->term->locked);
    computer->term->mode = lua_isboolean(L, 1) ? (lua_toboolean(L, 1) ? 1 : 0) : (int)lua_tointeger(L, 1);
    computer->term->changed = true;
    return 0;
}

static int term_getGraphicsMode(lua_State *L) {
    lastCFunction = __func__;
    Computer * computer = get_comp(L);
    if (selectedRenderer == 1 || selectedRenderer == 2 || !(computer->config->isColor || computer->isDebugger)) {
        lua_pushboolean(L, false);
        return 1;
    }
    if (computer->term->mode == 0) lua_pushboolean(L, false);
    else lua_pushinteger(L, computer->term->mode);
    return 1;
}

static int term_setPixel(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1 || selectedRenderer == 2) return 0;
    Computer * computer = get_comp(L);
    Terminal * term = computer->term;
    const int x = (int)luaL_checkinteger(L, 1);
    const int y = (int)luaL_checkinteger(L, 2);
    const int color = term->mode == 2 ? (int)luaL_checkinteger(L, 3) : log2i((int)luaL_checkinteger(L, 3));
    std::lock_guard<std::mutex> lock(term->locked);
    if (x < 0 || y < 0 || (unsigned)x >= term->width * Terminal::fontWidth || (unsigned)y >= term->height * Terminal::fontHeight) return 0;
    if (color < 0 || color > (term->mode == 2 ? 255 : 15)) return luaL_error(L, "bad argument #3 (invalid color %d)", color);
    term->pixels[y][x] = (unsigned char)color;
    term->changed = true;
    return 0;
}

static int term_getPixel(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer == 1 || selectedRenderer == 2) {
        lua_pushinteger(L, 0x8000);
        return 1;
    }
    Computer * computer = get_comp(L);
    Terminal * term = computer->term;
    const int x = (int)luaL_checkinteger(L, 1);
    const int y = (int)luaL_checkinteger(L, 2);
    if (x < 0 || y < 0 || (unsigned)x >= term->width * Terminal::fontWidth || (unsigned)y >= term->height * Terminal::fontHeight) lua_pushnil(L);
    else if (term->mode == 1) lua_pushinteger(L, 1 << term->pixels[y][x]);
    else if (term->mode == 2) lua_pushinteger(L, term->pixels[y][x]);
    else return 0;
    return 1;
}

static int term_drawPixels(lua_State *L) {
    lastCFunction = __func__;

    Computer* computer = get_comp(L);
    Terminal* term = computer->term;

    const int pixelWidth = term->width * Terminal::fontWidth,
              pixelHeight = term->height * Terminal::fontHeight;

    const int init_x = (int)luaL_checkinteger(L, 1),
              init_y = (int)luaL_checkinteger(L, 2);

    if (init_x >= pixelWidth || init_y >= pixelHeight) return 0;

    const int fillType = lua_type(L, 3);
    const bool isSolidFill = fillType == LUA_TNUMBER;

    if (!isSolidFill && fillType != LUA_TTABLE)
        return luaL_typerror(L, 3, "table or number");

    bool undefinedWidth;
    unsigned width, height;
    unsigned color;

    {
        int width_, height_;
        if (isSolidFill) {
            undefinedWidth = false;
            width_ = luaL_checkinteger(L, 4);
            height_ = luaL_checkinteger(L, 5);
        } else {
            undefinedWidth = lua_isnoneornil(L, 4);
            width_ = luaL_optinteger(L, 4, 0);
            height_ = luaL_optinteger(L, 5, lua_objlen(L, 3));
        }

        if (width_ < 0)
            return luaL_argerror(L, 4, "width cannot be negative");
        else if (height_ < 0)
            return luaL_argerror(L, 5, "height cannot be negative");

        width = (unsigned) width_;
        height = (unsigned) height_;
    }

    if (isSolidFill) {
        int color_ = lua_tonumber(L, 3);

        if (color_ < 0) return 0;
        else if (term->mode == 2 ? color_ > 255 : log2i(color_) > 15)
            return luaL_argerror(L, 3, "color index out of bounds");

        color = (unsigned) color_;
    }

    std::lock_guard<std::mutex> lock(term->locked);

    if (isSolidFill) {
        const unsigned char index = term->mode == 2
            ? (unsigned char) color
            : (unsigned char) log2i(color);

        const unsigned memset_x = max((int)init_x, 0),
                       memset_len = max(min((int) width, max(pixelWidth - init_x, 0)) + min((int)init_x, 0), 0);
        if (memset_len == 0) return 0;

        const int cool_height = min((int) height, pixelHeight - init_y);
        for (int h = max(-init_y, 0); h < cool_height; h++) {
            memset(&term->pixels[init_y + h][memset_x], index, memset_len);
        }

        term->changed = true;
        return 0;
    }

    const size_t str_offset = init_x < 0 ? (size_t)-init_x : 0,
                 str_maxlen = init_x > pixelWidth ? 0 : (size_t)(pixelWidth - init_x);

    const unsigned cool_height = min((int) height, pixelHeight - init_y);
    for (unsigned h = max(-init_y, 0); h < cool_height; h++) {
        lua_pushinteger(L, h + 1);
        lua_gettable(L, 3);

        if (lua_isstring(L, -1)) {
            if (str_offset >= str_maxlen) continue;

            size_t len;
            const char *str = lua_tolstring(L, -1, &len);
            if (len > str_maxlen) len = str_maxlen;
            if (!undefinedWidth && width < len) len = width;

            if (str_offset < len)
                memcpy(&term->pixels[init_y + h][init_x + str_offset],
                       str + str_offset,
                       len - str_offset
                );
        } else if (lua_istable(L, -1)) {
            // lol
            const unsigned cool_width = (unsigned) undefinedWidth
                ? (int) min(lua_objlen(L, -1), (size_t) (max(pixelWidth - init_x, 0)))
                : (int) min((int) width, pixelWidth - init_x);

            for (unsigned w = max(-init_x, 0); w < cool_width; w++) {
                lua_pushinteger(L, w + 1);
                lua_gettable(L, -2);

                if (lua_isnumber(L, -1)) {
                    const int color = lua_tointeger(L, -1);

                    if (color >= 0)
                        term->pixels[init_y + h][init_x + w] = term->mode == 2
                            ? color
                            : log2i(color);
                }

                lua_pop(L, 1);
            }
        }

        lua_pop(L, 1);
    }

    term->changed = true;
    return 0;
}

static int term_getPixels(lua_State* L) {
    lastCFunction = __func__;

    Computer* computer = get_comp(L);
    Terminal* term = computer->term;

    const int pixelWidth = term->width * Terminal::fontWidth,
              pixelHeight = term->height * Terminal::fontHeight;

    const int init_x = (int) luaL_checkinteger(L, 1),
              init_y = (int) luaL_checkinteger(L, 2),
              end_w = (int) luaL_checkinteger(L, 3),
              end_h = (int) luaL_checkinteger(L, 4);

    if (end_w < 0) return luaL_argerror(L, 3, "width cannot be negative");
    else if (end_h < 0) return luaL_argerror(L, 4, "height cannot be negative");
    else if (!lua_isnoneornil(L, 5) && !lua_isboolean(L, 5))
        return luaL_typerror(L, 5, "boolean");

    const bool use_strings = lua_toboolean(L, 5);

    lua_createtable(L, 0, end_h);

    const int cool_min_h = max(-init_y, 0);
    const int cool_max_h = min(end_h, pixelHeight - init_y);
    const int cool_min_w = max(-init_x, 0);
    const int cool_max_w = min(end_w, pixelWidth - init_x);

    // scratch space for drawing background color from
    char* bg = new char[end_w];
    memset(bg, 15, end_w);

    for (int h = 0; h < end_h; h++) {
        lua_pushnumber(L, h + 1);

        if (use_strings) {
            if (h < cool_min_h || h >= cool_max_h || cool_min_w >= cool_max_w) {
                lua_pushlstring(L, bg, end_w);
            } else {
                int concats = 1;

                if (cool_min_w > 0) {
                    lua_pushlstring(L, bg, cool_min_w);
                    concats++;
                }

                lua_pushlstring(L,
                    (const char *) &term->pixels[init_y + h][init_x + cool_min_w],
                    max(cool_max_w - cool_min_w, 0)
                );

                if (cool_max_w < end_w) {
                    lua_pushlstring(L, bg, end_w - cool_max_w);
                    concats++;
                }

                lua_concat(L, concats);
            }
        } else {
            lua_createtable(L, end_w, 0);

            for (int w = 0; w < end_w; w++) {
                lua_pushnumber(L, w + 1);

                const int x = init_x + w,
                          y = init_y + h;

                if (h < cool_min_h || h >= cool_max_h ||
                    w < cool_min_w || w >= cool_max_w)
                    lua_pushinteger(L, -1);
                else if (term->mode == 2)
                    lua_pushinteger(L, term->pixels[y][x]);
                else
                    lua_pushinteger(L, 1 << term->pixels[y][x]);

                lua_settable(L, -3);
            }
        }

        lua_settable(L, -3);
    }

    delete[] bg;
    return 1;
}

static int term_screenshot(lua_State *L) {
    lastCFunction = __func__;
    if (selectedRenderer != 0 && selectedRenderer != 5) return 0;
    Computer * computer = get_comp(L);
    SDLTerminal * term = dynamic_cast<SDLTerminal*>(computer->term);
    if (term == NULL) return 0;
    if (std::chrono::system_clock::now() - term->lastScreenshotTime < std::chrono::milliseconds(1000 / config.recordingFPS)) return 0;
    // Specifying a save path is no longer supported.
    if (lua_toboolean(L, 1)) term->screenshot("clipboard");
    else term->screenshot();
    term->lastScreenshotTime = std::chrono::system_clock::now();
    return 0;
}

static int term_nativePaletteColor(lua_State *L) {
    lastCFunction = __func__;
    const int color = log2i((int)luaL_checkinteger(L, 1));
    if (color < 0 || color > 15) return luaL_error(L, "bad argument #1 (invalid color %d)", color);
    const Color c = defaultPalette[color];
    lua_pushnumber(L, c.r / 255.0);
    lua_pushnumber(L, c.g / 255.0);
    lua_pushnumber(L, c.b / 255.0);
    return 3;
}

static int term_showMouse(lua_State *L) {
    lastCFunction = __func__;
    if (!lua_isboolean(L, 1)) luaL_typerror(L, 1, "boolean");
    SDL_ShowCursor(lua_toboolean(L, 1));
    return 0;
}

static int term_setFrozen(lua_State *L) {
    lastCFunction = __func__;
    if (!lua_isboolean(L, 1)) luaL_typerror(L, 1, "boolean");
    Terminal * term = get_comp(L)->term;
    if (term == NULL) return 0;
    std::lock_guard<std::mutex> lock(term->locked);
    term->frozen = lua_toboolean(L, 1);
    return 0;
}

static int term_getFrozen(lua_State *L) {
    lastCFunction = __func__;
    Terminal * term = get_comp(L)->term;
    if (term == NULL) return 0;
    lua_pushboolean(L, term->frozen);
    return 1;
}

/* export */ int term_benchmark(lua_State *L) {
    lastCFunction = __func__;
    if (get_comp(L)->term == NULL) return 0;
    lua_pushinteger(L, get_comp(L)->term->framecount);
    get_comp(L)->term->framecount = 0;
    return 1;
}

static luaL_reg term_reg[] = {
    {"write", term_write},
    {"scroll", term_scroll},
    {"setCursorPos", term_setCursorPos},
    {"setCursorBlink", term_setCursorBlink},
    {"getCursorPos", term_getCursorPos},
    {"getCursorBlink", term_getCursorBlink},
    {"getSize", term_getSize},
    {"clear", term_clear},
    {"clearLine", term_clearLine},
    {"setTextColour", term_setTextColor},
    {"setTextColor", term_setTextColor},
    {"setBackgroundColour", term_setBackgroundColor},
    {"setBackgroundColor", term_setBackgroundColor},
    {"isColour", term_isColor},
    {"isColor", term_isColor},
    {"getTextColour", term_getTextColor},
    {"getTextColor", term_getTextColor},
    {"getBackgroundColour", term_getBackgroundColor},
    {"getBackgroundColor", term_getBackgroundColor},
    {"blit", term_blit},
    {"getPaletteColor", term_getPaletteColor},
    {"getPaletteColour", term_getPaletteColor},
    {"setPaletteColor", term_setPaletteColor},
    {"setPaletteColour", term_setPaletteColor},
    {"setGraphicsMode", term_setGraphicsMode},
    {"getGraphicsMode", term_getGraphicsMode},
    {"setPixel", term_setPixel},
    {"getPixel", term_getPixel},
    {"screenshot", term_screenshot},
    {"nativePaletteColor", term_nativePaletteColor},
    {"nativePaletteColour", term_nativePaletteColor},
    {"drawPixels", term_drawPixels},
    {"getPixels", term_getPixels},
    {"showMouse", term_showMouse},
    {"setFrozen", term_setFrozen},
    {"getFrozen", term_getFrozen},
    {NULL, NULL}
};

library_t term_lib = {"term", term_reg, nullptr, nullptr};
