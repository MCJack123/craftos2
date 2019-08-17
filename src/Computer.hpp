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
#include <lua.hpp>
#include <string>
#include <vector>
#include <tuple>
#include <list>
#include <queue>
#include <unordered_map>
#include "peripheral/peripheral.hpp"
#include "TerminalWindow.hpp"

typedef const char * (*event_provider)(lua_State *L, void* data);

class Computer {
public:
    int id;
    int running = 0;
    int files_open = 0;
    std::vector<std::tuple<std::list<std::string>, std::string, bool> > mounts;
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
    lua_State *L = NULL;

    Computer(int i);
    void run();
    ~Computer();
};

extern std::vector<Computer*> computers;
extern void* computerThread(void* data);

#endif