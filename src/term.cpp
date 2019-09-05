/*
 * term.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the term API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "term.hpp"
#include "os.hpp"
#include "config.hpp"
#include "platform.hpp"
#include "TerminalWindow.hpp"
#include "periphemu.hpp"
#include "peripheral/monitor.hpp"
#include <unordered_map>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <chrono>
#include <algorithm>
#include <queue>
#include <tuple>

extern monitor * findMonitorFromWindowID(Computer *comp, int id, std::string& sideReturn);
extern void peripheral_update();
extern bool headless;
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

Uint32 task_event_type;

void termInit() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_SetHint(SDL_HINT_RENDER_DIRECT3D_THREADSAFE, "1");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
    task_event_type = SDL_RegisterEvents(1);
}

void termClose() {
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

int convertX(TerminalWindow * term, int x) {
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

int convertY(TerminalWindow * term, int x) {
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
    Computer * computer = get_comp(L);
    if (!computer->getting_event && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - computer->last_event).count() > config.abortTimeout) {
        printf("Too long without yielding\n");
        computer->last_event = std::chrono::high_resolution_clock::now();
        lua_pushstring(L, "Too long without yielding");
        lua_error(L);
    }
}

void* termRenderLoop(void* arg) {
    Computer * comp = (Computer*)arg;
    while (comp->running == 1) {
        if (!comp->canBlink) comp->term->blink = false;
        else if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - comp->last_blink).count() > 500) {
            comp->term->blink = !comp->term->blink;
            comp->last_blink = std::chrono::high_resolution_clock::now();
        }
        std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        comp->term->render();
        long long count = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
        //printf("Render took %lld ms (%lld fps)\n", count, count == 0 ? 1000 : 1000 / count);
        peripheral_update(comp);
        long t = (1000/config.clockSpeed) - count;
        if (t > 0) std::this_thread::sleep_for(std::chrono::milliseconds(t));
    }
    return NULL;
}

void termQueueProvider(Computer *comp, event_provider p, void* data) {
    comp->event_provider_queue_mutex.lock();
    comp->event_provider_queue.push(std::make_pair(p, data));
    comp->event_provider_queue_mutex.unlock();
    comp->event_lock.notify_all();
}

void gettingEvent(Computer *comp) {comp->getting_event = true;}
void gotEvent(Computer *comp) {comp->last_event = std::chrono::high_resolution_clock::now(); comp->getting_event = false;}

int nextTaskID = 0;
std::queue< std::tuple<int, void*(*)(void*), void*> > taskQueue;
std::unordered_map<int, void*> taskQueueReturns;
bool exiting = false;

void* queueTask(void*(*func)(void*), void* arg) {
    int myID = nextTaskID++;
    taskQueue.push(std::make_tuple(myID, func, arg));
    SDL_Event ev;
    ev.type = task_event_type;
    SDL_PushEvent(&ev);
    while (taskQueueReturns.find(myID) == taskQueueReturns.end() && !exiting) std::this_thread::yield();
    void* retval = taskQueueReturns[myID];
    taskQueueReturns.erase(myID);
    return retval;
}

void mainLoop() {
    SDL_Event e;
    std::string tmps;
    while (computers.size() > 0) {
        if (!headless && SDL_WaitEvent(&e)) { 
            if (e.type == task_event_type) {
                while (taskQueue.size() > 0) {
                    auto v = taskQueue.front();
                    void* retval = std::get<1>(v)(std::get<2>(v));
                    taskQueueReturns[std::get<0>(v)] = retval;
                    taskQueue.pop();
                }
            } else {
                for (Computer * c : computers) {
                    if (((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && (e.key.windowID == c->term->id || findMonitorFromWindowID(c, e.key.windowID, tmps) != NULL)) ||
                        ((e.type == SDL_DROPFILE || e.type == SDL_DROPTEXT || e.type == SDL_DROPBEGIN || e.type == SDL_DROPCOMPLETE) && (e.drop.windowID == c->term->id || findMonitorFromWindowID(c, e.drop.windowID, tmps) != NULL)) ||
                        ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && (e.button.windowID == c->term->id || findMonitorFromWindowID(c, e.button.windowID, tmps) != NULL)) ||
                        (e.type == SDL_MOUSEMOTION && (e.motion.windowID == c->term->id || findMonitorFromWindowID(c, e.motion.windowID, tmps) != NULL)) ||
                        (e.type == SDL_MOUSEWHEEL && (e.wheel.windowID == c->term->id || findMonitorFromWindowID(c, e.wheel.windowID, tmps) != NULL)) ||
                        (e.type == SDL_TEXTINPUT && (e.text.windowID == c->term->id || findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL)) ||
                        (e.type == SDL_WINDOWEVENT && (e.window.windowID == c->term->id || findMonitorFromWindowID(c, e.window.windowID, tmps) != NULL)) ||
                        e.type == SDL_QUIT) {
                        c->termEventQueue.push(e);
                        c->event_lock.notify_all();
                    }
                }
            }
        } else {
            while (taskQueue.size() == 0) std::this_thread::yield();
            while (taskQueue.size() > 0) {
                auto v = taskQueue.front();
                void* retval = std::get<1>(v)(std::get<2>(v));
                taskQueueReturns[std::get<0>(v)] = retval;
                taskQueue.pop();
            }
        }
        std::this_thread::yield();
    }
    exiting = true;
}

// MIGHT DELETE?
int termHasEvent(Computer * computer) {
    if (computer->running != 1) return 0;
    return computer->event_provider_queue.size() + computer->lastResizeEvent + computer->termEventQueue.size();
}

const char * termGetEvent(lua_State *L) {
    Computer * computer = get_comp(L);
    computer->event_provider_queue_mutex.lock();
    if (computer->event_provider_queue.size() > 0) {
        std::pair<event_provider, void*> p = computer->event_provider_queue.front();
        computer->event_provider_queue.pop();
        computer->event_provider_queue_mutex.unlock();
        return p.first(L, p.second);
    }
    computer->event_provider_queue_mutex.unlock();
    if (computer->lastResizeEvent) {
        computer->lastResizeEvent = false;
        return "term_resize";
    }
    if (computer->running != 1) return NULL;
    SDL_Event e;
    if (computer->getEvent(&e)) {
        if (e.type == SDL_QUIT) 
            return "die";
        else if (e.type == SDL_KEYDOWN && keymap.find(e.key.keysym.scancode) != keymap.end()) {
            if (e.key.keysym.scancode == SDL_SCANCODE_F2 && !config.ignoreHotkeys) computer->term->screenshot();
            else if (e.key.keysym.scancode == SDL_SCANCODE_F3 && !config.ignoreHotkeys) computer->term->toggleRecording();
            else if (e.key.keysym.scancode == SDL_SCANCODE_T && (e.key.keysym.mod & KMOD_CTRL)) {
                if (computer->waitingForTerminate == 1) {
                    computer->waitingForTerminate = 2;
                    return "terminate";
                } else if (computer->waitingForTerminate == 0) computer->waitingForTerminate = 1;
            } else if (e.key.keysym.scancode == SDL_SCANCODE_V && (e.key.keysym.mod & KMOD_CTRL) && SDL_HasClipboardText()) {
                char * text = SDL_GetClipboardText();
                lua_pushstring(L, text);
                SDL_free(text);
                return "paste";
            } else {
                computer->waitingForTerminate = 0;
                lua_pushinteger(L, keymap.at(e.key.keysym.scancode));
                lua_pushboolean(L, false);
                return "key";
            }
        } else if (e.type == SDL_KEYUP && keymap.find(e.key.keysym.scancode) != keymap.end()) {
            if (e.key.keysym.scancode != SDL_SCANCODE_F2 || config.ignoreHotkeys) {
                computer->waitingForTerminate = 0;
                lua_pushinteger(L, keymap.at(e.key.keysym.scancode));
                return "key_up";
            }
        } else if (e.type == SDL_TEXTINPUT) {
            char tmp[2];
            tmp[0] = e.text.text[0];
            tmp[1] = 0;
            lua_pushstring(L, tmp);
            return "char";
        } else if (e.type == SDL_MOUSEBUTTONDOWN && computer->config.isColor) {
            lua_pushinteger(L, buttonConvert(e.button.button));
            lua_pushinteger(L, convertX(computer->term, e.button.x));
            lua_pushinteger(L, convertY(computer->term, e.button.y));
            return "mouse_click";
        } else if (e.type == SDL_MOUSEBUTTONUP && computer->config.isColor) {
            lua_pushinteger(L, buttonConvert(e.button.button));
            lua_pushinteger(L, convertX(computer->term, e.button.x));
            lua_pushinteger(L, convertY(computer->term, e.button.y));
            return "mouse_up";
        } else if (e.type == SDL_MOUSEWHEEL && computer->config.isColor) {
            int x = 0, y = 0;
            computer->term->getMouse(&x, &y);
            lua_pushinteger(L, e.wheel.y * (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? 1 : -1));
            lua_pushinteger(L, convertX(computer->term, x));
            lua_pushinteger(L, convertY(computer->term, y));
            return "mouse_scroll";
        } else if (e.type == SDL_MOUSEMOTION && e.motion.state && computer->config.isColor) {
            lua_pushinteger(L, buttonConvert2(e.motion.state));
            lua_pushinteger(L, convertX(computer->term, e.motion.x));
            lua_pushinteger(L, convertY(computer->term, e.motion.y));
            return "mouse_drag";
        } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
            if (e.window.windowID == computer->term->id && computer->term->resize(e.window.data1, e.window.data2)) {
                computer->lastResizeEvent = true;
                return "term_resize";
            } else {
                std::string side;
                monitor * m = findMonitorFromWindowID(computer, e.window.windowID, side);
                if (m != NULL && m->term->resize(e.window.data1, e.window.data2)) {
                    lua_pushstring(L, side.c_str());
                    return "monitor_resize";
                }
            }
        } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {
            if (e.window.windowID == computer->term->id) return "die";
            else {
                std::string side;
                monitor * m = findMonitorFromWindowID(computer, e.window.windowID, side);
                if (m != NULL) {
                    lua_pushstring(L, side.c_str());
                    lua_pop(L, periphemu_lib.values[1](L) + 1);
                }
            }
        }
    }
    return NULL;
}

int headlessCursorX = 1, headlessCursorY = 1;

int term_write(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (headless) {
        printf("%s", lua_tostring(L, 1));
        headlessCursorX += lua_strlen(L, 1);
        return 0;
    }
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    int dummy = 0;
    while (term->locked);
    term->locked = true;
    const char * str = lua_tostring(L, 1);
    #ifdef TESTING
    printf("%s\n", str);
    #endif
    for (int i = 0; i < strlen(str) && term->blinkX < term->width; i++, term->blinkX++) {
        term->screen[term->blinkY][term->blinkX] = str[i];
        term->colors[term->blinkY][term->blinkX] = computer->colors;
    }
    term->locked = false;
    return 0;
}

int term_scroll(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (headless) {
        for (int i = 0; i < lua_tointeger(L, 1); i++) printf("\n");
        return 0;
    }
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    while (term->locked) if (!term->locked) break;
    term->locked = true;
    int lines = lua_tointeger(L, 1);
    for (int i = lines; i < term->height; i++) {
        term->screen[i-lines] = term->screen[i];
        term->colors[i-lines] = term->colors[i];
    }
    for (int i = term->height; i < term->height + lines; i++) {
        term->screen[i-lines] = std::vector<char>(term->width, ' ');
        term->colors[i-lines] = std::vector<unsigned char>(term->width, computer->colors);
    }
    term->locked = false;
    return 0;
}

int term_setCursorPos(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    if (headless) {
        if (lua_tointeger(L, 1) < headlessCursorX) printf("\r");
        else if (lua_tointeger(L, 1) > headlessCursorX) for (int i = headlessCursorX; i < lua_tointeger(L, 1); i++) printf(" ");
        if (lua_tointeger(L, 2) != headlessCursorY) printf("\n");
        headlessCursorX = lua_tointeger(L, 1);
        headlessCursorY = lua_tointeger(L, 2);
        fflush(stdout);
        return 0;
    }
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
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
    get_comp(L)->canBlink = lua_toboolean(L, 1);
    return 0;
}

int term_getCursorPos(lua_State *L) {
    if (headless) {
        lua_pushinteger(L, headlessCursorX);
        lua_pushinteger(L, headlessCursorY);
        return 2;
    }
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    lua_pushinteger(L, term->blinkX + 1);
    lua_pushinteger(L, term->blinkY + 1);
    return 2;
}

int term_getCursorBlink(lua_State *L) {
    lua_pushboolean(L, get_comp(L)->canBlink);
    return 1;
}

int term_getSize(lua_State *L) {
    if (headless) {
        lua_pushinteger(L, 51);
        lua_pushinteger(L, 19);
        return 2;
    }
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    lua_pushinteger(L, term->width);
    lua_pushinteger(L, term->height);
    return 2;
}

int term_clear(lua_State *L) {
    if (headless) {
        for (int i = 0; i < 30; i++) printf("\n");
        return 0;
    }
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    while (term->locked) if (!term->locked) break;
    term->locked = true;
    if (term->isPixel) {
        term->pixels = std::vector<std::vector<char> >(term->height * term->charHeight, std::vector<char>(term->width * term->charWidth, 15));
    } else {
        term->screen = std::vector<std::vector<char> >(term->height, std::vector<char>(term->width, ' '));
        term->colors = std::vector<std::vector<unsigned char> >(term->height, std::vector<unsigned char>(term->width, computer->colors));
    }
    term->locked = false;
    return 0;
}

int term_clearLine(lua_State *L) {
    if (headless) {
        printf("\r");
        for (int i = 0; i < 100; i++) printf(" ");
        printf("\r");
        return 0;
    }
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    while (term->locked) if (!term->locked) break;
    term->locked = true;
    term->screen[term->blinkY] = std::vector<char>(term->width, ' ');
    term->colors[term->blinkY] = std::vector<unsigned char>(term->width, computer->colors);
    term->locked = false;
    return 0;
}

int term_setTextColor(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    Computer * computer = get_comp(L);
    unsigned int c = log2i(lua_tointeger(L, 1));
    if (computer->config.isColor || ((c & 7) - 1) >= 6)
        computer->colors = (computer->colors & 0xf0) | c;
    return 0;
}

int term_setBackgroundColor(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    Computer * computer = get_comp(L);
    unsigned int c = log2i(lua_tointeger(L, 1));
    if (computer->config.isColor || ((c & 7) - 1) >= 6)
        computer->colors = (computer->colors & 0x0f) | (c << 4);
    return 0;
}

int term_isColor(lua_State *L) {
    if (headless) {
        lua_pushboolean(L, false);
        return 1;
    }
    lua_pushboolean(L, get_comp(L)->config.isColor);
    return 1;
}

int term_getTextColor(lua_State *L) {
    lua_pushinteger(L, 1 << (get_comp(L)->colors & 0x0f));
    return 1;
}

int term_getBackgroundColor(lua_State *L) {
    lua_pushinteger(L, 1 << (get_comp(L)->colors >> 4));
    return 1;
}

unsigned char htoi(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    else if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

int term_blit(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    if (!lua_isstring(L, 3)) bad_argument(L, "string", 3);
    if (headless) {
        printf("%s", lua_tostring(L, 1));
        headlessCursorX += lua_strlen(L, 1);
        return 0;
    }
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    while (term->locked) if (!term->locked) break;
    term->locked = true;
    const char * str = lua_tostring(L, 1);
    const char * fg = lua_tostring(L, 2);
    const char * bg = lua_tostring(L, 3);
    for (int i = 0; i < strlen(str) && term->blinkX < term->width; i++, term->blinkX++) {
        if (computer->config.isColor || ((unsigned)(htoi(bg[i]) & 7) - 1) >= 6) 
            computer->colors = htoi(bg[i]) << 4 | (computer->colors & 0xF);
        if (computer->config.isColor || ((unsigned)(htoi(fg[i]) & 7) - 1) >= 6) 
            computer->colors = (computer->colors & 0xF0) | htoi(fg[i]);
        term->screen[term->blinkY][term->blinkX] = str[i];
        term->colors[term->blinkY][term->blinkX] = computer->colors;
    }
    term->locked = false;
    return 0;
}

int term_getPaletteColor(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (headless) {
        if (lua_tointeger(L, 1) == 0x1) {
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
    int color = log2i(lua_tointeger(L, 1));
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
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
    Computer * computer = get_comp(L);
    if (headless || !computer->config.isColor) return 0;
    TerminalWindow * term = computer->term;
    int color = log2i(lua_tointeger(L, 1));
    term->palette[color].r = (int)(lua_tonumber(L, 2) * 255);
    term->palette[color].g = (int)(lua_tonumber(L, 3) * 255);
    term->palette[color].b = (int)(lua_tonumber(L, 4) * 255);
    return 0;
}

int term_setGraphicsMode(lua_State *L) {
    if (!lua_isboolean(L, 1)) bad_argument(L, "boolean", 1);
    if (headless) return 0;
    get_comp(L)->term->isPixel = lua_toboolean(L, 1);
    return 0;
}

int term_getGraphicsMode(lua_State *L) {
    if (headless) {
        lua_pushboolean(L, false);
        return 1;
    }
    lua_pushboolean(L, get_comp(L)->term->isPixel);
    return 1;
}

int term_setPixel(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    if (!lua_isnumber(L, 3)) bad_argument(L, "number", 3);
    if (headless) return 0;
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    if (x >= term->width * 6 || y >= term->height * 9 || x < 0 || y < 0) return 0;
    term->pixels[y][x] = log2i(lua_tointeger(L, 3));
    //printf("Wrote pixel %ld = %d\n", lua_tointeger(L, 3), term->pixels[y][x]);
    return 0;
}

int term_getPixel(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    if (headless) {
        lua_pushinteger(L, 0x8000);
        return 1;
    }
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    if (x > term->width || y > term->height || x < 0 || y < 0) return 0;
    lua_pushinteger(L, 2^term->pixels[lua_tointeger(L, 2)][lua_tointeger(L, 1)]);
    return 1;
}

int term_screenshot(lua_State *L) {
    if (headless) return 0;
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    if (lua_isstring(L, 1)) term->screenshot(lua_tostring(L, 1));
    else term->screenshot();
    return 0;
}

int term_nativePaletteColor(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    Color c = defaultPalette[log2i(lua_tointeger(L, 1))];
    lua_pushnumber(L, c.r / 255.0);
    lua_pushnumber(L, c.g / 255.0);
    lua_pushnumber(L, c.b / 255.0);
    return 3;
}

const char * term_keys[30] = {
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
    "screenshot",
    "nativePaletteColor"
};

lua_CFunction term_values[30] = {
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
    term_getPixel,
    term_screenshot,
    term_nativePaletteColor
};

library_t term_lib = {"term", 30, term_keys, term_values, NULL, NULL};
