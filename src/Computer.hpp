/*
 * Computer.hpp
 * CraftOS-PC 2
 * 
 * This file defines the Computer class, which stores the state of each running
 * computer.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
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
#include <unordered_map>
#include <atomic>
#ifdef WIN32
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#include "peripheral/peripheral.hpp"
#include "TerminalWindow.hpp"
#include "config.hpp"

typedef const char * (*event_provider)(lua_State *L, void* data);

class Computer {
public:
    int id;
    int running = 0;
    int files_open = 0;
    std::vector< std::tuple<std::list<std::string>, std::string, bool> > mounts;
    bool mounter_initializing = false;
    std::queue<std::string> eventQueue;
    lua_State * paramQueue;
    std::vector<std::chrono::steady_clock::time_point> timers;
    std::vector<double> alarms;
    std::unordered_map<std::string, peripheral*> peripherals;
    std::queue<std::pair<event_provider, void*> > event_provider_queue;
    TerminalWindow * term;
    bool canBlink = true;
    unsigned char colors = 0xF0;
    std::chrono::high_resolution_clock::time_point last_blink = std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point last_event = std::chrono::high_resolution_clock::now();
    bool getting_event = false;
    bool lastResizeEvent = false;
    int waitingForTerminate = 0;
    std::queue<SDL_Event> termEventQueue;
    lua_State *L = NULL;
    std::list<Computer*> referencers;
    struct computer_configuration config;
    std::unordered_map<int, void *> userdata;

    Computer(int i);
    ~Computer();
    void run();
    bool getEvent(SDL_Event* e);
};

extern std::vector<Computer*> computers;
extern void* computerThread(void* data);
extern Computer* startComputer(int id);

#endif