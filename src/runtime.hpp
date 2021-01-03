/*
 * runtime.hpp
 * CraftOS-PC 2
 * 
 * This file defines some common methods for the CraftOS-PC runtime.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#ifndef RUNTIME_HPP
#define RUNTIME_HPP
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <SDL2/SDL.h>
#include "util.hpp"

extern ProtectedObject<std::vector<Computer*> > computers;
extern ProtectedObject<std::unordered_set<SDL_TimerID> > freedTimers;
extern bool exiting;
extern int selectedRenderer;
extern std::unordered_map<int, path_t> customDataDirs;
extern std::list<path_t> customPlugins;
extern std::list<std::tuple<std::string, std::string, int> > customMounts;
extern path_t computerDir;
extern std::unordered_set<Computer*> freedComputers;
extern std::list<std::thread*> computerThreads;
extern std::thread::id mainThreadID;

extern int getNextEvent(lua_State* L, const std::string& filter);
extern void* queueTask(const std::function<void*(void*)>& func, void* arg, bool async = false);
extern void runComputer(Computer * self, const path_t& bios_name);
extern bool Computer_getEvent(Computer * self, SDL_Event* e);
extern void* computerThread(void* data);
extern Computer* startComputer(int id);
extern void queueEvent(Computer *comp, const event_provider& p, void* data);
extern bool addMount(Computer *comp, const path_t& real_path, const char * comp_path, bool read_only);
extern bool addVirtualMount(Computer * comp, const FileEntry& vfs, const char * comp_path);
extern void registerPeripheral(const std::string& name, const peripheral_init& initializer);
extern void registerSDLEvent(SDL_EventType type, const sdl_event_handler& handler, void* userdata);
extern void mainLoop();
extern std::unordered_map<path_t, std::string> initializePlugins();
extern void loadPlugins(Computer * comp);
extern void deinitializePlugins();
extern void stopWebsocket(void*);

#endif