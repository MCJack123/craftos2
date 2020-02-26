/*
 * peripheral/debugger.hpp
 * CraftOS-PC 2
 * 
 * This file defines the class for the debugger peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2020 JackMacWindows. 
 */

#ifndef PERIPHERAL_DEBUGGER_HPP
#define PERIPHERAL_DEBUGGER_HPP
#include "peripheral.hpp"
#include <mutex>
#include <condition_variable>

#define DEBUGGER_BREAK_TYPE_NONSTOP 0
#define DEBUGGER_BREAK_TYPE_LINE    1
#define DEBUGGER_BREAK_TYPE_RETURN  2

#define DEBUGGER_BREAK_FUNC_ERROR   1
#define DEBUGGER_BREAK_FUNC_LOAD    2
#define DEBUGGER_BREAK_FUNC_RUN     4
#define DEBUGGER_BREAK_FUNC_RESUME  8
#define DEBUGGER_BREAK_FUNC_YIELD   16

class debugger: public peripheral {
    friend int debugger_lib_getInfo(lua_State *L);
    friend void debuggerThread(Computer*, debugger*, std::string);
private:
    bool deleteThis = false;
    Computer * monitor;
    std::thread * compThread;
    int _break(lua_State *L);
    int setBreakpoint(lua_State *L);
    int print(lua_State *L);
    void init(Computer * comp);
    int _deinit(lua_State *L);
    library_t * createDebuggerLibrary();
    static library_t methods;
public:
    struct profile_entry {
        bool running;
        unsigned long count;
        std::chrono::high_resolution_clock::time_point start;
        std::chrono::high_resolution_clock::duration time;
    };
    std::atomic_bool running;
    Computer * computer;
    int breakType = DEBUGGER_BREAK_TYPE_NONSTOP;
    int stepCount = 0;
    int breakMask = 0;
    std::string breakFunc;
    bool didBreak = false;
    bool confirmBreak = false;
    std::string breakReason;
    std::mutex breakMutex;
    std::condition_variable breakNotify;
    lua_State * volatile thread = NULL;
    std::unordered_map<std::string, std::unordered_map<std::string, profile_entry > > profile;
    bool isProfiling = false;
    static void deinit(peripheral * p) {delete (debugger*)p;}
    destructor getDestructor() {return deinit;}
    debugger(lua_State *L, const char * side);
    ~debugger();
    void update(){}
    int call(lua_State *L, const char * method);
    library_t getMethods() {return methods;}
};

#endif