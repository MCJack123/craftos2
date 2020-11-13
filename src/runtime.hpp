/*
 * runtime.hpp
 * CraftOS-PC 2
 * 
 * This file defines some common methods for the CraftOS-PC runtime.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef RUNTIME_HPP
#define RUNTIME_HPP
#include "util.hpp"
#include <unordered_map>
#include <unordered_set>
#include <SDL2/SDL.h>

typedef std::string (*event_provider)(lua_State *L, void* data);

extern ProtectedObject<std::vector<Computer*> > computers;
extern ProtectedObject<std::unordered_set<SDL_TimerID> > freedTimers;
extern bool exiting;
extern int selectedRenderer;

extern int getNextEvent(lua_State* L, std::string filter);
extern void* queueTask(std::function<void*(void*)> func, void* arg, bool async = false);
extern std::unordered_map<int, path_t> customDataDirs;
extern std::list<path_t> customPlugins;
extern std::list<std::tuple<std::string, std::string, int> > customMounts;
extern void runComputer(Computer * self, path_t bios_name);
extern bool Computer_getEvent(Computer * self, SDL_Event* e);
extern void Computer_loadPlugin(Computer * self, path_t path);
extern path_t computerDir;
extern void* computerThread(void* data);
extern Computer* startComputer(int id);
extern void queueEvent(Computer *comp, event_provider p, void* data);
extern bool addMount(Computer *comp, path_t real_path, const char * comp_path, bool read_only);
#endif