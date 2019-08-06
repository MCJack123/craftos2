#include "term.h"
#include "os.h"
#include "config.h"
#include "platform.h"
#include "TerminalWindow.hpp"
#include <unordered_map>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <chrono>
#include <algorithm>
#include <queue>

TerminalWindow * term;
bool canBlink = true;
unsigned char colors = 0xF0;
extern int os_queueEvent(lua_State *L);
extern "C" void peripheral_update();
std::chrono::high_resolution_clock::time_point last_blink = std::chrono::high_resolution_clock::now();
std::chrono::high_resolution_clock::time_point last_event = std::chrono::high_resolution_clock::now();
bool getting_event = false;
const std::unordered_map<int, unsigned char> keymap = {
    {0, 1},
    {SDL_SCANCODE_1, 2},
    {SDL_SCANCODE_2, 3},
    {SDL_SCANCODE_3, 4},
    {SDL_SCANCODE_4, 5},
    {SDL_SCANCODE_5, 6},
    {SDL_SCANCODE_6, 7},
    {SDL_SCANCODE_7, 8},
    {SDL_SCANCODE_8, 9},
    {SDL_SCANCODE_9, 10},
    {SDL_SCANCODE_0, 11},
    {SDL_SCANCODE_MINUS, 12},
    {SDL_SCANCODE_EQUALS, 13},
    {SDL_SCANCODE_BACKSPACE, 14},
    {SDL_SCANCODE_TAB, 15},
    {SDL_SCANCODE_Q, 16},
    {SDL_SCANCODE_W, 17},
    {SDL_SCANCODE_E, 18},
    {SDL_SCANCODE_R, 19},
    {SDL_SCANCODE_T, 20},
    {SDL_SCANCODE_Y, 21},
    {SDL_SCANCODE_U, 22},
    {SDL_SCANCODE_I, 23},
    {SDL_SCANCODE_O, 24},
    {SDL_SCANCODE_P, 25},
    {SDL_SCANCODE_LEFTBRACKET, 26},
    {SDL_SCANCODE_RIGHTBRACKET, 27},
    {SDL_SCANCODE_RETURN, 28},
    {SDL_SCANCODE_LCTRL, 29},
    {SDL_SCANCODE_A, 30},
    {SDL_SCANCODE_S, 31},
    {SDL_SCANCODE_D, 32},
    {SDL_SCANCODE_F, 33},
    {SDL_SCANCODE_G, 34},
    {SDL_SCANCODE_H, 35},
    {SDL_SCANCODE_J, 36},
    {SDL_SCANCODE_K, 37},
    {SDL_SCANCODE_L, 38},
    {SDL_SCANCODE_SEMICOLON, 39},
    {SDL_SCANCODE_APOSTROPHE, 40},
    {SDL_SCANCODE_GRAVE, 41},
    {SDL_SCANCODE_LSHIFT, 42},
    {SDL_SCANCODE_BACKSLASH, 43},
    {SDL_SCANCODE_Z, 44},
    {SDL_SCANCODE_X, 45},
    {SDL_SCANCODE_C, 46},
    {SDL_SCANCODE_V, 47},
    {SDL_SCANCODE_B, 48},
    {SDL_SCANCODE_N, 49},
    {SDL_SCANCODE_M, 50},
    {SDL_SCANCODE_COMMA, 51},
    {SDL_SCANCODE_PERIOD, 52},
    {SDL_SCANCODE_SLASH, 53},
    {SDL_SCANCODE_RSHIFT, 54},
    {SDL_SCANCODE_KP_MULTIPLY, 55},
    {SDL_SCANCODE_LALT, 56},
    {SDL_SCANCODE_SPACE, 57},
    {SDL_SCANCODE_CAPSLOCK, 58},
    {SDL_SCANCODE_F1, 59},
    {SDL_SCANCODE_F2, 60},
    {SDL_SCANCODE_F3, 61},
    {SDL_SCANCODE_F4, 62},
    {SDL_SCANCODE_F5, 63},
    {SDL_SCANCODE_F6, 64},
    {SDL_SCANCODE_F7, 65},
    {SDL_SCANCODE_F8, 66},
    {SDL_SCANCODE_F9, 67},
    {SDL_SCANCODE_F10, 68},
    {SDL_SCANCODE_NUMLOCKCLEAR, 69},
    {SDL_SCANCODE_SCROLLLOCK, 70},
    {SDL_SCANCODE_KP_7, 71},
    {SDL_SCANCODE_KP_8, 72},
    {SDL_SCANCODE_KP_9, 73},
    {SDL_SCANCODE_KP_MINUS, 74},
    {SDL_SCANCODE_KP_4, 75},
    {SDL_SCANCODE_KP_5, 76},
    {SDL_SCANCODE_KP_6, 77},
    {SDL_SCANCODE_KP_PLUS, 78},
    {SDL_SCANCODE_KP_1, 79},
    {SDL_SCANCODE_KP_2, 80},
    {SDL_SCANCODE_KP_3, 81},
    {SDL_SCANCODE_KP_0, 82},
    {SDL_SCANCODE_KP_DECIMAL, 83},
    {SDL_SCANCODE_F11, 87},
    {SDL_SCANCODE_F12, 88},
    {SDL_SCANCODE_F13, 100},
    {SDL_SCANCODE_F14, 101},
    {SDL_SCANCODE_F15, 102},
    {SDL_SCANCODE_KP_EQUALS, 141},
    {SDL_SCANCODE_KP_AT, 145},
    {SDL_SCANCODE_KP_COLON, 146},
    {SDL_SCANCODE_STOP, 149},
    {SDL_SCANCODE_KP_ENTER, 156},
    {SDL_SCANCODE_RCTRL, 157},
    {SDL_SCANCODE_KP_COMMA, 179},
    {SDL_SCANCODE_KP_DIVIDE, 181},
    {SDL_SCANCODE_RALT, 184},
    {SDL_SCANCODE_PAUSE, 197},
    {SDL_SCANCODE_HOME, 199},
    {SDL_SCANCODE_UP, 200},
    {SDL_SCANCODE_PAGEUP, 201},
    {SDL_SCANCODE_LEFT, 203},
    {SDL_SCANCODE_RIGHT, 205},
    {SDL_SCANCODE_END, 207},
    {SDL_SCANCODE_DOWN, 208},
    {SDL_SCANCODE_PAGEDOWN, 209},
    {SDL_SCANCODE_INSERT, 210},
    {SDL_SCANCODE_DELETE, 211}
};

