#include "term.h"
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <ncurses.h>
#define TERM_WIDTH 80
#define TERM_HEIGHT 25

unsigned int cursorX = 0, cursorY = 0;
unsigned char colors = 0x07;
const unsigned char colorMap[16] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};

void termInit() {
    initscr();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();
    nodelay(stdscr, TRUE);
    start_color();
    for (int i = 0; i < 256; i++) init_pair(i, i & 0x0f, i >> 4);
}

void termClose() {
    echo();
    nocbreak();
    nodelay(stdscr, FALSE);
    keypad(stdscr, FALSE);
    endwin();
}

#define setchar(x, y, c) mvaddch(y, x, c);

int term_write(lua_State *L) {
    const char * str = lua_tostring(L, 1);
    #ifdef TESTING
    printf("%s\n", str);
    #endif
    for (int i = 0; i < strlen(str) && cursorX < TERM_WIDTH; i++, cursorX++)
        setchar(cursorX, cursorY, str[i]);
    refresh();
    return 0;
}

int term_scroll(lua_State *L) {
    scrl(lua_tointeger(L, 1));
    refresh();
    return 0;
}

int term_setCursorPos(lua_State *L) {
    cursorX = lua_tointeger(L, 1) - 1;
    cursorY = lua_tointeger(L, 2) - 1;
    move(cursorY, cursorX);
    refresh();
    return 0;
}

int term_setCursorBlink(lua_State *L) {
    curs_set(lua_toboolean(L, 1));
    refresh();
    return 0;
}

int term_getCursorPos(lua_State *L) {
    lua_pushinteger(L, cursorX + 1);
    lua_pushinteger(L, cursorY + 1);
    return 2;
}

int term_getSize(lua_State *L) {
    lua_pushinteger(L, TERM_WIDTH);
    lua_pushinteger(L, TERM_HEIGHT);
    return 2;
}

int term_clear(lua_State *L) {
    clear();
    refresh();
    return 0;
}

int term_clearLine(lua_State *L) {
    move(cursorY, 0);
    clrtoeol();
    refresh();
    return 0;
}

int log2i(int num) {
    int retval;
    for (retval = 0; num & 1 == 0; retval++) num = num >> 1;
    return retval;
}

int term_setTextColor(lua_State *L) {
    int c = colorMap[log2i(lua_tointeger(L, 1))];
    attroff(COLOR_PAIR(colors));
    colors = (colors & 0xf0) | c;
    attron(COLOR_PAIR(colors));
    return 0;
}

int term_setBackgroundColor(lua_State *L) {
    int c = colorMap[log2i(lua_tointeger(L, 1))];
    attroff(COLOR_PAIR(colors));
    colors = (colors & 0x0f) | (c << 4);
    attron(COLOR_PAIR(colors));
    return 0;
}

int term_isColor(lua_State *L) {
    lua_pushboolean(L, true);
    return 1;
}

int term_getTextColor(lua_State *L) {
    lua_pushinteger(L, 1 >> colorMap[colors & 0x0f]);
    return 1;
}

int term_getBackgroundColor(lua_State *L) {
    lua_pushinteger(L, 1 >> colorMap[colors >> 4]);
    return 1;
}

