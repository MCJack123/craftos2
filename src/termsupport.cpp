/*
 * termsupport.cpp
 * CraftOS-PC 2
 * 
 * This file implements some helper functions for terminal interaction.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <codecvt>
#include <locale>
#include <queue>
#include <unordered_map>
#include <Computer.hpp>
#include <configuration.hpp>
#include <Terminal.hpp>
#include "apis.hpp"
#include "runtime.hpp"
#include "peripheral/monitor.hpp"
#include "peripheral/debugger.hpp"
#include "terminal/SDLTerminal.hpp"
#include "termsupport.hpp"
#ifndef NO_CLI
#include "terminal/CLITerminal.hpp"
#endif

std::thread * renderThread;
/* export */ std::unordered_map<int, unsigned char> keymap = {
    {0, 1},
    {SDLK_1, 2},
    {SDLK_2, 3},
    {SDLK_3, 4},
    {SDLK_4, 5},
    {SDLK_5, 6},
    {SDLK_6, 7},
    {SDLK_7, 8},
    {SDLK_8, 9},
    {SDLK_9, 10},
    {SDLK_0, 11},
    {SDLK_MINUS, 12},
    {SDLK_EQUALS, 13},
    {SDLK_BACKSPACE, 14},
    {SDLK_TAB, 15},
    {SDLK_q, 16},
    {SDLK_w, 17},
    {SDLK_e, 18},
    {SDLK_r, 19},
    {SDLK_t, 20},
    {SDLK_y, 21},
    {SDLK_u, 22},
    {SDLK_i, 23},
    {SDLK_o, 24},
    {SDLK_p, 25},
    {SDLK_LEFTBRACKET, 26},
    {SDLK_RIGHTBRACKET, 27},
    {SDLK_RETURN, 28},
    {SDLK_LCTRL, 29},
    {SDLK_a, 30},
    {SDLK_s, 31},
    {SDLK_d, 32},
    {SDLK_f, 33},
    {SDLK_g, 34},
    {SDLK_h, 35},
    {SDLK_j, 36},
    {SDLK_k, 37},
    {SDLK_l, 38},
    {SDLK_SEMICOLON, 39},
    {SDLK_QUOTE, 40},
    {SDLK_BACKQUOTE, 41},
    {SDLK_LSHIFT, 42},
    {SDLK_BACKSLASH, 43},
    {SDLK_z, 44},
    {SDLK_x, 45},
    {SDLK_c, 46},
    {SDLK_v, 47},
    {SDLK_b, 48},
    {SDLK_n, 49},
    {SDLK_m, 50},
    {SDLK_COMMA, 51},
    {SDLK_PERIOD, 52},
    {SDLK_SLASH, 53},
    {SDLK_RSHIFT, 54},
    {SDLK_KP_MULTIPLY, 55},
    {SDLK_LALT, 56},
    {SDLK_SPACE, 57},
    {SDLK_CAPSLOCK, 58},
    {SDLK_F1, 59},
    {SDLK_F2, 60},
    {SDLK_F3, 61},
    {SDLK_F4, 62},
    {SDLK_F5, 63},
    {SDLK_F6, 64},
    {SDLK_F7, 65},
    {SDLK_F8, 66},
    {SDLK_F9, 67},
    {SDLK_F10, 68},
    {SDLK_NUMLOCKCLEAR, 69},
    {SDLK_SCROLLLOCK, 70},
    {SDLK_KP_7, 71},
    {SDLK_KP_8, 72},
    {SDLK_KP_9, 73},
    {SDLK_KP_MINUS, 74},
    {SDLK_KP_4, 75},
    {SDLK_KP_5, 76},
    {SDLK_KP_6, 77},
    {SDLK_KP_PLUS, 78},
    {SDLK_KP_1, 79},
    {SDLK_KP_2, 80},
    {SDLK_KP_3, 81},
    {SDLK_KP_0, 82},
    {SDLK_KP_DECIMAL, 83},
    {SDLK_F11, 87},
    {SDLK_F12, 88},
    {SDLK_F13, 100},
    {SDLK_F14, 101},
    {SDLK_F15, 102},
    {SDLK_KP_EQUALS, 141},
    {SDLK_KP_AT, 145},
    {SDLK_KP_COLON, 146},
    {SDLK_STOP, 149},
    {SDLK_KP_ENTER, 156},
    {SDLK_RCTRL, 157},
    {SDLK_KP_COMMA, 179},
    {SDLK_KP_DIVIDE, 181},
    {SDLK_RALT, 184},
    {SDLK_PAUSE, 197},
    {SDLK_HOME, 199},
    {SDLK_UP, 200},
    {SDLK_PAGEUP, 201},
    {SDLK_LEFT, 203},
    {SDLK_RIGHT, 205},
    {SDLK_END, 207},
    {SDLK_DOWN, 208},
    {SDLK_PAGEDOWN, 209},
    {SDLK_INSERT, 210},
    {SDLK_DELETE, 211}
};
#ifndef NO_CLI
/* export */ std::unordered_map<int, unsigned char> keymap_cli = {
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
    {1025, 29},
    {1026, 56}
};
#endif

