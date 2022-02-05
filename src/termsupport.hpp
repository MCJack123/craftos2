/*
 * termsupport.hpp
 * CraftOS-PC 2
 * 
 * This file defines some functions that interact with the terminal.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#ifndef TERMSUPPORT_HPP
#define TERMSUPPORT_HPP
#include <atomic>
#include "peripheral/monitor.hpp"
#include "terminal/SDLTerminal.hpp"
#include "util.hpp"

#ifdef __APPLE__
#define KMOD_LSYSMOD KMOD_LGUI
#define KMOD_RSYSMOD KMOD_RGUI
#define KMOD_SYSMOD KMOD_GUI
#else
#define KMOD_LSYSMOD KMOD_LCTRL
#define KMOD_RSYSMOD KMOD_RCTRL
#define KMOD_SYSMOD KMOD_CTRL
#endif

extern std::thread * renderThread;
extern std::unordered_set<Terminal*> orphanedTerminals;
extern std::atomic_bool taskQueueReady;
extern std::condition_variable taskQueueNotify;
extern std::unordered_map<int, unsigned char> keymap;
extern std::unordered_map<int, unsigned char> keymap_cli;
extern Uint32 task_event_type;
extern Uint32 render_event_type;
extern bool singleWindowMode;
extern std::list<Terminal*> renderTargets;
extern std::mutex renderTargetsLock;
extern std::list<Terminal*>::iterator renderTarget;
extern std::set<unsigned> currentWindowIDs;
extern std::vector<TerminalFactory *> terminalFactories;

extern Terminal * createTerminal(const std::string& title);
extern std::string termGetEvent(lua_State *L);
extern int convertX(SDLTerminal *term, int x);
extern int convertY(SDLTerminal *term, int y);
extern void termRenderLoop();
extern void termHook(lua_State *L, lua_Debug *ar);
extern int termPanic(lua_State *L);
extern monitor * findMonitorFromWindowID(Computer *comp, unsigned id, std::string* sideReturn);
extern void displayFailure(Terminal * term, const std::string& message, const std::string& extra = "");

inline bool checkWindowID(Computer * c, unsigned wid) {
    if (singleWindowMode) return c->term == *renderTarget || findMonitorFromWindowID(c, (*renderTarget)->id, NULL) != NULL;
    return wid == c->term->id || findMonitorFromWindowID(c, wid, NULL) != NULL;
}

inline std::list<Terminal*>::iterator& nextRenderTarget() {
    std::lock_guard<std::mutex> lock(renderTargetsLock);
    if (++renderTarget == renderTargets.end()) renderTarget = renderTargets.begin();
    (*renderTarget)->changed = true;
    (*renderTarget)->onActivate();
    return renderTarget;
}

inline std::list<Terminal*>::iterator& previousRenderTarget() {
    std::lock_guard<std::mutex> lock(renderTargetsLock);
    if (renderTarget == renderTargets.begin()) renderTarget = renderTargets.end();
    --renderTarget;
    (*renderTarget)->changed = true;
    (*renderTarget)->onActivate();
    return renderTarget;
}

#endif