char htoi(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    else if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

int term_blit(lua_State *L) {
    const char * str = lua_tostring(L, 1);
    const char * fg = lua_tostring(L, 2);
    const char * bg = lua_tostring(L, 3);
    for (int i = 0; i < strlen(str) && cursorX < TERM_WIDTH; i++, cursorX++) {
        attroff(COLOR_PAIR(colors));
        colors = colorMap[htoi(bg[i])] << 4 | colorMap[htoi(fg[i])];
        attron(COLOR_PAIR(colors));
        setchar(cursorX, cursorY, str[i]);
    }
    refresh();
    return 0;
}

int term_getPaletteColor(lua_State *L) {
    int color = colorMap[log2i(lua_tointeger(L, 1))];
    switch (color) {
        case 0:
            lua_pushnumber(L, 0.0/255.0);
            lua_pushnumber(L, 0.0/255.0);
            lua_pushnumber(L, 0.0/255.0);
            return 3;
        case 1:
            lua_pushnumber(L, 0.0/255.0);
            lua_pushnumber(L, 0.0/255.0);
            lua_pushnumber(L, 170.0/255.0);
            return 3;
        case 2:
            lua_pushnumber(L, 0.0/255.0);
            lua_pushnumber(L, 170.0/255.0);
            lua_pushnumber(L, 0.0/255.0);
            return 3;
        case 3:
            lua_pushnumber(L, 0.0/255.0);
            lua_pushnumber(L, 170.0/255.0);
            lua_pushnumber(L, 170.0/255.0);
            return 3;
        case 4:
            lua_pushnumber(L, 170.0/255.0);
            lua_pushnumber(L, 0.0/255.0);
            lua_pushnumber(L, 0.0/255.0);
            return 3;
        case 5:
            lua_pushnumber(L, 170.0/255.0);
            lua_pushnumber(L, 0.0/255.0);
            lua_pushnumber(L, 170.0/255.0);
            return 3;
        case 6:
            lua_pushnumber(L, 170.0/255.0);
            lua_pushnumber(L, 85.0/255.0);
            lua_pushnumber(L, 0.0/255.0);
            return 3;
        case 7:
            lua_pushnumber(L, 170.0/255.0);
            lua_pushnumber(L, 170.0/255.0);
            lua_pushnumber(L, 170.0/255.0);
            return 3;
        case 8:
            lua_pushnumber(L, 85.0/255.0);
            lua_pushnumber(L, 85.0/255.0);
            lua_pushnumber(L, 85.0/255.0);
            return 3;
        case 9:
            lua_pushnumber(L, 85.0/255.0);
            lua_pushnumber(L, 85.0/255.0);
            lua_pushnumber(L, 255.0/255.0);
            return 3;
        case 10:
            lua_pushnumber(L, 85.0/255.0);
            lua_pushnumber(L, 255.0/255.0);
            lua_pushnumber(L, 85.0/255.0);
            return 3;
        case 11:
            lua_pushnumber(L, 85.0/255.0);
            lua_pushnumber(L, 255.0/255.0);
            lua_pushnumber(L, 255.0/255.0);
            return 3;
        case 12:
            lua_pushnumber(L, 255.0/255.0);
            lua_pushnumber(L, 85.0/255.0);
            lua_pushnumber(L, 85.0/255.0);
            return 3;
        case 13:
            lua_pushnumber(L, 255.0/255.0);
            lua_pushnumber(L, 85.0/255.0);
            lua_pushnumber(L, 255.0/255.0);
            return 3;
        case 14:
            lua_pushnumber(L, 255.0/255.0);
            lua_pushnumber(L, 255.0/255.0);
            lua_pushnumber(L, 85.0/255.0);
            return 3;
        case 15:
            lua_pushnumber(L, 255.0/255.0);
            lua_pushnumber(L, 255.0/255.0);
            lua_pushnumber(L, 255.0/255.0);
            return 3;
        default:
            return 0;
    }
}

int term_setPaletteColor(lua_State *L) {
    return 0;
}

const char * term_keys[23] = {
    "write",
    "scroll",
    "setCursorPos",
    "setCursorBlink",
    "getCursorPos",
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
    "setPaletteColour"
};

lua_CFunction term_values[23] = {
    term_write,
    term_scroll,
    term_setCursorPos,
    term_setCursorBlink,
    term_getCursorPos,
    term_getSize,
    term_clear,
    term_clearLine,
    term_setTextColor,
    term_setTextColor,
    term_setBackgroundColor,
    term_setBackgroundColor,
    term_isColor,
    term_isColor,
    term_getTextColor,
    term_getTextColor,
    term_getBackgroundColor,
    term_getBackgroundColor,
    term_blit,
    term_getPaletteColor,
    term_getPaletteColor,
    term_setPaletteColor,
    term_setPaletteColor
};

library_t term_lib = {"term", 23, term_keys, term_values};