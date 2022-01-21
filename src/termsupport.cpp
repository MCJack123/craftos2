/*
 * termsupport.cpp
 * CraftOS-PC 2
 * 
 * This file implements some helper functions for terminal interaction.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
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
#ifndef NO_CLI
#include <curses.h>
#endif
#include <dirent.h>
#ifndef WIN32
#include <libgen.h>
#endif
#include <sys/stat.h>
#include <Terminal.hpp>
#include "apis.hpp"
#include "runtime.hpp"
#include "peripheral/monitor.hpp"
#include "peripheral/debugger.hpp"
#include "termsupport.hpp"
#include "terminal/SDLTerminal.hpp"
#include "terminal/HardwareSDLTerminal.hpp"
#include "terminal/RawTerminal.hpp"
#include "terminal/TRoRTerminal.hpp"
#ifndef NO_CLI
#include "terminal/CLITerminal.hpp"
#endif
#if defined(__INTELLISENSE__) && !defined(S_ISDIR)
#define S_ISDIR(m) 1 // silence errors in IntelliSense (which isn't very intelligent for its name)
#define W_OK 2
#endif

#ifdef __ANDROID__
extern "C" {extern int Android_JNI_SetupThread(void);}
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

template<class T>
class TerminalFactoryImpl: public TerminalFactory {
public:
    virtual Terminal * createTerminal(const std::string& title) {T* retval = new T(title); retval->factory = this; return retval;}
    virtual void deleteTerminal(Terminal * term) {delete (T*)term;}
    virtual void init() {T::init();}
    virtual void quit() {T::quit();}
    virtual void pollEvents() {T::pollEvents();}
};

TerminalFactoryImpl<SDLTerminal> SDLTerminalFactory;
TerminalFactoryImpl<HardwareSDLTerminal> HardwareSDLTerminalFactory;
TerminalFactoryImpl<RawTerminal> RawTerminalFactory;
TerminalFactoryImpl<TRoRTerminal> TRoRTerminalFactory;
#ifndef NO_CLI
TerminalFactoryImpl<CLITerminal> CLITerminalFactory;
#endif

std::vector<TerminalFactory *> terminalFactories = {
    &SDLTerminalFactory,
    NULL,
#ifndef NO_CLI
    &CLITerminalFactory,
#else
    NULL,
#endif
    &RawTerminalFactory,
    &TRoRTerminalFactory,
    &HardwareSDLTerminalFactory
};

Uint32 task_event_type;
Uint32 render_event_type;
std::list<Terminal*> renderTargets;
std::mutex renderTargetsLock;
std::list<Terminal*>::iterator renderTarget = renderTargets.end();
std::set<unsigned> currentWindowIDs;
#if defined(__EMSCRIPTEN__) || defined(__IPHONEOS__) || defined(__ANDROID__)
bool singleWindowMode = true;
#else
bool singleWindowMode = false;
#endif

int convertX(SDLTerminal * term, int x) {
    if (term->mode != 0) {
        if ((unsigned)x < 2 * term->dpiScale * (int)term->charScale) return 0;
        else if ((unsigned)x >= term->charWidth * term->dpiScale * term->width + 2 * term->charScale * term->dpiScale)
            return (int)(Terminal::fontWidth * term->width - 1);
        return (int)(((unsigned)x - (2 * term->dpiScale * term->charScale)) / (term->charScale * term->dpiScale));
    } else {
        if ((unsigned)x < 2 * term->dpiScale * (int)term->charScale) return 1;
        else if ((unsigned)x >= term->charWidth * term->dpiScale * term->width + 2 * term->charScale * term->dpiScale) return (int)term->width;
        return (int)((x - 2 * term->charScale * term->dpiScale) / (term->dpiScale * term->charWidth) + 1);
    }
}

int convertY(SDLTerminal * term, int x) {
    if (term->mode != 0) {
        if ((unsigned)x < 2 * term->dpiScale * (int)term->charScale) return 0;
        else if ((unsigned)x >= term->charHeight * term->dpiScale * term->height + 2 * term->charScale * term->dpiScale)
            return (int)(Terminal::fontHeight * term->height - 1);
        return (int)(((unsigned)x - (2 * term->dpiScale * term->charScale)) / (term->charScale * term->dpiScale));
    } else {
        if ((unsigned)x < 2 * (int)term->charScale * term->dpiScale) return 1;
        else if ((unsigned)x >= term->charHeight * term->dpiScale * term->height + 2 * term->charScale * term->dpiScale) return (int)term->height;
        return (int)((x - 2 * term->charScale * term->dpiScale) / (term->dpiScale * term->charHeight) + 1);
    }
}

inline const char * checkstr(const char * str) {return str == NULL ? "(null)" : str;}

extern library_t * libraries[];
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
    // Stop all open websockets
    while (!comp->openWebsockets.empty()) stopWebsocket(*comp->openWebsockets.begin());
    for (library_t ** lib = libraries; *lib != NULL; lib++) if ((*lib)->deinit != NULL) (*lib)->deinit(comp);
    lua_close(comp->L);   /* Cya, Lua */
    if (comp->eventTimeout != 0) SDL_RemoveTimer(comp->eventTimeout);
    comp->eventTimeout = 0;
    comp->L = NULL;
    if (comp->rawFileStack) {
        std::lock_guard<std::mutex> lock(comp->rawFileStackMutex);
        lua_close(comp->rawFileStack);
        comp->rawFileStack = NULL;
    }
    longjmp(comp->on_panic, 0);
}