Uint32 task_event_type;
Uint32 render_event_type;

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

int convertX(SDLTerminal * term, int x) {
    if (term->mode != 0) {
        if (x < 2 * (int)term->charScale) return 0;
        else if ((unsigned)x >= term->charWidth * term->width + 2 * term->charScale * (2 / SDLTerminal::fontScale))
            return (int)(Terminal::fontWidth * term->width - 1);
        return (int)(((unsigned)x - (2 * term->charScale)) / (term->charScale * (2 / SDLTerminal::fontScale)));
    } else {
        if (x < 2 * (int)term->charScale) x = (int)(2 * term->charScale * (2 / SDLTerminal::fontScale));
        else if ((unsigned)x > term->charWidth * term->width + 2 * term->charScale * (2 / SDLTerminal::fontScale))
            x = (int)(term->charWidth * term->width + 2 * term->charScale * (2 / SDLTerminal::fontScale));
        return (int)((x - 2 * term->charScale * (2 / SDLTerminal::fontScale)) / term->charWidth + 1);
    }
}

int convertY(SDLTerminal * term, int x) {
    if (term->mode != 0) {
        if (x < 2 * (int)term->charScale) return 0;
        else if ((unsigned)x >= term->charHeight * term->height + 2 * term->charScale * (2 / SDLTerminal::fontScale))
            return (int)(Terminal::fontHeight * term->height - 1);
        return (int)(((unsigned)x - (2 * term->charScale)) / (term->charScale * (2 / SDLTerminal::fontScale)));
    } else {
        if (x < 2 * (int)term->charScale * (int)(2 / SDLTerminal::fontScale)) x = 2 * (int)term->charScale * (int)(2 / SDLTerminal::fontScale);
        else if ((unsigned)x > term->charHeight * term->height + 2 * term->charScale * (2 / SDLTerminal::fontScale))
            x = (int)(term->charHeight * term->height + 2 * term->charScale * (2 / SDLTerminal::fontScale));
        return (int)((x - 2 * term->charScale * (2 / SDLTerminal::fontScale)) / term->charHeight + 1);
    }
}

inline const char * checkstr(const char * str) {return str == NULL ? "(null)" : str;}

extern library_t * libraries[8];
int termPanic(lua_State *L) {
    Computer * comp = get_comp(L);
    comp->running = 0;
    lua_Debug ar;
    int status;
    for (int i = 0; (status = lua_getstack(L, i, &ar)) && (status = lua_getinfo(L, "nSl", &ar)) && ar.what[0] == 'C'; i++);
    if (status && ar.what[0] != 'C') {
        fprintf(stderr, "An unexpected error occurred in a Lua function: %s:%s:%d: %s\n", checkstr(ar.short_src), checkstr(ar.name), ar.currentline, checkstr(lua_tostring(L, 1)));
        if (config.standardsMode) displayFailure(comp->term, "Error running computer", checkstr(lua_tostring(L, 1)));
        else if (comp->term != NULL) queueTask([ar, comp](void* L_)->void*{comp->term->showMessage(SDL_MESSAGEBOX_ERROR, "Lua Panic", ("An unexpected error occurred in a Lua function: " + std::string(checkstr(ar.short_src)) + ":" + std::string(checkstr(ar.name)) + ":" + std::to_string(ar.currentline) + ": " + std::string(!lua_isstring((lua_State*)L_, 1) ? "(null)" : lua_tostring((lua_State*)L_, 1)) + ". The computer will now shut down.").c_str()); return NULL;}, L);
    } else {
        fprintf(stderr, "An unexpected error occurred in a Lua function: (unknown): %s\n", checkstr(lua_tostring(L, 1)));
        if (config.standardsMode) displayFailure(comp->term, "Error running computer", checkstr(lua_tostring(L, 1)));
        else if (comp->term != NULL) queueTask([comp](void* L_)->void*{comp->term->showMessage(SDL_MESSAGEBOX_ERROR, "Lua Panic", ("An unexpected error occurred in a Lua function: (unknown): " + std::string(!lua_isstring((lua_State*)L_, 1) ? "(null)" : lua_tostring((lua_State*)L_, 1)) + ". The computer will now shut down.").c_str()); return NULL;}, L);
    }
    comp->event_lock.notify_all();
    for (const library_t * lib : libraries) if (lib->deinit != NULL) lib->deinit(comp);
    lua_close(comp->L);   /* Cya, Lua */
    comp->L = NULL;
    longjmp(comp->on_panic, 0);
}

