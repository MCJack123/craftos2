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
#include <Poco/Net/TCPServerConnection.h>
#include <Poco/Net/TCPServerConnectionFactory.h>

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
        std::unique_lock<std::mutex> lock(dbg->breakMutex);
        dbg->breakNotify.notify_all();
        dbg->breakNotify.wait(lock);
        if (freedComputers.find(comp) == freedComputers.end())
            termQueueProvider(comp, debugger_break, NULL);
    }, comp);
    th.detach();
    return 0;
}

int debugger_lib_step(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    dbg->breakType = DEBUGGER_BREAK_TYPE_LINE;
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
    lua_getstack(dbg->thread, 0, &ar);
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
    return 1;
}

int debugger_lib_setBreakpoint(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    dbg->computer->breakpoints.push_back(std::make_pair("@/" + fixpath(dbg->computer, lua_tostring(L, 1), false), lua_tointeger(L, 2)));
    return 0;
}

int debugger_lib_run(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    int oldtop = lua_gettop(dbg->thread);
    lua_settop(L, 1);
    lua_xmove(L, dbg->thread, 1);
    lua_pushboolean(L, !lua_pcall(dbg->thread, 0, LUA_MULTRET, 0));
    int top = lua_gettop(dbg->thread) - oldtop;
    lua_xmove(dbg->thread, L, top);
    return top + 1;
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

const char * debugger_lib_keys[] = {
    "waitForBreak",
    "step",
    "continue",
    "stepOut",
    "getInfo",
    "setBreakpoint",
    "run",
    "status",
    "startProfiling",
    "profile",
    "getfenv",
};

lua_CFunction debugger_lib_values[] = {
    debugger_lib_waitForBreak,
    debugger_lib_step,
    debugger_lib_continue,
    debugger_lib_stepOut,
    debugger_lib_getInfo,
    debugger_lib_setBreakpoint,
    debugger_lib_run,
    debugger_lib_status,
    debugger_lib_startProfiling,
    debugger_lib_profile,
    debugger_lib_getfenv,
};

library_t debugger_lib = {"debugger", 11, debugger_lib_keys, debugger_lib_values, nullptr, nullptr};

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
    computer->breakpoints.push_back(std::make_pair(std::string(lua_tostring(L, 1)), lua_tointeger(L, 2)));
    return 0;
}

debugger::debugger(lua_State *L, const char * side) {
    computer = get_comp(L);
    monitor = (Computer*)queueTask([this](void*)->void*{return new Computer(computer->id, true);}, NULL);
    monitor->debugger = createDebuggerLibrary();
    computers.push_back(monitor);
    compThread = new std::thread(debuggerThread, monitor, this, side);
    setThreadName(*compThread, std::string("Computer " + std::to_string(computer->id) + " Thread (Debugger)").c_str());
    computerThreads.push_back(compThread);
    if (computer->debugger != NULL) throw std::bad_exception();
    computer->debugger = this;
}

debugger::~debugger() {
    deleteThis = true;
    if (freedComputers.find(monitor) == freedComputers.end()) {
        monitor->running = 0;
        monitor->event_lock.notify_all();
        compThread->join();
        delete monitor;
    }
    running = false;
    breakNotify.notify_all();
    if (compThread->joinable()) compThread->join();
    computer->debugger = NULL;
}

int debugger::call(lua_State *L, const char * method) {
    std::string m(method);
    if (m == "stop" || m == "break") return _break(L);
    else if (m == "setBreakpoint") return setBreakpoint(L);
    else return 0;
}

const char * debugger_keys[] = {
    "stop",
    "setBreakpoint"
};

library_t debugger::methods = {"debugger", 2, debugger_keys, NULL, nullptr, nullptr};