extern "C" {

void termInit() {
    SDL_Init(SDL_INIT_VIDEO);
    term = new TerminalWindow("CraftOS Terminal");
}

void termClose() {
    delete term;
    SDL_Quit();
}

int buttonConvert(Uint8 button) {
    switch (button) {
        case SDL_BUTTON_RIGHT: return 2;
        case SDL_BUTTON_MIDDLE: return 3;
        default: return 1;
    }
}

int buttonConvert2(Uint32 state) {
    if (state & SDL_BUTTON_RMASK) return 2;
    else if (state & SDL_BUTTON_MMASK) return 3;
    else return 1;
}

int convertX(int x) {
    if (term->isPixel) {
        if (x < 2 * term->charScale) return 0;
        else if (x >= term->charWidth * term->width + 2 * term->charScale)
            return TerminalWindow::fontWidth * term->width - 1;
        return (x - (2 * term->charScale)) / term->charScale;
    } else {
        if (x < 2 * term->charScale) x = 2 * term->charScale;
        else if (x > term->charWidth * term->width + 2 * term->charScale)
            x = term->charWidth * term->width + 2 * term->charScale;
        return (x - 2 * term->charScale) / term->charWidth + 1;
    }
}

int convertY(int x) {
    if (term->isPixel) {
        if (x < 2 * term->charScale) return 0;
        else if (x >= term->charHeight * term->height + 2 * term->charScale)
            return TerminalWindow::fontHeight * term->height - 1;
        return (x - (2 * term->charScale)) / term->charScale;
    } else {
        if (x < 2 * term->charScale) x = 2 * term->charScale;
        else if (x > term->charHeight * term->height + 2 * term->charScale)
            x = term->charHeight * term->height + 2 * term->charScale;
        return (x - 2 * term->charScale) / term->charHeight + 1;
    }
}

int log2i(int num) {
    if (num == 0) return 0;
    int retval;
    for (retval = 0; (num & 1) == 0; retval++) num = num >> 1;
    return retval;
}

void termHook(lua_State *L, lua_Debug *ar) {
    if (!getting_event && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - last_event).count() > config.abortTimeout) {
        printf("Too long without yielding\n");
        last_event = std::chrono::high_resolution_clock::now();
        lua_pushstring(L, "Too long without yielding");
        lua_error(L);
    }
}

