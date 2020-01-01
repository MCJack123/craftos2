/*
 * term.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the term API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2020 JackMacWindows.
 */

#include "term.hpp"
#include "os.hpp"
#include "config.hpp"
#include "platform.hpp"
#include "TerminalWindow.hpp"
#ifndef NO_CLI
#include "CLITerminalWindow.hpp"
#endif
#include "periphemu.hpp"
#include "peripheral/monitor.hpp"
#include "peripheral/debugger.hpp"
#include <unordered_map>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <chrono>
#include <algorithm>
#include <queue>
#include <tuple>
#include <cassert>

extern monitor * findMonitorFromWindowID(Computer *comp, unsigned id, std::string& sideReturn);
extern void peripheral_update();
extern bool headless;
extern bool cli;
extern bool exiting;
std::thread * renderThread;
std::unordered_map<int, unsigned char> keymap = {
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
#ifndef NO_CLI
std::unordered_map<int, unsigned char> keymap_cli = {
    {'1', 2},
    {'2', 3},
    {'3', 4},
    {'4', 5},
    {'5', 6},
    {'6', 7},
    {'7', 8},
    {'8', 9},
    {'9', 1},
    {'0', 11},
    {'-', 12},
    {'=', 13},
    {KEY_BACKSPACE, 14},
    {8, 14},
    {0x7F, 14},
    {'\t', 15},
    {'q', 16},
    {'w', 17},
    {'e', 18},
    {'r', 19},
    {'t', 20},
    {'y', 21},
    {'u', 22},
    {'i', 23},
    {'o', 24},
    {'p', 25},
    {'[', 26},
    {']', 27},
    {'\n', 28},
    {'a', 30},
    {'s', 31},
    {'d', 32},
    {'f', 33},
    {'g', 34},
    {'h', 35},
    {'j', 36},
    {'k', 37},
    {'l', 38},
    {';', 39},
    {'\'', 40},
    {'\\', 43},
    {'z', 44},
    {'x', 45},
    {'c', 46},
    {'v', 47},
    {'b', 48},
    {'n', 49},
    {'m', 50},
    {',', 51},
    {'.', 52},
    {'/', 53},
    {' ', 57},
    {KEY_F(1), 59},
    {KEY_F(2), 60},
    {KEY_F(3), 61},
    {KEY_F(4), 62},
    {KEY_F(5), 63},
    {KEY_F(6), 64},
    {KEY_F(7), 65},
    {KEY_F(8), 66},
    {KEY_F(9), 67},
    {KEY_F(10), 68},
    {KEY_F(11), 87},
    {KEY_F(12), 88},
    {KEY_F(13), 100},
    {KEY_F(14), 101},
    {KEY_F(15), 102},
    {KEY_UP, 200},
    {KEY_LEFT, 203},
    {KEY_RIGHT, 205},
    {KEY_DOWN, 208},
    {KEY_SHOME, 199},
    {KEY_SEND, 207},
    {KEY_HOME, 29},
    {KEY_END, 56}
};
#endif

Uint32 task_event_type;
Uint32 render_event_type;

void termRenderLoop();

void termInit() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_SetHint(SDL_HINT_RENDER_DIRECT3D_THREADSAFE, "1");
    SDL_SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1");
#if SDL_VERSION_ATLEAST(2, 0, 8)
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
    task_event_type = SDL_RegisterEvents(2);
    render_event_type = task_event_type + 1;
    renderThread = new std::thread(termRenderLoop);
}

