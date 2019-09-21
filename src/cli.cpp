#ifndef NO_CLI
#include "cli.hpp"
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <ncurses.h>
#include <panel.h>

unsigned CLITerminalWindow::selectedWindow = 0;
unsigned CLITerminalWindow::nextID = 0;

void CLITerminalWindow::renderNavbar(std::string title) {
    move(LINES-1, 0);
    printw("%d: %s", selectedWindow+1, title.c_str());
}

CLITerminalWindow::CLITerminalWindow(std::string title): title(title), TerminalWindow(COLS, LINES-1) {
    id = nextID++;
    selectedWindow = id;
    last_pair = 0;
}

CLITerminalWindow::~CLITerminalWindow() {
    int i = 0;
    if (nextID == id + 1) nextID--;
}

bool CLITerminalWindow::drawChar(char c, int x, int y, Color fg, Color bg, bool transparent) {
    return false;
}

void CLITerminalWindow::render() {
    if (selectedWindow == id) {
        move(0, 0);
        clear();
        for (int y = 0; y < screen.size(); y++) {
            for (int x = 0; x < screen[y].size(); x++) {
                //wattron(term, COLOR_PAIR(colors[y][x]));
                move(y, x);
                addch(screen[y][x] | COLOR_PAIR(colors[y][x]));
                //wattroff(term, COLOR_PAIR(colors[y][x]));
            }
        }
        renderNavbar(title);
        move(blinkY, blinkX);
        refresh();
    }
}

void CLITerminalWindow::getMouse(int *x, int *y) {
    *x = -1;
    *y = -1;
}

void CLITerminalWindow::showMessage(Uint32 flags, const char * title, const char * message) {
    fprintf(stderr, "%s: %s\n", title, message);
}

void cliInit() {
    SDL_Init(SDL_INIT_AUDIO);
    initscr();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();
    nodelay(stdscr, TRUE);
    mousemask(BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON2_PRESSED | BUTTON2_CLICKED | BUTTON2_RELEASED | BUTTON3_PRESSED | BUTTON3_CLICKED | BUTTON3_RELEASED | REPORT_MOUSE_POSITION, NULL);
    start_color();
    for (int i = 0; i < 256; i++) init_pair(i, 15 - (i & 0x0f), 15 - ((i >> 4) & 0xf));
}

void cliClose() {
    echo();
    nocbreak();
    nodelay(stdscr, FALSE);
    keypad(stdscr, FALSE);
    endwin();
    SDL_Quit();
} /*

#define setchar(x, y, c) mvaddch(y, x, c);

int cli_write(lua_State *L) {
    Computer * computer = get_comp(L);
    const char * str = lua_tostring(L, 1);
    #ifdef TESTING
    printf("%s\n", str);
    #endif
    for (int i = 0; i < strlen(str) && computer->cursorX < COLS; i++, cursorX++)
        setchar(cursorX, cursorY, str[i]);
    refresh();
    return 0;
}

int cli_scroll(lua_State *L) {
    Computer * computer = get_comp(L);
    scrl(lua_tointeger(L, 1));
    refresh();
    return 0;
}

int cli_setCursorPos(lua_State *L) {
    Computer * computer = get_comp(L);
    cursorX = lua_tointeger(L, 1) - 1;
    cursorY = lua_tointeger(L, 2) - 1;
    move(cursorY, cursorX);
    refresh();
    return 0;
}

int cli_setCursorBlink(lua_State *L) {
    Computer * computer = get_comp(L);
    curs_set(lua_toboolean(L, 1));
    refresh();
    return 0;
}

int cli_getCursorPos(lua_State *L) {
    Computer * computer = get_comp(L);
    lua_pushinteger(L, cursorX + 1);
    lua_pushinteger(L, cursorY + 1);
    return 2;
}

int cli_getSize(lua_State *L) {
    Computer * computer = get_comp(L);
    lua_pushinteger(L, TERM_WIDTH);
    lua_pushinteger(L, TERM_HEIGHT);
    return 2;
}

int cli_clear(lua_State *L) {
    Computer * computer = get_comp(L);
    clear();
    refresh();
    return 0;
}

int cli_clearLine(lua_State *L) {
    Computer * computer = get_comp(L);
    move(cursorY, 0);
    clrtoeol();
    refresh();
    return 0;
}

extern int log2i(int num);

int cli_setTextColor(lua_State *L) {
    Computer * computer = get_comp(L);
    int c = colorMap[log2i(lua_tointeger(L, 1))];
    attroff(COLOR_PAIR(colors));
    colors = (colors & 0xf0) | c;
    attron(COLOR_PAIR(colors));
    return 0;
}

int cli_setBackgroundColor(lua_State *L) {
    Computer * computer = get_comp(L);
    int c = colorMap[log2i(lua_tointeger(L, 1))];
    attroff(COLOR_PAIR(colors));
    colors = (colors & 0x0f) | (c << 4);
    attron(COLOR_PAIR(colors));
    return 0;
}

int cli_isColor(lua_State *L) {
    Computer * computer = get_comp(L);
    lua_pushboolean(L, true);
    return 1;
}

int cli_getTextColor(lua_State *L) {
    Computer * computer = get_comp(L);
    lua_pushinteger(L, 1 >> colorMap[colors & 0x0f]);
    return 1;
}

int cli_getBackgroundColor(lua_State *L) {
    Computer * computer = get_comp(L);
    lua_pushinteger(L, 1 >> colorMap[colors >> 4]);
    return 1;
}

extern char htoi(char c);

int cli_blit(lua_State *L) {
    Computer * computer = get_comp(L);
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

int cli_getPaletteColor(lua_State *L) {
    Computer * computer = get_comp(L);
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

int cli_setPaletteColor(lua_State *L) {
    return 0;
}

const char * cli_keys[23] = {
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

lua_CFunction cli_values[23] = {
    cli_write,
    cli_scroll,
    cli_setCursorPos,
    cli_setCursorBlink,
    cli_getCursorPos,
    cli_getSize,
    cli_clear,
    cli_clearLine,
    cli_setTextColor,
    cli_setTextColor,
    cli_setBackgroundColor,
    cli_setBackgroundColor,
    cli_isColor,
    cli_isColor,
    cli_getTextColor,
    cli_getTextColor,
    cli_getBackgroundColor,
    cli_getBackgroundColor,
    cli_blit,
    cli_getPaletteColor,
    cli_getPaletteColor,
    cli_setPaletteColor,
    cli_setPaletteColor
};

library_t cli_lib = {"term", 23, cli_keys, cli_values};*/
#endif