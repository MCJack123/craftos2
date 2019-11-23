/*
 * peripheral/debugger.cpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the debugger peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "debugger.hpp"
#include "../os.hpp"
#include "../CLITerminalWindow.hpp"
#include "../term.hpp"
#include "../mounter.hpp"
#include <sstream>
#include <functional>
#include <unordered_set>
#include <cassert>
extern "C" {
#include <lauxlib.h>
}

extern bool cli;
extern std::list<std::thread*> computerThreads;
extern std::unordered_set<Computer*> freedComputers;
extern std::thread::id mainThreadID;

void debuggerThread(Computer * comp, debugger * dbg, std::string side) {
#ifdef __APPLE__
    pthread_setname_np(std::string("Computer " + std::to_string(comp->id) + " Thread (Debugger)").c_str());
#endif
    comp->run("debug/bios.lua");
    freedComputers.insert(comp);
    for (auto it = computers.begin(); it != computers.end(); it++) {
        if (*it == comp) {
            it = computers.erase(it);
            if (it == computers.end()) break;
        }
    }
    delete (library_t*)comp->debugger;
    if (!dbg->deleteThis) {
        dbg->computer->peripherals_mutex.lock();
        dbg->computer->peripherals.erase(side);
        dbg->computer->peripherals_mutex.unlock();
        queueTask([comp](void*arg)->void*{delete (debugger*)arg; delete comp; return NULL;}, dbg, true);
    }
}

const char * debugger_break(lua_State *L, void* userp) {return "debugger_break";}

int debugger_lib_waitForBreak(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    Computer * comp = get_comp(L);
    std::thread th([dbg](Computer*comp){
        dbg->didBreak = false;
        dbg->breakNotify.notify_all();
        std::unique_lock<std::mutex> lock(dbg->breakMutex);
        dbg->breakNotify.wait(lock);
        dbg->didBreak = true;
        dbg->confirmBreak = false;
        while (!dbg->confirmBreak) {
        if (freedComputers.find(comp) == freedComputers.end())
            termQueueProvider(comp, debugger_break, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
    }, comp);
    th.detach();
    return 0;
}

int debugger_lib_confirmBreak(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    dbg->confirmBreak = true;
    return 0;
}

int debugger_lib_step(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    dbg->breakType = DEBUGGER_BREAK_TYPE_LINE;
    dbg->stepCount = lua_isnumber(L, 1) ? lua_tointeger(L, 1) : 0;
    return 0;
}

int debugger_lib_continue(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    dbg->breakType = DEBUGGER_BREAK_TYPE_NONSTOP;
    return 0;
}

int debugger_lib_stepOut(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    if (dbg->thread == NULL) {
        lua_pushboolean(L, false);
        return 1;
    }
    lua_Debug ar;
    lua_getstack(dbg->thread, 0, &ar);
    lua_getinfo(dbg->thread, "nSl", &ar);
    if (ar.name != NULL) {
        dbg->breakType = DEBUGGER_BREAK_TYPE_RETURN;
        dbg->breakFunc = ar.name;
    }
    lua_pushboolean(L, ar.name != NULL);
    return 1;
}

static void settabss (lua_State *L, const char *i, const char *v) {
  lua_pushstring(L, v);
  lua_setfield(L, -2, i);
}


static void settabsi (lua_State *L, const char *i, int v) {
  lua_pushinteger(L, v);
  lua_setfield(L, -2, i);
}

int debugger_lib_getInfo(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    if (dbg->thread == NULL) {
        lua_pushnil(L);
        return 1;
    }
    lua_Debug ar;
    if (!lua_getstack(dbg->thread, lua_isnumber(L, 1) ? lua_tointeger(L, 1) : 0, &ar)) lua_pushnil(L);
    else {
        lua_getinfo(dbg->thread, "nSl", &ar);
        lua_createtable(L, 0, 2);
        settabss(L, "source", ar.source);
        settabss(L, "short_src", ar.short_src);
        settabsi(L, "linedefined", ar.linedefined);
        settabsi(L, "lastlinedefined", ar.lastlinedefined);
        settabss(L, "what", ar.what);
        settabsi(L, "currentline", ar.currentline);
        settabss(L, "name", ar.name);
        settabss(L, "namewhat", ar.namewhat);
    }
    return 1;
}

int debugger_lib_setBreakpoint(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    int id = dbg->computer->breakpoints.size() > 0 ? dbg->computer->breakpoints.rbegin()->first + 1 : 1;
    dbg->computer->breakpoints[id] = std::make_pair("@/" + fixpath(dbg->computer, lua_tostring(L, 1), false), lua_tointeger(L, 2));
    lua_pushinteger(L, id);
    return 1;
}

int debugger_lib_unsetBreakpoint(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    if (dbg->computer->breakpoints.find(lua_tointeger(L, 1)) != dbg->computer->breakpoints.end()) {
        dbg->computer->breakpoints.erase(lua_tointeger(L, 1));
        lua_pushboolean(L, true);
    } else lua_pushboolean(L, false);
    return 1;
}

int debugger_lib_listBreakpoints(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_newtable(L);
    for (auto it = dbg->computer->breakpoints.begin(); it != dbg->computer->breakpoints.end(); it++) {
        lua_pushinteger(L, it->first);
        lua_newtable(L);
        lua_pushstring(L, "file");
        lua_pushstring(L, it->second.first.c_str());
        lua_settable(L, -3);
        lua_pushstring(L, "line");
        lua_pushinteger(L, it->second.second);
        lua_settable(L, -3);
        lua_settable(L, -3);
    }
    return 1;
}

// int debugger_lib_run(lua_State *L) {
//     lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
//     debugger * dbg = (debugger*)lua_touserdata(L, -1);
//     lua_Debug ar;
//     lua_settop(L, 1);
//     lua_State *coro = lua_newthread(dbg->thread);
//     luaL_loadstring(coro, lua_tostring(L, 1));
//     luaL_loadstring(dbg->thread, "return setmetatable({_echo = function(...) return ... end, getfenv = getfenv, locals = ..., _ENV = getfenv(2)}, {__index = getfenv(2)})");
//     lua_newtable(dbg->thread);
//     lua_getstack(dbg->thread, 1, &ar);
//     const char * name;
//     for (int i = 1; (name = lua_getlocal(dbg->thread, &ar, i)) != NULL; i++) {
//         if (std::string(name) == "(*temporary)") {
//             lua_pop(dbg->thread, 1);
//             continue;
//         }
//         lua_setfield(dbg->thread, -2, name);
//     }
//     lua_call(dbg->thread, 1, 1);
//     lua_pushvalue(dbg->thread, -1);
//     lua_setfenv(dbg->thread, -3);
//     lua_xmove(dbg->thread, coro, 1);
//     lua_setfenv(coro, -2);
//     int status = lua_resume(coro, 0);
//     int narg;
//     while (status == LUA_YIELD) {
//         if (lua_isstring(coro, -1)) narg = getNextEvent(coro, std::string(lua_tostring(coro, -1), lua_strlen(coro, -1)));
//         else narg = getNextEvent(coro, "");
//         status = lua_resume(coro, narg);
//     }
//     lua_pushboolean(L, status == 0);
//     int top2 = lua_gettop(coro);
//     lua_xmove(coro, L, top2);
//     lua_pop(dbg->thread, 1);
//     return top2 + 1;
// }

int debugger_lib_run(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_Debug ar;
    lua_settop(L, 1);
    int top = lua_gettop(dbg->thread); // ...
    luaL_loadstring(dbg->thread, lua_tostring(L, 1)); // ..., func
    luaL_loadstring(dbg->thread, "return setmetatable({_echo = function(...) return ... end, getfenv = getfenv, locals = ..., _ENV = getfenv(2)}, {__index = getfenv(2)})"); // ..., func, getenv
    lua_newtable(dbg->thread); // ..., func, getenv, table
    lua_getstack(dbg->thread, 1, &ar); // ..., func, getenv, table
    const char * name;
    for (int i = 1; (name = lua_getlocal(dbg->thread, &ar, i)) != NULL; i++) { // ..., func, getenv, table, local
        if (std::string(name) == "(*temporary)") {
            lua_pop(dbg->thread, 1); // ..., func, getenv, table
            continue;
        }
        lua_setfield(dbg->thread, -2, name); // ..., func, getenv, table
    }
    lua_call(dbg->thread, 1, 1); // ..., func, env
    lua_setfenv(dbg->thread, -2); // ..., func
    lua_pushboolean(L, !lua_pcall(dbg->thread, 0, LUA_MULTRET, 0)); // ..., results...
    int top2 = lua_gettop(dbg->thread) - top; // #{..., results...} - #{...} = #{results...}
    lua_xmove(dbg->thread, L, top2); // ...
    return top2 + 1;
}

int debugger_lib_status(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_pushboolean(L, dbg->thread != NULL);
    if (dbg->breakType == DEBUGGER_BREAK_TYPE_NONSTOP) lua_pushstring(L, "nonstop");
    else if (dbg->breakType == DEBUGGER_BREAK_TYPE_LINE) lua_pushstring(L, "line");
    else if (dbg->breakType == DEBUGGER_BREAK_TYPE_RETURN) lua_pushstring(L, "return");
    else lua_pushnil(L);
    return 2;
}

int debugger_lib_startProfiling(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    if (lua_toboolean(L, 1)) dbg->profile.clear();
    dbg->isProfiling = lua_toboolean(L, 1);
    return 0;
}

int debugger_lib_profile(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_newtable(L);
    for (auto it = dbg->profile.begin(); it != dbg->profile.end(); it++) {
        if (it->second.size() > 0) {
            lua_pushstring(L, it->first.c_str());
            lua_newtable(L);
            for (auto itt = it->second.begin(); itt != it->second.end(); itt++) {
                lua_pushstring(L, itt->first.c_str());
                lua_newtable(L);
                lua_pushinteger(L, std::get<0>(itt->second));
                lua_setfield(L, -2, "count");
                lua_pushnumber(L, std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(std::get<2>(itt->second)).count());
                lua_setfield(L, -2, "time");
                lua_settable(L, -3);
            }
            lua_settable(L, -3);
        }
    }
    return 1;
}

int debugger_lib_getfenv(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_Debug ar;
    lua_getstack(dbg->thread, 0, &ar);
    lua_getinfo(dbg->thread, "f", &ar);
    lua_getfenv(dbg->thread, -1);
    lua_xmove(dbg->thread, L, 1);
    lua_pop(dbg->thread, 1);
    return 1;
}

int debugger_lib_unblock(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    termQueueProvider(dbg->computer, debugger_break, NULL);
    return 0;
}

int debugger_lib_getReason(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_pushstring(L, dbg->breakReason.c_str());
    return 1;
}

int debugger_lib_getLocals(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_Debug ar;
    lua_getstack(dbg->thread, 1, &ar);
    lua_newtable(L);
    const char * name;
    for (int i = 1; (name = lua_getlocal(dbg->thread, &ar, i)) != NULL; i++) {
        if (std::string(name) == "(*temporary)") {
            lua_pop(dbg->thread, 1);
            continue;
        }
        lua_pushstring(L, name);
        lua_xmove(dbg->thread, L, 1);
        lua_settable(L, -3);
    }
    return 1;
}

int debugger_lib_catch(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    if (std::string(lua_tostring(L, 1)) == "error") dbg->breakMask |= DEBUGGER_BREAK_FUNC_ERROR;
    else if (std::string(lua_tostring(L, 1)) == "load") dbg->breakMask |= DEBUGGER_BREAK_FUNC_LOAD;
    else if (std::string(lua_tostring(L, 1)) == "run") dbg->breakMask |= DEBUGGER_BREAK_FUNC_RUN;
    return 0;
}

int debugger_lib_uncatch(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    if (std::string(lua_tostring(L, 1)) == "error") dbg->breakMask &= ~DEBUGGER_BREAK_FUNC_ERROR;
    else if (std::string(lua_tostring(L, 1)) == "load") dbg->breakMask &= ~DEBUGGER_BREAK_FUNC_LOAD;
    else if (std::string(lua_tostring(L, 1)) == "run") dbg->breakMask &= ~DEBUGGER_BREAK_FUNC_RUN;
    return 0;
}

const char * debugger_lib_keys[] = {
    "waitForBreak",
    "confirmBreak",
    "step",
    "continue",
    "stepOut",
    "getInfo",
    "setBreakpoint",
    "unsetBreakpoint",
    "listBreakpoints",
    "run",
    "status",
    "startProfiling",
    "profile",
    "getfenv",
    "unblock",
    "getReason",
    "getLocals",
    "catch",
    "uncatch",
};

lua_CFunction debugger_lib_values[] = {
    debugger_lib_waitForBreak,
    debugger_lib_confirmBreak,
    debugger_lib_step,
    debugger_lib_continue,
    debugger_lib_stepOut,
    debugger_lib_getInfo,
    debugger_lib_setBreakpoint,
    debugger_lib_unsetBreakpoint,
    debugger_lib_listBreakpoints,
    debugger_lib_run,
    debugger_lib_status,
    debugger_lib_startProfiling,
    debugger_lib_profile,
    debugger_lib_getfenv,
    debugger_lib_unblock,
    debugger_lib_getReason,
    debugger_lib_getLocals,
    debugger_lib_catch,
    debugger_lib_uncatch,
};

library_t debugger_lib = {"debugger", 19, debugger_lib_keys, debugger_lib_values, nullptr, nullptr};

library_t * debugger::createDebuggerLibrary() {
    library_t * lib = new library_t;
    memcpy(lib, &debugger_lib, sizeof(library_t));
    lib->init = std::bind(&debugger::init, this, std::placeholders::_1);
    return lib;
}

void debugger::init(Computer * comp) {
    lua_pushlightuserdata(comp->L, this);
    lua_setfield(comp->L, LUA_REGISTRYINDEX, "_debugger");
}

int debugger::_break(lua_State *L) {
    breakType = DEBUGGER_BREAK_TYPE_LINE;
    return 0;
}

int debugger::setBreakpoint(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    Computer * computer = get_comp(L);
    int id = computer->breakpoints.size() > 0 ? computer->breakpoints.rbegin()->first + 1 : 1;
    computer->breakpoints[id] = std::make_pair("@/" + fixpath(computer, lua_tostring(L, 1), false), lua_tointeger(L, 2));
    lua_pushinteger(L, id);
    return 1;
}

const char * debugger_print(lua_State *L, void* arg) {
    lua_pushstring(L, (char*)arg);
    delete[] (char*)arg;
    return "debugger_print";
}

int debugger::print(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    char * str = new char[lua_strlen(L, 1)];
    memcpy(str, lua_tostring(L, 1), lua_strlen(L, 1));
    termQueueProvider(monitor, debugger_print, str);
    return 0;
}

debugger::debugger(lua_State *L, const char * side) {
    didBreak = false;
    computer = get_comp(L);
    monitor = (Computer*)queueTask([](void*computer)->void*{return new Computer(((Computer*)computer)->id, true);}, computer);
    monitor->debugger = createDebuggerLibrary();
    computers.push_back(monitor);
    compThread = new std::thread(debuggerThread, monitor, this, std::string(side));
    setThreadName(*compThread, std::string("Computer " + std::to_string(computer->id) + " Thread (Debugger)").c_str());
    computerThreads.push_back(compThread);
    if (computer->debugger != NULL) throw std::bad_exception();
    computer->debugger = this;
}

debugger::~debugger() {
    deleteThis = true;
    breakType = DEBUGGER_BREAK_TYPE_NONSTOP;
    if (freedComputers.find(monitor) == freedComputers.end()) {
        monitor->running = 0;
        monitor->event_lock.notify_all();
        compThread->join();
        delete monitor;
    }
    running = false;
    while (thread != NULL) {
        std::lock_guard<std::mutex> guard(breakMutex);
        breakNotify.notify_all(); 
        std::this_thread::yield();
    }
    assert(thread == NULL);
    if (compThread->joinable()) compThread->join();
    computer->debugger = NULL;
}

int debugger::call(lua_State *L, const char * method) {
    std::string m(method);
    if (m == "stop" || m == "break") return _break(L);
    else if (m == "setBreakpoint") return setBreakpoint(L);
    else if (m == "print") return print(L);
    else return 0;
}

const char * debugger_keys[] = {
    "stop",
    "setBreakpoint",
    "print"
};

library_t debugger::methods = {"debugger", 3, debugger_keys, NULL, nullptr, nullptr};