void termClose() {
    renderThread->join();
    delete renderThread;
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
    if (term->mode != 0) {
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
    if (term->mode != 0) {
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

extern library_t * libraries[9];
int termPanic(lua_State *L) {
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "traceback");
    //lua_call(L, 0, 1);
    //printf("%s\n", lua_tostring(L, -1));
    lua_pop(L, 1);
    lua_Debug ar;
    lua_getstack(L, 1, &ar);
    lua_getinfo(L, "nSl", &ar);
    Computer * comp = get_comp(L);
    comp->running = 0;
    #ifndef NO_CLI
    if (cli)
        fprintf(stderr, "An unexpected error occurred in a Lua function: %s:%s:%d: %s\n", ar.short_src, ar.name == NULL ? "(null)" : ar.name, ar.currentline, lua_tostring(L, 1));
    else
    #endif
        queueTask([ar, comp](void* L)->void*{SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Lua Panic", ("An unexpected error occurred in a Lua function: " + std::string(ar.short_src) + ":" + std::string(ar.name == NULL ? "(null)" : ar.name) + ":" + std::to_string(ar.currentline) + ": " + std::string(!lua_isstring((lua_State*)L, 1) ? "(null)" : lua_tostring((lua_State*)L, 1)) + ". The computer must now shut down.").c_str(), comp->term->win); return NULL;}, L);
    comp->event_lock.notify_all();
    for (unsigned i = 0; i < sizeof(libraries) / sizeof(library_t*); i++) 
        if (libraries[i]->deinit != NULL) libraries[i]->deinit(comp);
    lua_close(comp->L);   /* Cya, Lua */
    comp->L = NULL;
    longjmp(comp->on_panic, 0);
}

extern "C" {extern void luaL_where (lua_State *L, int level);}

bool debuggerBreak(lua_State *L, Computer * computer, debugger * dbg, const char * reason) {
    bool lastBlink = computer->term->canBlink;
    computer->term->canBlink = false;
    dbg->thread = L;
    dbg->breakReason = reason;
    while (!dbg->didBreak) {
        std::lock_guard<std::mutex> guard(dbg->breakMutex);
        dbg->breakNotify.notify_all();
        std::this_thread::yield();
    }
    assert(dbg->didBreak);
    std::unique_lock<std::mutex> lock(dbg->breakMutex);
    while (dbg->didBreak) dbg->breakNotify.wait_for(lock, std::chrono::milliseconds(500));
    bool retval = !dbg->running;
    dbg->thread = NULL;
    computer->last_event = std::chrono::high_resolution_clock::now();
    computer->term->canBlink = lastBlink;
    return retval;
}

void noDebuggerBreak(lua_State *L, Computer * computer, lua_Debug * ar) {
    lua_State *coro = lua_newthread(L);
    lua_getglobal(coro, "os");
    lua_getfield(coro, -1, "run");
    lua_newtable(coro);
    lua_pushstring(coro, "locals");
    lua_newtable(coro);
    const char * name;
    for (int i = 1; (name = lua_getlocal(L, ar, i)) != NULL; i++) {
        if (std::string(name) == "(*temporary)") {
            lua_pop(L, 1);
            continue;
        }
        lua_pushstring(coro, name);
        lua_xmove(L, coro, 1);
        lua_settable(coro, -3);
    }
    lua_settable(coro, -3);
    lua_newtable(coro);
    lua_pushstring(coro, "__index");
    lua_getfenv(L, -2);
    lua_xmove(L, coro, 1);
    lua_settable(coro, -3);
    lua_setmetatable(coro, -2);
    lua_pushstring(coro, "/rom/programs/lua.lua");
    int status = lua_resume(coro, 2);
    int narg;
    while (status == LUA_YIELD) {
        if (lua_isstring(coro, -1)) narg = getNextEvent(coro, std::string(lua_tostring(coro, -1), lua_strlen(coro, -1)));
        else narg = getNextEvent(coro, "");
        status = lua_resume(coro, narg);
    }
    lua_pop(L, 1);
    computer->last_event = std::chrono::high_resolution_clock::now();
}

extern "C" {
    int db_debug(lua_State *L) {
        Computer * comp = get_comp(L);
        if (!comp->isDebugger && comp->debugger != NULL) debuggerBreak(L, comp, (debugger*)comp->debugger, "debug.debug() called");
        else {
            lua_Debug ar;
            lua_getstack(L, 1, &ar);
            noDebuggerBreak(L, comp, &ar);
        }
        return 0;
    }
}

extern "C" {extern const char KEY_HOOK;}

void termHook(lua_State *L, lua_Debug *ar) {
    Computer * computer = get_comp(L);
    if (ar->event == LUA_HOOKCOUNT) {
        if (!computer->getting_event && !(!computer->isDebugger && computer->debugger != NULL && ((debugger*)computer->debugger)->thread != NULL) && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - computer->last_event).count() > config.abortTimeout) {
            computer->last_event = std::chrono::high_resolution_clock::now();
            luaL_where(L, 1);
            lua_pushstring(L, "Too long without yielding");
            lua_concat(L, 2);
            printf("%s\n", lua_tostring(L, -1));
            lua_error(L);
        }
    } else if (ar->event == LUA_HOOKLINE) {
        if (::config.debug_enable && computer->debugger == NULL && computer->breakpoints.size() > 0) {
            lua_getinfo(L, "Sl", ar);
            for (std::pair<int, std::pair<std::string, lua_Integer> > b : computer->breakpoints) {
                if (b.second.first == std::string(ar->source) && b.second.second == ar->currentline) {
                    noDebuggerBreak(L, computer, ar);
                }
            }
        } else if (!computer->isDebugger && computer->debugger != NULL) {
            debugger * dbg = (debugger*)computer->debugger;
            if (dbg->thread == NULL) {
                if (dbg->breakType == DEBUGGER_BREAK_TYPE_LINE) {
                    if (dbg->stepCount >= 0) {dbg->stepCount = 0; debuggerBreak(L, computer, dbg, "Pause");}
                    else dbg->stepCount--;
                } else if (computer->breakpoints.size() > 0) {
                    lua_getinfo(L, "Sl", ar);
                    for (std::pair<int, std::pair<std::string, lua_Integer> > b : computer->breakpoints)
                        if (b.second.first == std::string(ar->source) && b.second.second == ar->currentline) 
                            if (debuggerBreak(L, computer, dbg, "Breakpoint")) return;
                }
            }
        }
    } else if (!computer->isDebugger && computer->debugger != NULL && (ar->event == LUA_HOOKRET || ar->event == LUA_HOOKTAILRET)) {
        debugger * dbg = (debugger*)computer->debugger;
        if (dbg->breakType == DEBUGGER_BREAK_TYPE_RETURN && dbg->thread == NULL && debuggerBreak(L, computer, dbg, "Pause")) return;
        if (dbg->isProfiling) {
            lua_getinfo(L, "nS", ar);
            if (ar->source != NULL && ar->name != NULL && dbg->profile.find(ar->source) != dbg->profile.end() && dbg->profile[ar->source].find(ar->name) != dbg->profile[ar->source].end()) {
                dbg->profile[ar->source][ar->name].time += (std::chrono::high_resolution_clock::now() - dbg->profile[ar->source][ar->name].start);
                dbg->profile[ar->source][ar->name].running = false;
            }
        }
    } else if (!computer->isDebugger && computer->debugger != NULL && ar->event == LUA_HOOKCALL && ar->source != NULL && ar->name != NULL) {
        debugger * dbg = (debugger*)computer->debugger;
        if (dbg->thread == NULL) {
            lua_getinfo(L, "nS", ar);
            if (ar->name != NULL && ((((std::string(ar->name) == "loadAPI" && std::string(ar->source).find("bios.lua") != std::string::npos) || std::string(ar->name) == "require") && (dbg->breakMask & DEBUGGER_BREAK_FUNC_LOAD)) ||
                (((std::string(ar->name) == "run" && std::string(ar->source).find("bios.lua") != std::string::npos) || (std::string(ar->name) == "dofile" && std::string(ar->source).find("bios.lua") != std::string::npos)) && (dbg->breakMask & DEBUGGER_BREAK_FUNC_RUN)))) if (debuggerBreak(L, computer, dbg, "Caught call")) return;
        }
        if (dbg->isProfiling) {
            lua_getinfo(L, "nS", ar);
            if (dbg->profile.find(ar->source) == dbg->profile.end()) dbg->profile[ar->source] = {};
            if (dbg->profile[ar->source].find(ar->name) == dbg->profile[ar->source].end()) dbg->profile[ar->source][ar->name] = {true, 1, std::chrono::high_resolution_clock::now(), std::chrono::microseconds(0)};
            else {
                if (dbg->profile[ar->source][ar->name].running) {
                    //printf("Function %s:%s skipped return for %d ms\n", ar->source, ar->name, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - dbg->profile[ar->source][ar->name].start).count());
                    dbg->profile[ar->source][ar->name].time += (std::chrono::high_resolution_clock::now() - dbg->profile[ar->source][ar->name].start);
                    dbg->profile[ar->source][ar->name].running = false;
                }
                dbg->profile[ar->source][ar->name].running = true;
                dbg->profile[ar->source][ar->name].count++;
                dbg->profile[ar->source][ar->name].start = std::chrono::high_resolution_clock::now();
            }
        }
    } else if (ar->event == LUA_HOOKERROR) {
        if (config.logErrors) printf("Got error: %s\n", lua_tostring(L, -2));
        if (!computer->isDebugger && computer->debugger != NULL) {
            debugger * dbg = (debugger*)computer->debugger;
            if (dbg->thread == NULL && (dbg->breakMask & DEBUGGER_BREAK_FUNC_ERROR)) 
                if (debuggerBreak(L, computer, dbg, lua_tostring(L, -2) == NULL ? "Error" : lua_tostring(L, -2))) return;
        }
    } else if (ar->event == LUA_HOOKRESUME && !computer->isDebugger && computer->debugger != NULL) {
        debugger * dbg = (debugger*)computer->debugger;
        if (dbg->thread == NULL && (dbg->breakMask & DEBUGGER_BREAK_FUNC_RESUME)) 
            if (debuggerBreak(L, computer, dbg, "Resume")) return;
    } else if (ar->event == LUA_HOOKYIELD && !computer->isDebugger && computer->debugger != NULL) {
        debugger * dbg = (debugger*)computer->debugger;
        if (dbg->thread == NULL && (dbg->breakMask & DEBUGGER_BREAK_FUNC_YIELD)) 
            if (debuggerBreak(L, computer, dbg, "Yield")) return;
    }
    if (ar->event != LUA_HOOKCOUNT && (computer->hookMask & (1 << ar->event))) {
        lua_pushlightuserdata(L, (void*)&KEY_HOOK);
        lua_gettable(L, LUA_REGISTRYINDEX);
        if (lua_istable(L, -1)) {
            lua_pushlightuserdata(L, L);
            lua_gettable(L, -2);
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "mask");
                if (lua_tointeger(L, -1) & (1 << ar->event)) {
                    lua_pop(L, 1);
                    lua_getfield(L, -1, "func");
                    if (lua_isfunction(L, -1)) {
                        static const char *const hooknames[] = {"call", "return", "line", "count", "tail return", "error", "resume", "yield"};
                        lua_pushstring(L, hooknames[ar->event]);
                        if (ar->event == LUA_HOOKLINE) {
                            lua_getinfo(L, "l", ar);
                            lua_pushinteger(L, ar->currentline);
                        }
                        else lua_pushnil(L);
                        lua_call(L, 2, 0);
                    } else lua_pop(L, 1);
                } else lua_pop(L, 1);
            }
            lua_pop(L, 1);
        } else lua_pop(L, 1);
    }
}

void termRenderLoop() {
#ifdef __APPLE__
    pthread_setname_np("Render Thread");
#endif
    while (!exiting) {
        std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        bool pushEvent = false;
        TerminalWindow::renderTargetsLock.lock();
        for (TerminalWindow* term : TerminalWindow::renderTargets) {
            if (!term->canBlink) term->blink = false;
            else if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - term->last_blink).count() > 500) {
                term->blink = !term->blink;
                term->last_blink = std::chrono::high_resolution_clock::now();
                term->changed = true;
            }
            pushEvent = pushEvent || term->changed;
            term->render();
        }
        TerminalWindow::renderTargetsLock.unlock();
        if (pushEvent) {
            SDL_Event ev;
            ev.type = render_event_type;
            SDL_PushEvent(&ev);
            long long count = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
            //printf("Render took %lld ms (%lld fps)\n", count, count == 0 ? 1000 : 1000 / count);
            long t = (1000/config.clockSpeed) - count;
            if (t > 0) std::this_thread::sleep_for(std::chrono::milliseconds(t));
        } else {
            int time = 1000/config.clockSpeed;
            std::chrono::milliseconds ms(time);
            std::this_thread::sleep_for(ms);
        }
    }
}