static bool debuggerBreak(lua_State *L, Computer * computer, debugger * dbg, const char * reason) {
    const bool lastBlink = computer->term->canBlink;
    computer->term->canBlink = false;
    dbg->thread = L;
    dbg->breakReason = reason;
    while (!dbg->didBreak) {
        std::lock_guard<std::mutex> guard(dbg->breakMutex);
        dbg->breakNotify.notify_all();
        std::this_thread::yield();
    }
    std::unique_lock<std::mutex> lock(dbg->breakMutex);
    while (dbg->didBreak) dbg->breakNotify.wait_for(lock, std::chrono::milliseconds(500));
    const bool retval = !dbg->running;
    dbg->thread = NULL;
    computer->last_event = std::chrono::high_resolution_clock::now();
    computer->term->canBlink = lastBlink;
    return retval;
}

static void noDebuggerBreak(lua_State *L, Computer * computer, lua_Debug * ar) {
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
    /* export */ int db_debug(lua_State *L) {
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
extern bool forceCheckTimeout;

void termHook(lua_State *L, lua_Debug *ar) {
    if (lua_icontext(L) == 1) {
        lua_pop(L, 1);
        return;
    }
    if (ar->event == LUA_HOOKCOUNT && !forceCheckTimeout) return;
    Computer * computer = get_comp(L);
    if (computer->debugger != NULL && !computer->isDebugger && (computer->shouldDeinitDebugger || ((debugger*)computer->debugger)->running == false)) {
        computer->shouldDeinitDebugger = false;
        lua_getfield(L, LUA_REGISTRYINDEX, "_coroutine_stack");
        for (size_t i = 1; i <= lua_objlen(L, -1); i++) {
            lua_rawgeti(L, -1, (int)i);
            if (lua_isthread(L, -1)) lua_sethook(lua_tothread(L, -1), termHook, LUA_MASKCOUNT | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        lua_sethook(computer->L, termHook, LUA_MASKCOUNT | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
        lua_sethook(computer->coro, termHook, LUA_MASKCOUNT | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
        lua_sethook(L, termHook, LUA_MASKCOUNT | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
        queueTask([](void*arg)->void*{delete (debugger*)arg; return NULL;}, computer->debugger, true);
        computer->debugger = NULL;
    }
    if (ar->event == LUA_HOOKCOUNT) {
        if (!computer->getting_event && !(!computer->isDebugger && computer->debugger != NULL && ((debugger*)computer->debugger)->thread != NULL) && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - computer->last_event).count() > (config.standardsMode ? 7000 : config.abortTimeout)) {
            forceCheckTimeout = false;
            if (++computer->timeoutCheckCount >= 5) {
                if (config.standardsMode) {
                    // In standards mode we give no second chances - just crash and burn
                    displayFailure(computer->term, "Error running computer", "Too long without yielding");
                    computer->event_lock.notify_all();
                    for (const library_t * lib : libraries) if (lib->deinit != NULL) lib->deinit(computer);
                    lua_close(computer->L);   /* Cya, Lua */
                    computer->L = NULL;
                    computer->running = 0;
                    longjmp(computer->on_panic, 0);
                } else {
                    if (queueTask([computer](void*)->void* {
                        if (dynamic_cast<SDLTerminal*>(computer->term) != NULL) {
                            SDL_MessageBoxData msg;
                            SDL_MessageBoxButtonData buttons[] = {
                                {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Restart"},
                                {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Wait"}
                            };
                            msg.flags = SDL_MESSAGEBOX_WARNING;
                            msg.window = dynamic_cast<SDLTerminal*>(computer->term)->win;
                            msg.title = "Computer not responding";
                            msg.message = "A long-running task has caused this computer to stop responding. You can either force restart the computer, or wait for the program to respond.";
                            msg.numbuttons = 2;
                            msg.buttons = buttons;
                            msg.colorScheme = NULL;
                            if (queueTask([](void* arg)->void* {int num = 0; SDL_ShowMessageBox((SDL_MessageBoxData*)arg, &num); return (void*)(ptrdiff_t)num; }, &msg) != NULL) {
                                computer->event_lock.notify_all();
                                for (const library_t * lib : libraries) if (lib->deinit != NULL) lib->deinit(computer);
                                lua_close(computer->L);   /* Cya, Lua */
                                computer->L = NULL;
                                computer->running = 2;
                                return (void*)1;
                            } else {
                                computer->timeoutCheckCount = -15;
                                return NULL;
                            }
                        }
                        return NULL;
                    }, NULL) != NULL) longjmp(computer->on_panic, 0);
                }
            }
            luaL_where(L, 1);
            lua_pushstring(L, "Too long without yielding");
            lua_concat(L, 2);
            //fprintf(stderr, "%s\n", lua_tostring(L, -1));
            lua_error(L);
        }
    } else if (ar->event == LUA_HOOKLINE && ::config.debug_enable) {
        if (computer->debugger == NULL && computer->hasBreakpoints) {
            lua_getinfo(L, "Sl", ar);
            for (std::pair<int, std::pair<std::string, lua_Integer> > b : computer->breakpoints) {
                if (b.second.first == std::string(ar->source) && b.second.second == ar->currentline) {
                    noDebuggerBreak(L, computer, ar);
                    break;
                }
            }
        } else if (computer->debugger != NULL && !computer->isDebugger) {
            debugger * dbg = (debugger*)computer->debugger;
            if (dbg->thread == NULL) {
                if (dbg->breakType == DEBUGGER_BREAK_TYPE_LINE) {
                    if (dbg->stepCount == 0) debuggerBreak(L, computer, dbg, "Pause");
                    else dbg->stepCount--;
                } else if (!computer->breakpoints.empty()) {
                    lua_getinfo(L, "Sl", ar);
                    for (std::pair<int, std::pair<std::string, lua_Integer> > b : computer->breakpoints) {
                        if (b.second.first == std::string(ar->source) && b.second.second == ar->currentline) {
                            if (debuggerBreak(L, computer, dbg, "Breakpoint")) return;
                            break;
                        }
                    }
                }
            }
        }
    } else if (ar->event == LUA_HOOKERROR) {
        if (config.logErrors) {
            if (config.debug_enable && !computer->isDebugger && (computer->debugger == NULL || ((debugger*)computer->debugger)->thread == NULL)) {
                lua_getglobal(L, "debug");
                lua_getfield(L, -1, "traceback");
                lua_pushfstring(L, "Got error: %s", lua_tostring(L, -4));
                lua_call(L, 1, 1);
                fprintf(stderr, "%s\n", lua_tostring(L, -1));
                lua_pop(L, 2);
            } else {
               fprintf(stderr, "Got error: %s\n", lua_tostring(L, -2));
            }
        }
        if (!computer->isDebugger && computer->debugger != NULL) {
            debugger * dbg = (debugger*)computer->debugger;
            if (dbg->thread == NULL && (dbg->breakMask & DEBUGGER_BREAK_FUNC_ERROR)) 
                if (debuggerBreak(L, computer, dbg, lua_tostring(L, -2) == NULL ? "Error" : lua_tostring(L, -2))) return;
        }
    } else if (computer->debugger != NULL && !computer->isDebugger) {
        if (ar->event == LUA_HOOKRET || ar->event == LUA_HOOKTAILRET) {
            debugger * dbg = (debugger*)computer->debugger;
            if (dbg->breakType == DEBUGGER_BREAK_TYPE_RETURN && dbg->thread == NULL && debuggerBreak(L, computer, dbg, "Pause")) return;
            if (dbg->isProfiling) {
                lua_getinfo(L, "nS", ar);
                if (ar->source != NULL && ar->name != NULL && dbg->profile.find(ar->source) != dbg->profile.end() && dbg->profile[ar->source].find(ar->name) != dbg->profile[ar->source].end()) {
                    dbg->profile[ar->source][ar->name].time += (std::chrono::high_resolution_clock::now() - dbg->profile[ar->source][ar->name].start);
                    dbg->profile[ar->source][ar->name].running = false;
                }
            }
        } else if (ar->event == LUA_HOOKCALL && ar->source != NULL && ar->name != NULL) {
            debugger * dbg = (debugger*)computer->debugger;
            if (dbg->thread == NULL) {
                lua_getinfo(L, "nS", ar);
                if (ar->name != NULL && ((((std::string(ar->name) == "loadAPI" && std::string(ar->source).find("bios.lua") != std::string::npos) || std::string(ar->name) == "require") && (dbg->breakMask & DEBUGGER_BREAK_FUNC_LOAD)) ||
                    (((std::string(ar->name) == "run" && std::string(ar->source).find("bios.lua") != std::string::npos) || (std::string(ar->name) == "dofile" && std::string(ar->source).find("bios.lua") != std::string::npos)) && (dbg->breakMask & DEBUGGER_BREAK_FUNC_RUN)))) if (debuggerBreak(L, computer, dbg, "Caught call")) return;
            }
            if (dbg->isProfiling) {
                lua_getinfo(L, "nS", ar);
                std::string name;
                if (ar->name == NULL) name = "(unknown)";
                else name = ar->name;
                if (dbg->profile.find(ar->source) == dbg->profile.end()) dbg->profile[ar->source] = {};
                if (dbg->profile[ar->source].find(name) == dbg->profile[ar->source].end()) dbg->profile[ar->source][name] = {true, 1, std::chrono::high_resolution_clock::now(), std::chrono::microseconds(0)};
                else {
                    if (dbg->profile[ar->source][name].running) {
                        dbg->profile[ar->source][name].time += (std::chrono::high_resolution_clock::now() - dbg->profile[ar->source][name].start);
                        dbg->profile[ar->source][name].running = false;
                    }
                    dbg->profile[ar->source][name].running = true;
                    dbg->profile[ar->source][name].count++;
                    dbg->profile[ar->source][name].start = std::chrono::high_resolution_clock::now();
                }
            }
        } else if (ar->event == LUA_HOOKRESUME) {
            debugger * dbg = (debugger*)computer->debugger;
            if (dbg->thread == NULL && (dbg->breakMask & DEBUGGER_BREAK_FUNC_RESUME)) 
                if (debuggerBreak(L, computer, dbg, "Resume")) return;
        } else if (ar->event == LUA_HOOKYIELD) {
            debugger * dbg = (debugger*)computer->debugger;
            if (dbg->thread == NULL && (dbg->breakMask & DEBUGGER_BREAK_FUNC_YIELD)) 
                if (debuggerBreak(L, computer, dbg, "Yield")) return;
        }
    }
    if (ar->event != LUA_HOOKCOUNT && (computer->hookMask & (1 << ar->event))) {
        lua_pushlightuserdata(L, (void*)&KEY_HOOK);
        lua_gettable(L, LUA_REGISTRYINDEX);
        if (lua_istable(L, -1)) {
            lua_pushlightuserdata(L, L);
            lua_gettable(L, -2);
            if (lua_istable(L, -1)) {
                lua_getfield(L, -1, "mask");
                if (lua_tointeger(L, -1) & ((lua_Integer)1 << ar->event)) {
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
                        lua_icall(L, 2, 0, 1);
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
        renderTargetsLock.lock();
        #ifndef NO_CLI
        bool willForceRender = CLITerminal::forceRender;
        #endif
        bool errored = false;
        for (Terminal* term : renderTargets) {
            if (!term->canBlink) term->blink = false;
            else if (selectedRenderer != 1 && selectedRenderer != 2 && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - term->last_blink).count() > 500) {
                term->blink = !term->blink;
                term->last_blink = std::chrono::high_resolution_clock::now();
                term->changed = true;
            }
            const bool changed = term->changed;
            try {
                term->render();
            } catch (std::exception &ex) {
                fprintf(stderr, "Warning: Render on term %d threw an error: %s (%d)\n", term->id, ex.what(), term->errorcount);
                if (term->errorcount++ > 10) {
                    term->errorcount = 0;
                    term->showMessage(SDL_MESSAGEBOX_ERROR, "Error rendering terminal", std::string(std::string("An error repeatedly occurred while attempting to render the terminal: ") + ex.what() + ". This is likely a bug in CraftOS-PC. Please go to https://www.craftos-pc.cc/bugreport and report this issue. The window will now close. Please note that CraftOS-PC may be left in an invalid state - you should restart the emulator.").c_str());
                    SDL_Event e;
                    e.type = SDL_WINDOWEVENT;
                    e.window.event = SDL_WINDOWEVENT_CLOSE;
                    e.window.windowID = term->id;
                    SDL_PushEvent(&e);
                    errored = true;
                    break;
                }
                continue;
            }
            if (changed) term->errorcount = 0;
            pushEvent = pushEvent || changed;
            term->framecount++;
        }
        renderTargetsLock.unlock();
        if (errored) continue;
        if (pushEvent) {
            SDL_Event ev;
            ev.type = render_event_type;
            SDL_PushEvent(&ev);
            const long long count = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
            //printf("Render thread took %lld us (%lld fps)\n", count, count == 0 ? 1000000 : 1000000 / count);
            long long t = (1000/config.clockSpeed) - count / 1000;
            if (t > 0) std::this_thread::sleep_for(std::chrono::milliseconds(t));
        } else {
            int time = 1000/config.clockSpeed;
            std::chrono::milliseconds ms(time);
            std::this_thread::sleep_for(ms);
        }
        #ifndef NO_CLI
        if (willForceRender) CLITerminal::forceRender = false;
        #endif
    }
}

void gettingEvent(Computer *comp) {comp->getting_event = true;}
void gotEvent(Computer *comp) {comp->last_event = std::chrono::high_resolution_clock::now(); comp->getting_event = false;}

static std::string utf8_to_string(const char *utf8str, const std::locale& loc)
{
    // UTF-8 to wstring
    std::wstring_convert<std::codecvt_utf8<wchar_t>> wconv;
    const std::wstring wstr = wconv.from_bytes(utf8str);
    // wstring to string
    std::vector<char> buf(wstr.size());
    std::use_facet<std::ctype<wchar_t>>(loc).narrow(wstr.data(), wstr.data() + wstr.size(), '\0', buf.data());
    return std::string(buf.data(), buf.size());
}

static Uint32 mouseDebounce(Uint32 interval, void* param);

static std::string mouse_move(lua_State *L, void* param) {
    Computer * computer = get_comp(L);
    lua_pushinteger(L, 1);
    lua_pushinteger(L, computer->nextMouseMove.x);
    lua_pushinteger(L, computer->nextMouseMove.y);
    if (!computer->nextMouseMove.side.empty()) lua_pushstring(L, computer->nextMouseMove.side.c_str());
    computer->nextMouseMove = {0, 0, 0, 0, std::string()};
    computer->mouseMoveDebounceTimer = SDL_AddTimer(config.mouse_move_throttle, mouseDebounce, computer);
    return "mouse_move";
}

static Uint32 mouseDebounce(Uint32 interval, void* param) {
    Computer * computer = (Computer*)param;
    if (freedComputers.find(computer) != freedComputers.end()) return 0;
    if (computer->nextMouseMove.event) queueEvent(computer, mouse_move, NULL);
    else computer->mouseMoveDebounceTimer = 0;
    return 0;
}

std::string termGetEvent(lua_State *L) {
    Computer * computer = get_comp(L);
    computer->event_provider_queue_mutex.lock();
    if (!computer->event_provider_queue.empty()) {
        const std::pair<event_provider, void*> p = computer->event_provider_queue.front();
        computer->event_provider_queue.pop();
        computer->event_provider_queue_mutex.unlock();
        return p.first(L, p.second);
    }
    computer->event_provider_queue_mutex.unlock();
    if (computer->lastResizeEvent) {
        computer->lastResizeEvent = false;
        return "term_resize";
    }
    if (computer->running != 1) return "";
    SDL_Event e;
    std::string tmpstrval;
    if (Computer_getEvent(computer, &e)) {
        if (e.type == SDL_QUIT) 
            return "die";
        else if (e.type == SDL_KEYDOWN && ((selectedRenderer != 0 && selectedRenderer != 5) || keymap.find(e.key.keysym.sym) != keymap.end())) {
            Terminal * term = e.key.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.key.windowID, tmpstrval)->term;
            SDLTerminal * sdlterm = dynamic_cast<SDLTerminal*>(term);
            if (e.key.keysym.sym == SDLK_F2 && (e.key.keysym.mod & ~(KMOD_CAPS | KMOD_NUM)) == 0 && sdlterm != NULL && !config.ignoreHotkeys) sdlterm->screenshot();
            else if (e.key.keysym.sym == SDLK_F3 && (e.key.keysym.mod & ~(KMOD_CAPS | KMOD_NUM)) == 0 && sdlterm != NULL && !config.ignoreHotkeys) sdlterm->toggleRecording();
#ifndef __EMSCRIPTEN__
            else if (e.key.keysym.sym == SDLK_F11 && (e.key.keysym.mod & ~(KMOD_CAPS | KMOD_NUM)) == 0 && sdlterm != NULL && !config.ignoreHotkeys) sdlterm->toggleFullscreen();
#endif
            else if (e.key.keysym.sym == SDLK_F12 && (e.key.keysym.mod & ~(KMOD_CAPS | KMOD_NUM)) == 0 && sdlterm != NULL && !config.ignoreHotkeys) sdlterm->screenshot("clipboard");
            else if (((selectedRenderer == 0 || selectedRenderer == 5) ? e.key.keysym.sym == SDLK_t : e.key.keysym.sym == 20) && (e.key.keysym.mod & KMOD_CTRL)) {
                if (computer->waitingForTerminate & 1) {
                    computer->waitingForTerminate |= 2;
                    computer->waitingForTerminate &= ~1;
                    return "terminate";
                } else if ((computer->waitingForTerminate & 3) == 0) computer->waitingForTerminate |= 1;
            } else if (((selectedRenderer == 0 || selectedRenderer == 5) ? e.key.keysym.sym == SDLK_s : e.key.keysym.sym == 31) && (e.key.keysym.mod & KMOD_CTRL)) {
                if (computer->waitingForTerminate & 4) {
                    computer->waitingForTerminate |= 8;
                    computer->waitingForTerminate &= ~4;
                    computer->running = 0;
                    return "terminate";
                } else if ((computer->waitingForTerminate & 12) == 0) computer->waitingForTerminate |= 4;
            } else if (((selectedRenderer == 0 || selectedRenderer == 5) ? e.key.keysym.sym == SDLK_r : e.key.keysym.sym == 19) && (e.key.keysym.mod & KMOD_CTRL)) {
                if (computer->waitingForTerminate & 16) {
                    computer->waitingForTerminate |= 32;
                    computer->waitingForTerminate &= ~16;
                    computer->running = 2;
                    return "terminate";
                } else if ((computer->waitingForTerminate & 48) == 0) computer->waitingForTerminate |= 16;
            } else if (e.key.keysym.sym == SDLK_v && 
#ifdef __APPLE__
              (e.key.keysym.mod & KMOD_GUI) &&
#else
              (e.key.keysym.mod & KMOD_CTRL) &&
#endif
              SDL_HasClipboardText()) {
                char * text = SDL_GetClipboardText();
                std::string str = utf8_to_string(text, std::locale("C"));
                str.erase(std::remove(str.begin(), str.end(), '\r'), str.end());
                lua_pushstring(L, str.c_str());
                SDL_free(text);
                return "paste";
            } else computer->waitingForTerminate = 0;
            if (selectedRenderer != 0 && selectedRenderer != 5) lua_pushinteger(L, e.key.keysym.sym); 
            else lua_pushinteger(L, keymap.at(e.key.keysym.sym));
            lua_pushboolean(L, e.key.repeat);
            return "key";
        } else if (e.type == SDL_KEYUP && (selectedRenderer == 2 || keymap.find(e.key.keysym.sym) != keymap.end())) {
            if (e.key.keysym.sym != SDLK_F2 || config.ignoreHotkeys) {
                computer->waitingForTerminate = 0;
                if (selectedRenderer != 0 && selectedRenderer != 5) lua_pushinteger(L, e.key.keysym.sym); 
                else lua_pushinteger(L, keymap.at(e.key.keysym.sym));
                return "key_up";
            }
        } else if (e.type == SDL_TEXTINPUT) {
            std::string str = utf8_to_string(e.text.text, std::locale("C"));
            if (str[0] != '\0') {
                lua_pushlstring(L, str.c_str(), 1);
                return "char";
            }
        } else if (e.type == SDL_MOUSEBUTTONDOWN && (computer->config->isColor || computer->isDebugger)) {
            Terminal * term = e.button.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.button.windowID, tmpstrval)->term;
            int x = 1, y = 1;
            if (selectedRenderer >= 2 && selectedRenderer <= 4) {
                x = e.button.x; y = e.button.y;
            } else if (dynamic_cast<SDLTerminal*>(term) != NULL) {
                x = convertX(dynamic_cast<SDLTerminal*>(term), e.button.x); y = convertY(dynamic_cast<SDLTerminal*>(term), e.button.y);
            }
            if (computer->lastMouse.x == x && computer->lastMouse.y == y && computer->lastMouse.button == e.button.button && computer->lastMouse.event == 0) return "";
            computer->lastMouse = {x, y, e.button.button, 0, ""};
            if (e.button.windowID == term->id || config.monitorsUseMouseEvents) lua_pushinteger(L, buttonConvert(e.button.button));
            else lua_pushstring(L, tmpstrval.c_str());
            lua_pushinteger(L, x);
            lua_pushinteger(L, y);
            if (e.button.windowID != term->id && config.monitorsUseMouseEvents) lua_pushstring(L, tmpstrval.c_str());
            return (e.button.windowID == term->id || config.monitorsUseMouseEvents) ? "mouse_click" : "monitor_touch";
        } else if (e.type == SDL_MOUSEBUTTONUP && (computer->config->isColor || computer->isDebugger) && (e.button.windowID == computer->term->id || config.monitorsUseMouseEvents)) {
            Terminal * term = e.button.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.button.windowID, tmpstrval)->term;
            int x = 1, y = 1;
            if (selectedRenderer >= 2 && selectedRenderer <= 4) {
                x = e.button.x; y = e.button.y;
            } else if (dynamic_cast<SDLTerminal*>(term) != NULL) {
                x = convertX(dynamic_cast<SDLTerminal*>(term), e.button.x); y = convertY(dynamic_cast<SDLTerminal*>(term), e.button.y);
            }
            if (computer->lastMouse.x == x && computer->lastMouse.y == y && computer->lastMouse.button == e.button.button && computer->lastMouse.event == 1) return "";
            computer->lastMouse = {x, y, e.button.button, 1, ""};
            lua_pushinteger(L, buttonConvert(e.button.button));
            lua_pushinteger(L, x);
            lua_pushinteger(L, y);
            if (e.button.windowID != term->id && config.monitorsUseMouseEvents) lua_pushstring(L, tmpstrval.c_str());
            return "mouse_up";
        } else if (e.type == SDL_MOUSEWHEEL && (computer->config->isColor || computer->isDebugger) && (e.wheel.windowID == computer->term->id || config.monitorsUseMouseEvents)) {
            SDLTerminal * term = dynamic_cast<SDLTerminal*>(e.wheel.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.wheel.windowID, tmpstrval)->term);
            if (term == NULL) {
                return "";
            } else {
                int x = 0, y = 0;
                term->getMouse(&x, &y);
                lua_pushinteger(L, max(min(e.wheel.y * (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? 1 : -1), 1), -1));
                lua_pushinteger(L, convertX(term, x));
                lua_pushinteger(L, convertY(term, y));
                if (e.wheel.windowID != term->id && config.monitorsUseMouseEvents) lua_pushstring(L, tmpstrval.c_str());
            }
            return "mouse_scroll";
        } else if (e.type == SDL_MOUSEMOTION && (config.mouse_move_throttle >= 0 || e.motion.state) && (computer->config->isColor || computer->isDebugger) && (e.motion.windowID == computer->term->id || config.monitorsUseMouseEvents)) {
            SDLTerminal * term = dynamic_cast<SDLTerminal*>(e.motion.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.motion.windowID, tmpstrval)->term);
            if (term == NULL) return "";
            int x = 1, y = 1;
            if (selectedRenderer >= 2 && selectedRenderer <= 4) {
                x = e.motion.x; y = e.motion.y;
            } else if (term != NULL) {
                x = convertX(term, e.motion.x); y = convertY(dynamic_cast<SDLTerminal*>(term), e.motion.y);
            }
            if (computer->lastMouse.x == x && computer->lastMouse.y == y && computer->lastMouse.button == buttonConvert2(e.motion.state) && computer->lastMouse.event == 2) return "";
            computer->lastMouse = {x, y, (uint8_t)buttonConvert2(e.motion.state), 2, ""};
            if (!e.motion.state) {
                if (computer->mouseMoveDebounceTimer == 0) {
                    computer->mouseMoveDebounceTimer = SDL_AddTimer(config.mouse_move_throttle, mouseDebounce, computer);
                    computer->nextMouseMove = {0, 0, 0, 0, std::string()};
                } else {
                    computer->nextMouseMove = {x, y, 0, 1, (e.motion.windowID != computer->term->id && config.monitorsUseMouseEvents) ? tmpstrval : ""};
                    return "";
                }
            }
            lua_pushinteger(L, buttonConvert2(e.motion.state));
            lua_pushinteger(L, x);
            lua_pushinteger(L, y);
            if (e.motion.windowID != term->id && config.monitorsUseMouseEvents) lua_pushstring(L, tmpstrval.c_str());
            return e.motion.state ? "mouse_drag" : "mouse_move";
        } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
            unsigned w, h;
            if (selectedRenderer == 0 || selectedRenderer == 5) {
                SDLTerminal * sdlterm = dynamic_cast<SDLTerminal*>(computer->term);
                if (sdlterm != NULL) {
                    w = (e.window.data1 - 4*(2/SDLTerminal::fontScale)*sdlterm->charScale) / sdlterm->charWidth;
                    h = (e.window.data2 - 4*(2/SDLTerminal::fontScale)*sdlterm->charScale) / sdlterm->charHeight;
                } else {w = 51; h = 19;}
            } else {w = e.window.data1; h = e.window.data2;}
            if (computer->term != NULL && e.window.windowID == computer->term->id && computer->term->resize(w, h)) {
                computer->lastResizeEvent = true;
                return "term_resize";
            } else {
                std::string side;
                monitor * m = findMonitorFromWindowID(computer, e.window.windowID, side);
                if (m != NULL && m->term->resize(w, h)) {
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
                    if (strcmp(periphemu_lib.functions[1].name, "detach") == 0) lua_pop(L, periphemu_lib.functions[1].func(L) + 1);
                    else for (int i = 0; periphemu_lib.functions[i].name; i++)
                        if (strcmp(periphemu_lib.functions[i].name, "detach") == 0) {lua_pop(L, periphemu_lib.functions[i].func(L) + 1); break;}
                }
            }
        } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_LEAVE && config.mouse_move_throttle >= 0 && (e.button.windowID == computer->term->id || config.monitorsUseMouseEvents)) {
            if (computer->mouseMoveDebounceTimer != 0) {
                SDL_RemoveTimer(computer->mouseMoveDebounceTimer);
                computer->mouseMoveDebounceTimer = 0;
                computer->nextMouseMove = {0, 0, 0, 0, std::string() };
            }
            lua_pushinteger(L, 1);
            lua_pushnil(L);
            lua_pushnil(L);
            return "mouse_move";
        }
    }
    return "";
}

void displayFailure(Terminal * term, std::string message, std::string extra) {
    if (!term) return;
    std::lock_guard<std::mutex> lock(term->locked);
    term->mode = 0;
    memset(term->screen.data(), ' ', term->height * term->width);
    memset(term->colors.data(), term->grayscale ? 0xF0 : 0xFE, term->height * term->width);
    memcpy(term->screen.data(), message.c_str(), min(message.size(), (size_t)term->width));
    unsigned offset = term->width;
    if (!extra.empty()) {
        memcpy(term->screen.data() + offset, extra.c_str(), min(extra.size(), (size_t)term->width));
        offset *= 2;
    }
    strcpy((char*)term->screen.data() + offset, "CraftOS-PC may be installed incorrectly");
    term->canBlink = false;
    term->errorMode = true;
    term->changed = true;
}
