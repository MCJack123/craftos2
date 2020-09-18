/*
 * Computer.hpp
 * CraftOS-PC 2
 * 
 * This file defines the Computer class, which stores the state of each running
 * computer.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef COMPUTER_HPP
#define COMPUTER_HPP
extern "C" {
#include <lua.h>
}
#include <string>
#include <vector>
#include <tuple>
#include <list>
#include <queue>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <condition_variable>
#include <csetjmp>
#include <SDL2/SDL.h>
#ifndef NO_CLI
#include <ncurses.h>
#include <panel.h>
#endif
#include "peripheral/peripheral.hpp"
#include "terminal/Terminal.hpp"
#include "config.hpp"
#ifdef WIN32
typedef std::wstring path_t;
#else
typedef std::string path_t;
#endif

typedef const char * (*event_provider)(lua_State *L, void* data);

extern "C" struct mouse_event_data {
    int x;
    int y;
    uint8_t button;
    uint8_t event;
    std::string side;
};

struct Computer {
    int id;
    int running = 0;
    int files_open = 0;
    std::vector< std::tuple<std::list<std::string>, path_t, bool> > mounts;
    bool mounter_initializing = false;
    std::queue<std::string> eventQueue;
    lua_State * paramQueue;
    std::vector<double> alarms;
    std::unordered_map<std::string, peripheral*> peripherals;
    std::mutex peripherals_mutex;
    std::queue<std::pair<event_provider, void*> > event_provider_queue;
    std::mutex event_provider_queue_mutex;
    Terminal * term;
#ifndef NO_CLI
    PANEL * cli_panel;
    WINDOW * cli_term;
#endif
    unsigned char colors = 0xF0;
    std::chrono::high_resolution_clock::time_point last_event = std::chrono::high_resolution_clock::now();
    bool getting_event = false;
    bool lastResizeEvent = false;
    int waitingForTerminate = 0;
    std::queue<SDL_Event> termEventQueue;
    lua_State *L = NULL;
    std::list<Computer*> referencers;
    struct computer_configuration config;
    std::unordered_map<int, void *> userdata;
    std::condition_variable event_lock;
    std::chrono::system_clock::time_point system_start = std::chrono::system_clock::now();
    jmp_buf on_panic;
    std::map< int, std::pair<std::string, lua_Integer> > breakpoints;
    void * debugger = NULL;
    bool isDebugger = false;
    int hookMask = 0;
    std::unordered_set<SDL_TimerID> timerIDs;
    std::vector<void*> openWebsockets;
    SDL_TimerID eventTimeout = 0;
    bool hasBreakpoints = false;
    bool shouldDeinitDebugger = false;
    int timeoutCheckCount = 0;
    std::unordered_set<int> usedDriveMounts;
    lua_State *coro;
    mouse_event_data lastMouse = {-1, -1, 0, 16, std::string()};
    SDL_TimerID mouseMoveDebounceTimer = 0;
    mouse_event_data nextMouseMove = {0, 0, 0, 0, std::string()};
    std::unordered_map<int, std::function<void(Computer*, int, void*)> > userdata_destructors;
    path_t dataDir;
    std::mutex termEventQueueMutex;

    static std::unordered_map<int, path_t> customDataDirs;
    static std::list<path_t> customPlugins;
    static std::list<std::tuple<std::string, std::string, int> > customMounts;
    Computer(int i): Computer(i, false) {}
    Computer(int i, bool debug);
    ~Computer();
    void run(path_t bios_name);
    bool getEvent(SDL_Event* e);
private:
    void loadPlugin(path_t path);
};

extern std::vector<Computer*> computers;
extern ProtectedObject<std::unordered_set<SDL_TimerID> > freedTimers;
extern path_t computerDir;
extern void* computerThread(void* data);
extern Computer* startComputer(int id);

#endif