void termQueueProvider(Computer *comp, event_provider p, void* data) {
    comp->event_provider_queue_mutex.lock();
    comp->event_provider_queue.push(std::make_pair(p, data));
    comp->event_provider_queue_mutex.unlock();
    comp->event_lock.notify_all();
}

void gettingEvent(Computer *comp) {comp->getting_event = true;}
void gotEvent(Computer *comp) {comp->last_event = std::chrono::high_resolution_clock::now(); comp->getting_event = false;}

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
    std::string tmpstrval;
    if (computer->getEvent(&e)) {
        if (e.type == SDL_QUIT) 
            return "die";
        else if (e.type == SDL_KEYDOWN && (cli || keymap.find(e.key.keysym.scancode) != keymap.end())) {
            TerminalWindow * term = e.key.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.key.windowID, tmpstrval)->term;
            if (e.key.keysym.scancode == SDL_SCANCODE_F2 && e.key.keysym.mod == 0 && !config.ignoreHotkeys) term->screenshot();
            else if (e.key.keysym.scancode == SDL_SCANCODE_F3 && e.key.keysym.mod == 0 && !config.ignoreHotkeys) term->toggleRecording();
            else if (e.key.keysym.scancode == SDL_SCANCODE_F11 && e.key.keysym.mod == 0 && !config.ignoreHotkeys) term->toggleFullscreen();
            else if (e.key.keysym.scancode == SDL_SCANCODE_F12 && e.key.keysym.mod == 0 && !config.ignoreHotkeys) term->screenshot("clipboard");
            else if (e.key.keysym.scancode == SDL_SCANCODE_T && (e.key.keysym.mod & KMOD_CTRL)) {
                if (computer->waitingForTerminate & 1) {
                    computer->waitingForTerminate |= 2;
                    computer->waitingForTerminate &= ~1;
                    return "terminate";
                } else if ((computer->waitingForTerminate & 3) == 0) computer->waitingForTerminate |= 1;
            } else if (e.key.keysym.scancode == SDL_SCANCODE_S && (e.key.keysym.mod & KMOD_CTRL)) {
                if (computer->waitingForTerminate & 4) {
                    computer->waitingForTerminate |= 8;
                    computer->waitingForTerminate &= ~4;
                    computer->running = 0;
                    return "terminate";
                } else if ((computer->waitingForTerminate & 12) == 0) computer->waitingForTerminate |= 4;
            } else if (e.key.keysym.scancode == SDL_SCANCODE_R && (e.key.keysym.mod & KMOD_CTRL)) {
                if (computer->waitingForTerminate & 16) {
                    computer->waitingForTerminate |= 32;
                    computer->waitingForTerminate &= ~16;
                    computer->running = 2;
                    return "terminate";
                } else if ((computer->waitingForTerminate & 48) == 0) computer->waitingForTerminate |= 16;
            } else if (e.key.keysym.scancode == SDL_SCANCODE_V && 
#ifdef __APPLE__
              (e.key.keysym.mod & KMOD_GUI) &&
#else
              (e.key.keysym.mod & KMOD_CTRL) &&
#endif
              SDL_HasClipboardText()) {
                char * text = SDL_GetClipboardText();
                lua_pushstring(L, text);
                SDL_free(text);
                return "paste";
            } else {
                computer->waitingForTerminate = 0;
#ifndef NO_CLI
                if (cli) lua_pushinteger(L, e.key.keysym.scancode); 
                else 
#endif
                lua_pushinteger(L, keymap.at(e.key.keysym.scancode));
                lua_pushboolean(L, false);
                return "key";
            }
        } else if (e.type == SDL_KEYUP && (cli || keymap.find(e.key.keysym.scancode) != keymap.end())) {
            if (e.key.keysym.scancode != SDL_SCANCODE_F2 || config.ignoreHotkeys) {
                computer->waitingForTerminate = 0;
#ifndef NO_CLI
                if (cli) lua_pushinteger(L, e.key.keysym.scancode); 
                else 
#endif
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
            TerminalWindow * term = e.button.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.button.windowID, tmpstrval)->term;
            lua_pushinteger(L, buttonConvert(e.button.button));
            if (cli) {
                lua_pushinteger(L, e.button.x);
                lua_pushinteger(L, e.button.y);
            } else {
                lua_pushinteger(L, convertX(term, e.button.x));
                lua_pushinteger(L, convertY(term, e.button.y));
            }
            return "mouse_click";
        } else if (e.type == SDL_MOUSEBUTTONUP && computer->config.isColor) {
            TerminalWindow * term = e.button.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.button.windowID, tmpstrval)->term;
            lua_pushinteger(L, buttonConvert(e.button.button));
            if (cli) {
                lua_pushinteger(L, e.button.x);
                lua_pushinteger(L, e.button.y);
            } else {
                lua_pushinteger(L, convertX(term, e.button.x));
                lua_pushinteger(L, convertY(term, e.button.y));
            }
            return "mouse_up";
        } else if (e.type == SDL_MOUSEWHEEL && computer->config.isColor) {
            TerminalWindow * term = e.button.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.button.windowID, tmpstrval)->term;
            int x = 0, y = 0;
            term->getMouse(&x, &y);
            lua_pushinteger(L, max(min(e.wheel.y * (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? 1 : -1), 1), -1));
            lua_pushinteger(L, convertX(term, x));
            lua_pushinteger(L, convertY(term, y));
            return "mouse_scroll";
        } else if (e.type == SDL_MOUSEMOTION && e.motion.state && computer->config.isColor) {
            TerminalWindow * term = e.button.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.button.windowID, tmpstrval)->term;
            lua_pushinteger(L, buttonConvert2(e.motion.state));
            lua_pushinteger(L, convertX(term, e.motion.x));
            lua_pushinteger(L, convertY(term, e.motion.y));
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
    std::lock_guard<std::mutex> locked_g(term->locked);
    size_t str_sz = 0;
    const char * str = lua_tolstring(L, 1, &str_sz);
    #ifdef TESTING
    printf("%s\n", str);
    #endif
    for (unsigned i = 0; i < str_sz && term->blinkX < term->width; i++, term->blinkX++) {
        term->screen[term->blinkY][term->blinkX] = str[i];
        term->colors[term->blinkY][term->blinkX] = computer->colors;
    }
    term->changed = true;
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
    std::lock_guard<std::mutex> locked_g(term->locked);
    int lines = lua_tointeger(L, 1);
    for (int i = lines; i < term->height; i++) {
        term->screen[i-lines] = term->screen[i];
        term->colors[i-lines] = term->colors[i];
    }
    for (int i = term->height; i < term->height + lines; i++) {
        term->screen[i-lines] = std::vector<unsigned char>(term->width, ' ');
        term->colors[i-lines] = std::vector<unsigned char>(term->width, computer->colors);
    }
    term->changed = true;
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
    std::lock_guard<std::mutex> locked_g(term->locked);
    term->blinkX = max(0, min((int)lua_tointeger(L, 1) - 1, term->width - 1));
    term->blinkY = max(0, min((int)lua_tointeger(L, 2) - 1, term->height - 1));
    term->changed = true;
    return 0;
}

