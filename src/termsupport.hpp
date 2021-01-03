/*
 * termsupport.hpp
 * CraftOS-PC 2
 * 
 * This file defines some functions that interact with the terminal.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#ifndef TERMSUPPORT_HPP
#define TERMSUPPORT_HPP
#include <atomic>
#include "peripheral/monitor.hpp"
#include "terminal/SDLTerminal.hpp"
#include "util.hpp"

extern std::thread * renderThread;
extern std::unordered_set<Terminal*> orphanedTerminals;
extern ProtectedObject<std::queue< std::tuple<int, std::function<void*(void*)>, void*, bool> > > taskQueue;
extern ProtectedObject<std::unordered_map<int, void*> > taskQueueReturns;
extern std::atomic_bool taskQueueReady;
extern std::condition_variable taskQueueNotify;
extern std::unordered_map<int, unsigned char> keymap;
extern std::unordered_map<int, unsigned char> keymap_cli;
extern Uint32 task_event_type;
extern Uint32 render_event_type;

extern std::string termGetEvent(lua_State *L);
extern int buttonConvert(Uint8 button);
extern int buttonConvert2(Uint32 state);
extern int convertX(SDLTerminal *term, int x);
extern int convertY(SDLTerminal *term, int y);
extern void termRenderLoop();
extern void termHook(lua_State *L, lua_Debug *ar);
extern int termPanic(lua_State *L);
extern monitor * findMonitorFromWindowID(Computer *comp, unsigned id, std::string& sideReturn);
extern void displayFailure(Terminal * term, const std::string& message, const std::string& extra = "");

#endif