static bool debuggerBreak(lua_State *L, Computer * computer, debugger * dbg, const char * reason) {
    const bool lastBlink = computer->term->canBlink;
    computer->term->canBlink = false;
    if (computer->eventTimeout != 0) 
#ifdef __EMSCRIPTEN__
        queueTask([computer](void*)->void*{
#endif
        SDL_RemoveTimer(computer->eventTimeout);
#ifdef __EMSCRIPTEN__
        return NULL;}, NULL);
#endif
    computer->eventTimeout = 0;
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
#ifdef __EMSCRIPTEN__
    queueTask([computer](void*)->void*{
#endif
    if (computer->eventTimeout != 0) SDL_RemoveTimer(computer->eventTimeout);
    if (config.abortTimeout > 0 || config.standardsMode) computer->eventTimeout = SDL_AddTimer(config.standardsMode ? 7000 : config.abortTimeout, eventTimeoutEvent, computer);
#ifdef __EMSCRIPTEN__
    return NULL;}, NULL);
#endif
    computer->last_event = std::chrono::high_resolution_clock::now();
    computer->term->canBlink = lastBlink;
    return retval;
}

static void noDebuggerBreak(lua_State *L, Computer * computer, lua_Debug * ar) {
    lua_pushboolean(L, true);
    lua_setglobal(L, "_CCPC_DEBUGGER_ACTIVE");
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
    lua_pushnil(L);
    lua_setglobal(L, "_CCPC_DEBUGGER_ACTIVE");
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

extern "C" {
#ifdef _WIN32
    __declspec(dllimport)
#endif
    extern const char KEY_HOOK;
}

void termHook(lua_State *L, lua_Debug *ar) {
    std::string name; // For some reason MSVC explodes when this isn't at the top of the function
                      // I've had issues with it randomly moving scope boundaries around (see apis/config.cpp:101, runtime.cpp:249),
                      // so I'm not surprised about it happening again.
    if (lua_icontext(L) == 1) {
        lua_pop(L, 1);
        return;
    }
    Computer * computer = get_comp(L);
    if (computer->debugger != NULL && !computer->isDebugger && (computer->shouldDeinitDebugger || ((debugger*)computer->debugger)->running == false)) {
        computer->shouldDeinitDebugger = false;
        lua_getfield(L, LUA_REGISTRYINDEX, "_coroutine_stack");
        for (size_t i = 1; i <= lua_objlen(L, -1); i++) {
            lua_rawgeti(L, -1, (int)i);
            if (lua_isthread(L, -1)) lua_sethook(lua_tothread(L, -1), NULL, 0, 0); //lua_sethook(lua_tothread(L, -1), termHook, LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 0);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
        /*lua_sethook(computer->L, termHook, LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 0);
        lua_sethook(computer->coro, termHook, LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 0);
        lua_sethook(L, termHook, LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 0);*/
        lua_sethook(computer->L, NULL, 0, 0);
        lua_sethook(computer->coro, NULL, 0, 0);
        lua_sethook(L, NULL, 0, 0);
        if (computer->shouldDeleteDebugger) queueTask([computer](void*arg)->void*{delete (debugger*)arg; computer->shouldDeleteDebugger = true; return NULL;}, computer->debugger, true);
        computer->shouldDeleteDebugger = true;
        computer->debugger = NULL;
    }
    if (ar->event == LUA_HOOKLINE) {
        if (computer->debugger == NULL && computer->hasBreakpoints) {
            lua_getinfo(L, "Sl", ar);
            for (std::pair<int, std::pair<std::string, lua_Integer> > b : computer->breakpoints) {
                if ((b.second.first == std::string(ar->source) || "@" + b.second.first.substr(2) == std::string(ar->source)) && b.second.second == ar->currentline) {
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
                        if ((b.second.first == std::string(ar->source) || "@" + b.second.first.substr(2) == std::string(ar->source)) && b.second.second == ar->currentline) {
                            if (debuggerBreak(L, computer, dbg, "Breakpoint")) return;
                            break;
                        }
                    }
                }
            }
        }
    } else if (ar->event == LUA_HOOKERROR) {
        if (config.logErrors) {
            if (!computer->isDebugger && (computer->debugger == NULL || ((debugger*)computer->debugger)->thread == NULL)) {
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
                if (ar->name != NULL && ((((std::string(ar->name) == "loadAPI" && std::string(ar->source).find("bios.lua") != std::string::npos) || std::string(ar->name) == "require" || (std::string(ar->name) == "loadfile" && std::string(ar->source).find("bios.lua") != std::string::npos)) && (dbg->breakMask & DEBUGGER_BREAK_FUNC_LOAD)) ||
                    (((std::string(ar->name) == "run" && std::string(ar->source).find("bios.lua") != std::string::npos) || (std::string(ar->name) == "dofile" && std::string(ar->source).find("bios.lua") != std::string::npos)) && (dbg->breakMask & DEBUGGER_BREAK_FUNC_RUN)))) if (debuggerBreak(L, computer, dbg, "Caught call")) return;
            }
            if (dbg->isProfiling) {
                lua_getinfo(L, "nS", ar);
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
}

static bool renderTerminal(Terminal * term, bool& pushEvent) {
    bool changed;
    {
        std::lock_guard<std::mutex> lock(term->locked);
        if (!term->canBlink) term->blink = false;
        else if (selectedRenderer != 1 && selectedRenderer != 2 && selectedRenderer != 3 && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - term->last_blink).count() > 400) {
            term->blink = !term->blink;
            term->last_blink = std::chrono::high_resolution_clock::now();
            term->changed = true;
        }
        if (term->frozen) return false;
        changed = term->changed;
    }
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
            return true;
        }
        return false;
    }
    if (changed) term->errorcount = 0;
    pushEvent = pushEvent || changed;
    term->framecount++;
    return false;
}

void termRenderLoop() {
#ifdef __APPLE__
    pthread_setname_np("Render Thread");
#endif
#ifdef __ANDROID__
    Android_JNI_SetupThread();
#endif
    while (!exiting) {
        std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
        bool pushEvent = false;
        #ifndef NO_CLI
        const bool willForceRender = CLITerminal::forceRender;
        #endif
        bool errored = false;
        {
            std::lock_guard<std::mutex> lock(renderTargetsLock);
            if (singleWindowMode) {if (renderTarget != renderTargets.end() && renderTerminal(*renderTarget, pushEvent)) {errored = true; break;}}
            else for (Terminal* term : renderTargets) if (renderTerminal(term, pushEvent)) {errored = true; break;}
        }
        if (errored) continue;
        if (pushEvent) {
            SDL_Event ev;
            ev.type = render_event_type;
            SDL_PushEvent(&ev);
        }
        const long long count = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - start).count();
        //printf("Render thread took %lld us (%lld fps)\n", count, count == 0 ? 1000000 : 1000000 / count);
        long long t = (1000000/config.clockSpeed) - count;
        if (t > 0) std::this_thread::sleep_for(std::chrono::microseconds(t));
#ifndef NO_CLI
        if (willForceRender) CLITerminal::forceRender = false;
#endif
    }
}

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
struct comp_term_pair {Computer * comp; Terminal * term;};