bool can_blink_headless = true;

int term_setCursorBlink(lua_State *L) {
    if (!lua_isboolean(L, 1)) bad_argument(L, "boolean", 1);
    if (!headless) {
        get_comp(L)->term->canBlink = lua_toboolean(L, 1);
        get_comp(L)->term->changed = true;
    } else can_blink_headless = lua_toboolean(L, 1);
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
    if (headless) lua_pushboolean(L, can_blink_headless);
    else lua_pushboolean(L, get_comp(L)->term->canBlink);
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
    std::lock_guard<std::mutex> locked_g(term->locked);
    if (term->mode > 0) {
        term->pixels = vector2d<unsigned char>(term->width * TerminalWindow::fontWidth, term->height * TerminalWindow::fontHeight, 0x0F);
    } else {
        term->screen = vector2d<unsigned char>(term->width, term->height, ' ');
        term->colors = vector2d<unsigned char>(term->width, term->height, 0xF0);
    }
    term->changed = true;
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
    std::lock_guard<std::mutex> locked_g(term->locked);
    term->screen[term->blinkY] = std::vector<unsigned char>(term->width, ' ');
    term->colors[term->blinkY] = std::vector<unsigned char>(term->width, computer->colors);
    term->changed = true;
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
    size_t str_sz, fg_sz, bg_sz;
    const char * str = lua_tolstring(L, 1, &str_sz);
    const char * fg = lua_tolstring(L, 2, &fg_sz);
    const char * bg = lua_tolstring(L, 3, &bg_sz);
    if (str_sz != fg_sz || fg_sz != bg_sz) {
        lua_pushstring(L, "Arguments must be the same length");
        lua_error(L);
    }
    std::lock_guard<std::mutex> locked_g(term->locked);
    for (unsigned i = 0; i < str_sz && term->blinkX < term->width; i++, term->blinkX++) {
        if (computer->config.isColor || ((unsigned)(htoi(bg[i]) & 7) - 1) >= 6) 
            computer->colors = htoi(bg[i]) << 4 | (computer->colors & 0xF);
        if (computer->config.isColor || ((unsigned)(htoi(fg[i]) & 7) - 1) >= 6) 
            computer->colors = (computer->colors & 0xF0) | htoi(fg[i]);
        term->screen[term->blinkY][term->blinkX] = str[i];
        term->colors[term->blinkY][term->blinkX] = computer->colors;
    }
    term->changed = true;
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
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    int color;
    if (term->mode == 2) color = lua_tointeger(L, 1);
    else color = log2i(lua_tointeger(L, 1));
    lua_pushnumber(L, term->palette[color].r/255.0);
    lua_pushnumber(L, term->palette[color].g/255.0);
    lua_pushnumber(L, term->palette[color].b/255.0);
    return 3;
}