void* termRenderLoop(void* arg) {
    while (running == 1) {
        if (!canBlink) term->blink = false;
        else if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - last_blink).count() > 500) {
            term->blink = !term->blink;
            last_blink = std::chrono::high_resolution_clock::now();
        }
        std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        term->render();
        //printf("Render took %d ms\n", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count());
        peripheral_update();
        long t = (1000/config.clockSpeed) - std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
        if (t > 0) msleep(t);
    }
    return NULL;
}

std::queue<std::pair<event_provider, void*> > event_provider_queue;
void termQueueProvider(event_provider p, void* data) {event_provider_queue.push(std::make_pair(p, data));}
extern "C" void gettingEvent(void) {getting_event = true;}
extern "C" void gotEvent(void) {last_event = std::chrono::high_resolution_clock::now(); getting_event = false;}

const char * termGetEvent(lua_State *L) {
    if (event_provider_queue.size() > 0) {
        std::pair<event_provider, void*> p = event_provider_queue.front();
        event_provider_queue.pop();
        return p.first(L, p.second);
    }
    SDL_Event e;
    if (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return "die";
        else if (e.type == SDL_KEYDOWN && keymap.find(e.key.keysym.scancode) != keymap.end()) {
            lua_pushinteger(L, keymap.at(e.key.keysym.scancode));
            lua_pushboolean(L, false);
            return "key";
        } else if (e.type == SDL_KEYUP && keymap.find(e.key.keysym.scancode) != keymap.end()) {
            lua_pushinteger(L, keymap.at(e.key.keysym.scancode));
            return "key_up";
        } else if (e.type == SDL_TEXTINPUT) {
            char tmp[2];
            tmp[0] = e.text.text[0];
            tmp[1] = 0;
            lua_pushstring(L, tmp);
            return "char";
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            lua_pushinteger(L, buttonConvert(e.button.button));
            lua_pushinteger(L, convertX(e.button.x));
            lua_pushinteger(L, convertY(e.button.y));
            return "mouse_click";
        } else if (e.type == SDL_MOUSEBUTTONUP) {
            lua_pushinteger(L, buttonConvert(e.button.button));
            lua_pushinteger(L, convertX(e.button.x));
            lua_pushinteger(L, convertY(e.button.y));
            return "mouse_up";
        } else if (e.type == SDL_MOUSEWHEEL) {
            int x = 0, y = 0;
            term->getMouse(&x, &y);
            lua_pushinteger(L, e.button.y);
            lua_pushinteger(L, convertX(x));
            lua_pushinteger(L, convertY(y));
            return "mouse_scroll";
        } else if (e.type == SDL_MOUSEMOTION && e.motion.state) {
            lua_pushinteger(L, buttonConvert2(e.motion.state));
            lua_pushinteger(L, convertX(e.motion.x));
            lua_pushinteger(L, convertY(e.motion.y));
            return "mouse_drag";
        } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
            if (term->resize()) return "term_resize";
        }
    }
    return NULL;
}

int term_write(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    int dummy = 0;
    while (term->locked);
    term->locked = true;
    const char * str = lua_tostring(L, 1);
    #ifdef TESTING
    printf("%s\n", str);
    #endif
    for (int i = 0; i < strlen(str) && term->blinkX < term->width; i++, term->blinkX++) {
        term->screen[term->blinkY][term->blinkX] = str[i];
        term->colors[term->blinkY][term->blinkX] = colors;
    }
    term->locked = false;
    return 0;
}

int term_scroll(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    while (term->locked) if (!term->locked) break;
    term->locked = true;
    int lines = lua_tointeger(L, 1);
    for (int i = lines; i < term->height; i++) {
        term->screen[i-lines] = term->screen[i];
        term->colors[i-lines] = term->colors[i];
    }
    for (int i = term->height; i < term->height + lines; i++) {
        term->screen[i-lines] = std::vector<char>(term->width, ' ');
        term->colors[i-lines] = std::vector<unsigned char>(term->width, 0);
    }
    term->locked = false;
    return 0;
}

int term_setCursorPos(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    while (term->locked);
    term->locked = true;
    term->blinkX = lua_tointeger(L, 1) - 1;
    term->blinkY = lua_tointeger(L, 2) - 1;
    if (term->blinkX >= term->width) term->blinkX = term->width - 1;
    if (term->blinkY >= term->height) term->blinkY = term->height - 1;
    term->locked = false;
    return 0;
}

int term_setCursorBlink(lua_State *L) {
    if (!lua_isboolean(L, 1)) bad_argument(L, "boolean", 1);
    canBlink = lua_toboolean(L, 1);
    return 0;
}