static std::string mouse_move(lua_State *L, void* param) {
    Terminal * term = (Terminal*)param;
    lua_pushinteger(L, 1);
    lua_pushinteger(L, term->nextMouseMove.x);
    lua_pushinteger(L, term->nextMouseMove.y);
    if (!term->nextMouseMove.side.empty()) lua_pushstring(L, term->nextMouseMove.side.c_str());
    term->nextMouseMove = {0, 0, 0, 0, std::string()};
    term->mouseMoveDebounceTimer = SDL_AddTimer(config.mouse_move_throttle, mouseDebounce, new comp_term_pair {get_comp(L), term});
    return "mouse_move";
}

static Uint32 mouseDebounce(Uint32 interval, void* param) {
    comp_term_pair * data = (comp_term_pair*)param;
    if (freedComputers.find(data->comp) != freedComputers.end()) return 0;
    std::lock_guard<std::mutex> lock(((SDLTerminal*)data->term)->mouseMoveLock);
    if (data->term->nextMouseMove.event) queueEvent(data->comp, mouse_move, data->term);
    else data->term->mouseMoveDebounceTimer = 0;
    delete data;
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
    if (computer->running != 1) return "";
    SDL_Event e;
    if (Computer_getEvent(computer, &e)) {
        if (e.type == SDL_QUIT) {
            computer->requestedExit = true;
            computer->running = 0;
            return "terminate";
        } else if (e.type == SDL_KEYDOWN && ((selectedRenderer != 0 && selectedRenderer != 5) || keymap.find(e.key.keysym.sym) != keymap.end())) {
            Terminal * term = e.key.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.key.windowID, NULL)->term;
            SDLTerminal * sdlterm = dynamic_cast<SDLTerminal*>(term);
            if (e.key.keysym.sym == SDLK_F2 && (e.key.keysym.mod & ~(KMOD_CAPS | KMOD_NUM)) == 0 && sdlterm != NULL && !config.ignoreHotkeys) sdlterm->screenshot();
            else if (e.key.keysym.sym == SDLK_F3 && (e.key.keysym.mod & ~(KMOD_CAPS | KMOD_NUM)) == 0 && sdlterm != NULL && !config.ignoreHotkeys) sdlterm->toggleRecording();
#ifndef __EMSCRIPTEN__
            else if (e.key.keysym.sym == SDLK_F11 && (e.key.keysym.mod & ~(KMOD_CAPS | KMOD_NUM)) == 0 && sdlterm != NULL && !config.ignoreHotkeys && (std::string(SDL_GetCurrentVideoDriver()) != "KMSDRM" || std::string(SDL_GetCurrentVideoDriver()) == "KMSDRM_LEGACY")) sdlterm->toggleFullscreen(); // KMS must be fullscreen
#endif
            else if (e.key.keysym.sym == SDLK_F12 && (e.key.keysym.mod & ~(KMOD_CAPS | KMOD_NUM)) == 0 && sdlterm != NULL && !config.ignoreHotkeys) sdlterm->screenshot("clipboard");
            else if (e.key.keysym.sym == SDLK_F8 && ((e.key.keysym.mod & ~(KMOD_CAPS | KMOD_NUM)) == KMOD_LSYSMOD || (e.key.keysym.mod & ~(KMOD_CAPS | KMOD_NUM)) == KMOD_RSYSMOD) && sdlterm != NULL && !config.ignoreHotkeys) {
                sdlterm->isOnTop = !sdlterm->isOnTop;
                setFloating(sdlterm->win, sdlterm->isOnTop);
            } else if (((selectedRenderer == 0 || selectedRenderer == 5) ? e.key.keysym.sym == SDLK_t : e.key.keysym.sym == 20) && (e.key.keysym.mod & KMOD_CTRL)) {
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
            } else if (e.key.keysym.sym == SDLK_v && (e.key.keysym.mod & KMOD_SYSMOD) && SDL_HasClipboardText()) {
                char * text = SDL_GetClipboardText();
                std::string str;
                try {str = utf8_to_string(text, std::locale("C"));}
                catch (std::exception &e) {return "";}
                str = str.substr(0, min(str.find_first_of("\r\n"), (std::string::size_type)512));
                lua_pushlstring(L, str.c_str(), str.size());
                SDL_free(text);
                return "paste";
            } else computer->waitingForTerminate = 0;
            if (selectedRenderer != 0 && selectedRenderer != 5) lua_pushinteger(L, e.key.keysym.sym); 
            else lua_pushinteger(L, keymap.at(e.key.keysym.sym));
            lua_pushboolean(L, e.key.repeat);
            return "key";
        } else if (e.type == SDL_KEYUP && (selectedRenderer == 2 || selectedRenderer == 3 || keymap.find(e.key.keysym.sym) != keymap.end())) {
            if ((e.key.keysym.sym != SDLK_F2 && e.key.keysym.sym != SDLK_F3 && e.key.keysym.sym != SDLK_F11 && e.key.keysym.sym != SDLK_F12) || config.ignoreHotkeys) {
                computer->waitingForTerminate = 0;
                if (selectedRenderer != 0 && selectedRenderer != 5) lua_pushinteger(L, e.key.keysym.sym); 
                else lua_pushinteger(L, keymap.at(e.key.keysym.sym));
                return "key_up";
            }
        } else if (e.type == SDL_TEXTINPUT) {
            std::string str;
            try {str = utf8_to_string(e.text.text, std::locale("C"));}
            catch (std::exception &ignored) {str = "?";}
            if (!str.empty()) {
                lua_pushlstring(L, str.c_str(), 1);
                return "char";
            }
        } else if (e.type == SDL_MOUSEBUTTONDOWN && (computer->config->isColor || computer->isDebugger)) {
            std::string side;
            Terminal * term = e.button.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.button.windowID, &side)->term;
            int x = 1, y = 1;
            if (selectedRenderer >= 2 && selectedRenderer <= 4) {
                x = e.button.x; y = e.button.y;
            } else if (dynamic_cast<SDLTerminal*>(term) != NULL) {
                x = convertX(dynamic_cast<SDLTerminal*>(term), e.button.x); y = convertY(dynamic_cast<SDLTerminal*>(term), e.button.y);
            }
            if (term->lastMouse.x == x && term->lastMouse.y == y && term->lastMouse.button == e.button.button && term->lastMouse.event == 0) return "";
            if (e.button.windowID == computer->term->id || config.monitorsUseMouseEvents) {
                switch (e.button.button) {
                case SDL_BUTTON_LEFT: lua_pushinteger(L, 1); break;
                case SDL_BUTTON_RIGHT: lua_pushinteger(L, 2); break;
                case SDL_BUTTON_MIDDLE: lua_pushinteger(L, 3); break;
                default:
                    if (config.standardsMode) return "";
                    else lua_pushinteger(L, e.button.button);
                    break;
                }
            } else lua_pushstring(L, side.c_str());
            term->lastMouse = {x, y, e.button.button, 0, ""};
            term->mouseButtonOrder.push_back(e.button.button);
            lua_pushinteger(L, x);
            lua_pushinteger(L, y);
            if (e.button.windowID != computer->term->id && config.monitorsUseMouseEvents) lua_pushstring(L, side.c_str());
            return (e.button.windowID == computer->term->id || config.monitorsUseMouseEvents) ? "mouse_click" : "monitor_touch";
        } else if (e.type == SDL_MOUSEBUTTONUP && (computer->config->isColor || computer->isDebugger)) {
            std::string side;
            Terminal * term = e.button.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.button.windowID, &side)->term;
            int x = 1, y = 1;
            if (selectedRenderer >= 2 && selectedRenderer <= 4) {
                x = e.button.x; y = e.button.y;
            } else if (dynamic_cast<SDLTerminal*>(term) != NULL) {
                x = convertX(dynamic_cast<SDLTerminal*>(term), e.button.x); y = convertY(dynamic_cast<SDLTerminal*>(term), e.button.y);
            }
            if (term->lastMouse.x == x && term->lastMouse.y == y && term->lastMouse.button == e.button.button && term->lastMouse.event == 1) return "";
            switch (e.button.button) {
            case SDL_BUTTON_LEFT: lua_pushinteger(L, 1); break;
            case SDL_BUTTON_RIGHT: lua_pushinteger(L, 2); break;
            case SDL_BUTTON_MIDDLE: lua_pushinteger(L, 3); break;
            default:
                if (config.standardsMode) return "";
                else lua_pushinteger(L, e.button.button);
                break;
            }
            term->lastMouse = {x, y, e.button.button, 1, ""};
            term->mouseButtonOrder.remove(e.button.button);
            if (!(e.button.windowID == computer->term->id || config.monitorsUseMouseEvents)) {
                lua_pop(L, 1);
                return "";
            }
            lua_pushinteger(L, x);
            lua_pushinteger(L, y);
            if (e.button.windowID != computer->term->id && config.monitorsUseMouseEvents) lua_pushstring(L, side.c_str());
            return "mouse_up";
        } else if (e.type == SDL_MOUSEWHEEL && (computer->config->isColor || computer->isDebugger) && (e.wheel.windowID == computer->term->id || config.monitorsUseMouseEvents)) {
            std::string side;
            SDLTerminal * term = dynamic_cast<SDLTerminal*>(e.wheel.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.wheel.windowID, &side)->term);
            if (term == NULL) {
                return "";
            } else {
                int x = 0, y = 0;
                term->getMouse(&x, &y);
                lua_pushinteger(L, max(min(-e.wheel.y, 1), -1));
                lua_pushinteger(L, convertX(term, x));
                lua_pushinteger(L, convertY(term, y));
                if (e.wheel.windowID != computer->term->id && config.monitorsUseMouseEvents) lua_pushstring(L, side.c_str());
            }
            return "mouse_scroll";
        } else if (e.type == SDL_MOUSEMOTION && (config.mouse_move_throttle >= 0 || e.motion.state) && (computer->config->isColor || computer->isDebugger) && (e.motion.windowID == computer->term->id || config.monitorsUseMouseEvents)) {
            std::string side;
            SDLTerminal * term = dynamic_cast<SDLTerminal*>(e.motion.windowID == computer->term->id ? computer->term : findMonitorFromWindowID(computer, e.motion.windowID, &side)->term);
            if (term == NULL) return "";
            int x = 1, y = 1;
            if (selectedRenderer >= 2 && selectedRenderer <= 4) {
                x = e.motion.x; y = e.motion.y;
            } else if (term != NULL) {
                x = convertX(term, e.motion.x); y = convertY(dynamic_cast<SDLTerminal*>(term), e.motion.y);
            }
            std::list<Uint8> used_buttons;
            for (Uint8 i = 0; i < 32; i++) if (e.motion.state & (1 << i)) used_buttons.push_back(i + 1);
            for (auto it = term->mouseButtonOrder.begin(); it != term->mouseButtonOrder.end();) {
                auto pos = std::find(used_buttons.begin(), used_buttons.end(), *it);
                if (pos == used_buttons.end()) it = term->mouseButtonOrder.erase(it);
                else ++it;
            }
            Uint8 button = used_buttons.back();
            if (!term->mouseButtonOrder.empty()) button = term->mouseButtonOrder.back();
            if (button == SDL_BUTTON_MIDDLE) button = 3;
            else if (button == SDL_BUTTON_RIGHT) button = 2;
            if ((term->lastMouse.x == x && term->lastMouse.y == y && term->lastMouse.button == button && term->lastMouse.event == 2) || (config.standardsMode && button > 3)) return "";
            term->lastMouse = {x, y, button, 2, ""};
            if (config.mouse_move_throttle > 0 && !e.motion.state) {
                std::lock_guard<std::mutex> lock(term->mouseMoveLock);
                if (term->mouseMoveDebounceTimer == 0) {
                    term->mouseMoveDebounceTimer = SDL_AddTimer(config.mouse_move_throttle, mouseDebounce, new comp_term_pair {computer, term});
                    term->nextMouseMove = {0, 0, 0, 0, std::string()};
                } else {
                    term->nextMouseMove = {x, y, 0, 1, (e.motion.windowID != computer->term->id && config.monitorsUseMouseEvents) ? side : ""};
                    return "";
                }
            }
            lua_pushinteger(L, button);
            lua_pushinteger(L, x);
            lua_pushinteger(L, y);
            if (e.motion.windowID != computer->term->id && config.monitorsUseMouseEvents) lua_pushstring(L, side.c_str());
            return e.motion.state ? "mouse_drag" : "mouse_move";
        } else if (e.type == SDL_DROPFILE) {
            if (config.dropFilePath) {
                // Simply paste the file path
                // Look for a path relative to a mount; if not then just give it the whole thing
                path_t path = wstr(e.drop.file);
                std::string path_final = e.drop.file;
                path_t::iterator largestMatch = path.begin();
                {
                    auto match = std::mismatch(path.begin(), path.end(), computer->dataDir.begin());
                    if (match.first > largestMatch) {
                        path_final = astr(path_t(match.first + 1, path.end()));
                        largestMatch = match.first;
                    }
                }
                for (const auto& m : computer->mounts) {
                    auto match = std::mismatch(path.begin(), path.end(), std::get<1>(m).begin());
                    if (match.first > largestMatch) {
                        path_final = "";
                        for (const std::string& c : std::get<0>(m)) path_final += c + "/";
                        path_final += astr(path_t(match.first + 1, path.end()));
                        largestMatch = match.first;
                    }
                }
                lua_pushfstring(L, "%s ", astr(fixpath(computer, path_final, false, false)).c_str());
                SDL_free(e.drop.file);
                return "paste";
            } else {
                // Copy the file into the computer
                path_t path = fixpath(computer, basename(e.drop.file), false);
                struct_stat st;
                if (platform_stat(path.c_str(), &st) == 0) {
                    if (S_ISREG(st.st_mode)) {
                        SDLTerminal * term = dynamic_cast<SDLTerminal*>(computer->term);
                        if (term != NULL) {
                            SDL_MessageBoxButtonData buttons[] = {
                                {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No"},
                                {0, 1, "Yes"}
                            };
                            std::string text = std::string("A file named ") + basename(e.drop.file) + " already exists on this computer. Would you like to overwrite it?";
                            SDL_MessageBoxData msg = {
                                SDL_MESSAGEBOX_WARNING,
                                term->win,
                                "File already exists",
                                text.c_str(),
                                2,
                                buttons,
                                NULL
                            };
                            if (!queueTask([](void*msg)->void*{int b = 0; SDL_ShowMessageBox((SDL_MessageBoxData*)msg, &b); return (void*)(ptrdiff_t)b;}, &msg)) {
                                SDL_free(e.drop.file);
                                return "";
                            }
                        }
                    }
                }
                FILE * infile = fopen(e.drop.file, "rb");
                if (infile == NULL) {
                    char * err = strerror(errno);
                    char * msg = new char[strlen(err)+1];
                    strcpy(msg, err);
                    queueTask([computer](void*msg)->void*{computer->term->showMessage(SDL_MESSAGEBOX_ERROR, "Upload Failed", (std::string("The input file could not be read: ") + (const char*)msg + ".").c_str()); delete[] (char*)msg; return NULL;}, msg, true);
                    SDL_free(e.drop.file);
                    return "";
                }
                FILE * outfile = platform_fopen(path.c_str(), "wb");
                if (outfile == NULL) {
                    char * err = strerror(errno);
                    char * msg = new char[strlen(err)+1];
                    strcpy(msg, err);
                    queueTask([computer](void*msg)->void*{computer->term->showMessage(SDL_MESSAGEBOX_ERROR, "Upload Failed", (std::string("The output file could not be written: ") + (const char*)msg + ".").c_str()); delete[] (char*)msg; return NULL;}, msg, true);
                    fclose(infile);
                    SDL_free(e.drop.file);
                    return "";
                }
                char buf[4096];
                while (!feof(infile)) {
                    size_t sz = fread(buf, 1, 4096, infile);
                    fwrite(buf, 1, sz, outfile);
                    if (sz < 4096) break;
                }
                fclose(infile);
                fclose(outfile);
                computer->fileUploadCount++;
                SDL_free(e.drop.file);
                return "";
            }
        } else if ((e.type == SDL_DROPBEGIN || e.type == SDL_DROPCOMPLETE) && !config.dropFilePath) {
            int c = computer->fileUploadCount;
            if (e.type == SDL_DROPCOMPLETE && computer->fileUploadCount)
                queueTask([computer, c](void*)->void*{computer->term->showMessage(SDL_MESSAGEBOX_INFORMATION, "Upload Succeeded", (std::to_string(c) + " files uploaded.").c_str()); return NULL;}, NULL, true);
            computer->fileUploadCount = 0;
            return "";
        } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
            unsigned w, h;
            Terminal * term = NULL;
            std::string side;
            if (computer->term != NULL && computer->term->id == e.window.windowID) term = computer->term;
            if (term == NULL) {
                monitor * m = findMonitorFromWindowID(computer, e.window.windowID, &side);
                if (m != NULL) term = m->term;
            }
            if (term == NULL) return "";
            if (selectedRenderer == 0 || selectedRenderer == 5) {
                SDLTerminal * sdlterm = dynamic_cast<SDLTerminal*>(term);
                if (sdlterm != NULL) {
                    if ((unsigned)e.window.data1 < 4*sdlterm->charScale*sdlterm->dpiScale) w = 0;
                    else w = (e.window.data1 - 4*sdlterm->charScale*sdlterm->dpiScale) / (sdlterm->charWidth*sdlterm->dpiScale);
                    if ((unsigned)e.window.data2 < 4*sdlterm->charScale*sdlterm->dpiScale) h = 0;
                    else h = (e.window.data2 - 4*sdlterm->charScale*sdlterm->dpiScale) / (sdlterm->charHeight*sdlterm->dpiScale);
                } else {w = 51; h = 19;}
            } else {w = e.window.data1; h = e.window.data2;}
            term->resize(w, h);
            if (computer->term == term) return "term_resize";
            else {
                lua_pushstring(L, side.c_str());
                return "monitor_resize";
            }
        } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {
            if (e.window.windowID == computer->term->id) {
                computer->requestedExit = true;
                computer->running = 0;
                return "terminate";
            } else {
                std::string side;
                monitor * m = findMonitorFromWindowID(computer, e.window.windowID, &side);
                if (m != NULL) detachPeripheral(computer, side);
            }
        } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_LEAVE && config.mouse_move_throttle >= 0 && (e.button.windowID == computer->term->id || config.monitorsUseMouseEvents)) {
            Terminal * term = NULL;
            std::string side;
            if (computer->term != NULL && computer->term->id == e.window.windowID) term = computer->term;
            if (term == NULL) {
                monitor * m = findMonitorFromWindowID(computer, e.window.windowID, &side);
                if (m != NULL) term = m->term;
            }
            if (term == NULL) return "";
            if (term->mouseMoveDebounceTimer != 0) {
                SDL_RemoveTimer(term->mouseMoveDebounceTimer);
                term->mouseMoveDebounceTimer = 0;
                term->nextMouseMove = {0, 0, 0, 0, std::string() };
            }
            lua_pushinteger(L, 1);
            lua_pushnil(L);
            lua_pushnil(L);
            if (e.window.windowID != computer->term->id && config.monitorsUseMouseEvents) lua_pushstring(L, side.c_str());
            return "mouse_move";
        }
    }
    return "";
}

void displayFailure(Terminal * term, const std::string& message, const std::string& extra) {
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

Terminal * createTerminal(const std::string& title) {
    if (selectedRenderer >= terminalFactories.size()) return NULL;
    TerminalFactory * factory = terminalFactories[selectedRenderer];
    if (factory == NULL) return NULL;
    return factory->createTerminal(title);
}