int term_setPaletteColor(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    if (!lua_isnoneornil(L, 3)) {
        if (!lua_isnumber(L, 3)) bad_argument(L, "number", 3);
        if (!lua_isnumber(L, 4)) bad_argument(L, "number", 4);
    }
    Computer * computer = get_comp(L);
    if (headless || !computer->config.isColor) return 0;
    TerminalWindow * term = computer->term;
    int color;
    if (term->mode == 2) color = lua_tointeger(L, 1);
    else color = log2i(lua_tointeger(L, 1));
    if (lua_isnoneornil(L, 3)) {
        unsigned int rgb = lua_tointeger(L, 2);
        term->palette[color].r = rgb >> 16 & 0xFF;
        term->palette[color].g = rgb >> 8 & 0xFF;
        term->palette[color].b = rgb & 0xFF;
    } else {
        term->palette[color].r = (int)(lua_tonumber(L, 2) * 255);
        term->palette[color].g = (int)(lua_tonumber(L, 3) * 255);
        term->palette[color].b = (int)(lua_tonumber(L, 4) * 255);
    }
    term->changed = true;
    //printf("%d -> %d, %d, %d\n", color, term->palette[color].r, term->palette[color].g, term->palette[color].b);
    return 0;
}

int term_setGraphicsMode(lua_State *L) {
    if (!lua_isboolean(L, 1) && !lua_isnumber(L, 1)) bad_argument(L, "boolean or number", 1);
    if (headless || cli || !get_comp(L)->config.isColor) return 0;
    get_comp(L)->term->mode = lua_isboolean(L, 1) ? lua_toboolean(L, 1) : lua_tointeger(L, 1);
    get_comp(L)->term->changed = true;
    return 0;
}