int term_getCursorPos(lua_State *L) {
    lua_pushinteger(L, term->blinkX + 1);
    lua_pushinteger(L, term->blinkY + 1);
    return 2;
}

int term_getCursorBlink(lua_State *L) {
    lua_pushboolean(L, canBlink);
    return 1;
}

int term_getSize(lua_State *L) {
    lua_pushinteger(L, term->width);
    lua_pushinteger(L, term->height);
    return 2;
}

int term_clear(lua_State *L) {
    while (term->locked) if (!term->locked) break;
    term->locked = true;
    term->screen = std::vector<std::vector<char> >(term->height, std::vector<char>(term->width, ' '));
    term->colors = std::vector<std::vector<unsigned char> >(term->height, std::vector<unsigned char>(term->width, colors));
    term->locked = false;
    return 0;
}

int term_clearLine(lua_State *L) {
    while (term->locked) if (!term->locked) break;
    term->locked = true;
    term->screen[term->blinkY] = std::vector<char>(term->width, ' ');
    term->colors[term->blinkY] = std::vector<unsigned char>(term->width, colors);
    term->locked = false;
    return 0;
}

int term_setTextColor(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    int c = log2i(lua_tointeger(L, 1));
    colors = (colors & 0xf0) | c;
    return 0;
}

int term_setBackgroundColor(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    int c = log2i(lua_tointeger(L, 1));
    colors = (colors & 0x0f) | (c << 4);
    return 0;
}

int term_isColor(lua_State *L) {
    lua_pushboolean(L, true);
    return 1;
}

int term_getTextColor(lua_State *L) {
    lua_pushinteger(L, 1 << (colors & 0x0f));
    return 1;
}

int term_getBackgroundColor(lua_State *L) {
    lua_pushinteger(L, 1 << (colors >> 4));
    return 1;
}

char htoi(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    else if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

int term_blit(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    if (!lua_isstring(L, 3)) bad_argument(L, "string", 3);
    while (term->locked) if (!term->locked) break;
    term->locked = true;
    const char * str = lua_tostring(L, 1);
    const char * fg = lua_tostring(L, 2);
    const char * bg = lua_tostring(L, 3);
    for (int i = 0; i < strlen(str) && term->blinkX < term->width; i++, term->blinkX++) {
        colors = htoi(bg[i]) << 4 | htoi(fg[i]);
        term->screen[term->blinkY][term->blinkX] = str[i];
        term->colors[term->blinkY][term->blinkX] = colors;
    }
    term->locked = false;
    return 0;
}

int term_getPaletteColor(lua_State *L) {
    int color = log2i(lua_tointeger(L, 1));
    lua_pushnumber(L, term->palette[color].r/255.0);
    lua_pushnumber(L, term->palette[color].g/255.0);
    lua_pushnumber(L, term->palette[color].b/255.0);
    return 3;
}

int term_setPaletteColor(lua_State *L) {
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

int term_setGraphicsMode(lua_State *L) {
    if (!lua_isboolean(L, 1)) bad_argument(L, "boolean", 1);
    term->isPixel = lua_toboolean(L, 1);
    return 0;
}

int term_getGraphicsMode(lua_State *L) {
    lua_pushboolean(L, term->isPixel);
    return 1;
}

int term_setPixel(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    if (!lua_isnumber(L, 3)) bad_argument(L, "number", 3);
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    if (x >= term->width * 6 || y >= term->height * 9 || x < 0 || y < 0) return 0;
    term->pixels[y][x] = log2i(lua_tointeger(L, 3));
    printf("Wrote pixel %ld = %d\n", lua_tointeger(L, 3), term->pixels[y][x]);
    return 0;
}

int term_getPixel(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    if (x > term->width || y > term->height || x < 0 || y < 0) return 0;
    lua_pushinteger(L, 2^term->pixels[lua_tointeger(L, 2)][lua_tointeger(L, 1)]);
    return 1;
}

const char * term_keys[28] = {
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
    "getPixel"
};

lua_CFunction term_values[28] = {
    term_write,
    term_scroll,
    term_setCursorPos,
    term_setCursorBlink,
    term_getCursorPos,
    term_getCursorBlink,
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
    term_setPaletteColor,
    term_setGraphicsMode,
    term_getGraphicsMode,
    term_setPixel,
    term_getPixel
};

library_t term_lib = {"term", 28, term_keys, term_values, termInit, termClose};
}