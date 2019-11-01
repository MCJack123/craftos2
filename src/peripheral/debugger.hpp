/*
 * peripheral/debugger.hpp
 * CraftOS-PC 2
 * 
 * This file defines the class for the debugger peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019 JackMacWindows. 
 */

#ifndef PERIPHERAL_DEBUGGER_HPP
#define PERIPHERAL_DEBUGGER_HPP
#include "peripheral.hpp"
#include <mutex>
#include <condition_variable>

#define DEBUGGER_BREAK_TYPE_NONSTOP 0
#define DEBUGGER_BREAK_TYPE_LINE    1
#define DEBUGGER_BREAK_TYPE_RETURN  2

class debugger: public peripheral {
    friend int debugger_lib_getInfo(lua_State *L);
private:
    bool running = true;
    Computer * monitor;
    std::thread compThread;
    void run();
    int _break(lua_State *L);
    int setBreakpoint(lua_State *L);
    void init(Computer * comp);
    library_t * createDebuggerLibrary();
    static library_t methods;
public:
    Computer * computer;
    int breakType = DEBUGGER_BREAK_TYPE_NONSTOP;
    bool didBreak = false;
    std::condition_variable breakNotify;
    std::condition_variable runNotify;
    lua_Debug * last_info = NULL;
    debugger(lua_State *L, const char * side);
    ~debugger();
    void update(){}
    int call(lua_State *L, const char * method);
    library_t getMethods() {return methods;}
};

#endif