int term_getGraphicsMode(lua_State *L) {
    if (headless || cli || !get_comp(L)->config.isColor) {
        lua_pushboolean(L, false);
        return 1;
    }
    if (get_comp(L)->term->mode == 0) lua_pushboolean(L, false);
    else lua_pushinteger(L, get_comp(L)->term->mode);
    return 1;
}

int term_setPixel(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    if (!lua_isnumber(L, 3)) bad_argument(L, "number", 3);
    if (headless || cli) return 0;
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    std::lock_guard<std::mutex> lock(term->locked);
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    if (x >= term->width * 6 || y >= term->height * 9 || x < 0 || y < 0) return 0;
    if (term->mode == 1) term->pixels[y][x] = log2i(lua_tointeger(L, 3));
    else if (term->mode == 2) term->pixels[y][x] = lua_tointeger(L, 3);
    term->changed = true;
    //printf("Wrote pixel %ld = %d\n", lua_tointeger(L, 3), term->pixels[y][x]);
    return 0;
}

int term_getPixel(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    if (headless || cli) {
        lua_pushinteger(L, 0x8000);
        return 1;
    }
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    if (x > term->width || y > term->height || x < 0 || y < 0) return 0;
    if (term->mode == 1) lua_pushinteger(L, 2^term->pixels[lua_tointeger(L, 2)][lua_tointeger(L, 1)]);
    else if (term->mode == 2) lua_pushinteger(L, term->pixels[lua_tointeger(L, 2)][lua_tointeger(L, 1)]);
    return 1;
}

