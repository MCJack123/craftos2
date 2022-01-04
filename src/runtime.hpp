/*
 * runtime.hpp
 * CraftOS-PC 2
 * 
 * This file defines some common methods for the CraftOS-PC runtime.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#ifndef RUNTIME_HPP
#define RUNTIME_HPP
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <SDL2/SDL.h>
#include "util.hpp"

struct TaskQueueItem {
    std::function<void*(void*)> func;
    void* data = NULL;
    bool async = false;
    bool ready = false;
    std::mutex lock;
    std::condition_variable notify;
    std::exception_ptr exception = nullptr;
};

extern ProtectedObject<std::vector<Computer*> > computers;
extern ProtectedObject<std::unordered_set<SDL_TimerID> > freedTimers;
extern ProtectedObject<std::queue<TaskQueueItem*> > taskQueue;
extern bool exiting;
extern int selectedRenderer;
extern std::unordered_map<int, path_t> customDataDirs;
extern std::list<path_t> customPlugins;
extern std::list<std::tuple<std::string, std::string, int> > customMounts;
extern path_t computerDir;
extern std::unordered_set<Computer*> freedComputers;
extern std::list<std::thread*> computerThreads;
extern std::thread::id mainThreadID;
extern bool listenerMode;
extern std::mutex listenerModeMutex;
extern std::condition_variable listenerModeNotify;

extern int getNextEvent(lua_State* L, const std::string& filter);
extern void* queueTask(const std::function<void*(void*)>& func, void* arg, bool async = false);
extern void runComputer(Computer * self, const path_t& bios_name);
extern bool Computer_getEvent(Computer * self, SDL_Event* e);
extern Uint32 eventTimeoutEvent(Uint32 interval, void* param);
extern void* computerThread(void* data);
extern Computer* startComputer(int id);
extern void queueEvent(Computer *comp, const event_provider& p, void* data);
extern bool addMount(Computer *comp, const path_t& real_path, const char * comp_path, bool read_only);
extern bool addVirtualMount(Computer * comp, const FileEntry& vfs, const char * comp_path);
extern void registerPeripheral(const std::string& name, const peripheral_init_fn& initializer);
extern void registerSDLEvent(SDL_EventType type, const sdl_event_handler& handler, void* userdata);
extern void pumpTaskQueue();
extern void defaultPollEvents();
extern void mainLoop();
extern void preloadPlugins();
extern std::unordered_map<path_t, std::string> initializePlugins();
extern void loadPlugins(Computer * comp);
extern void deinitializePlugins();
extern void unloadPlugins();
extern void stopWebsocket(void*);
extern peripheral* attachPeripheral(Computer * computer, const std::string& side, const std::string& type, std::string * errorReturn, const char * format, ...);
extern bool detachPeripheral(Computer * computer, const std::string& side);
extern void addEventHook(const std::string& event, Computer * computer, const event_hook& hook, void* userdata);

#endif
