/*
 * peripheral/debugger.hpp
 * CraftOS-PC 2
 * 
 * This file defines the class for the debugger peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2024 JackMacWindows. 
 */

#ifndef PERIPHERAL_DEBUGGER_HPP
#define PERIPHERAL_DEBUGGER_HPP
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <peripheral.hpp>

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
    friend void debuggerThread(Computer*, void*, std::string);
    friend void forwardInput();
    friend class DAPConnection;
private:
    bool deleteThis = false;
    int _break(lua_State *L);
    int setBreakpoint(lua_State *L);
    int print(lua_State *L);
    void init(Computer * comp);
    int _deinit(lua_State *L);
    library_t * createDebuggerLibrary();
    static library_t methods;
protected:
    std::thread * compThread = NULL;
    Computer * monitor = NULL;
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
    bool waitingForBreak = false;
    std::string breakReason;
    std::mutex breakMutex;
    std::condition_variable breakNotify;
    lua_State * volatile thread = NULL;
    std::unordered_map<std::string, std::unordered_map<std::string, profile_entry > > profile;
    bool isProfiling = false;
    static peripheral * _init(lua_State *L, const char * side) {return new debugger(L, side);}
    static void deinit(peripheral * p) {delete (debugger*)p;}
    virtual destructor getDestructor() const override {return deinit;}
    debugger(lua_State *L, const char * side);
    virtual ~debugger();
    virtual int call(lua_State *L, const char * method) override;
    library_t getMethods() const override {return methods;}
    void reinitialize(lua_State *L) override;
    void resetMounts();
};

#endif