int term_drawPixels(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    if (!lua_istable(L, 3)) bad_argument(L, "table", 3);
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    std::lock_guard<std::mutex> lock(term->locked);
    unsigned int init_x = lua_tointeger(L, 1), init_y = lua_tointeger(L, 2);
    for (unsigned int y = 1; y < lua_objlen(L, 3) && init_y + y - 1 < term->height * TerminalWindow::fontHeight; y++) {
        lua_pushinteger(L, y);
        lua_gettable(L, 3); 
        if (lua_isstring(L, -1)) {
            size_t str_sz;
            const char * str = lua_tolstring(L, -1, &str_sz);
            if (init_x + str_sz - 1 < term->width * TerminalWindow::fontWidth)
                memcpy(&term->pixels[init_y+y-1][init_x], str, str_sz);
        } else if (lua_istable(L, -1)) {
            for (unsigned int x = 1; x < lua_objlen(L, -1) && init_x + x - 1 < term->width * TerminalWindow::fontWidth; x++) {
                lua_pushinteger(L, x);
                lua_gettable(L, -2);
                term->pixels[init_y+y-1][init_x+x-1] = (unsigned char)(lua_tointeger(L, -1) % 256);
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    }
    term->changed = true;
    return 0;
}

int term_screenshot(lua_State *L) {
    if (headless || cli) return 0;
    Computer * computer = get_comp(L);
    TerminalWindow * term = computer->term;
    if (lua_isstring(L, 1)) term->screenshot(std::string(lua_tostring(L, 1), lua_strlen(L, 1)));
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

const char * term_keys[31] = {
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
    "nativePaletteColor",
    "drawPixels"
};

lua_CFunction term_values[31] = {
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
    term_nativePaletteColor,
    term_drawPixels
};

library_t term_lib = {"term", 31, term_keys, term_values, nullptr, nullptr};
