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

void* debuggerThread(void* data) {
    Computer * comp = (Computer*)data;
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
    queueTask([](void* arg)->void* {delete (Computer*)arg; return NULL; }, comp);
    return NULL;
}

const char * debugger_break(lua_State *L, void* userp) {return "debugger_break";}

int debugger_lib_waitForBreak(lua_State *L) {
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    Computer * comp = get_comp(L);
    std::thread th([dbg](Computer*comp){
        std::mutex mtx;
        std::unique_lock<std::mutex> lock(mtx);
        dbg->runNotify.notify_all();
        dbg->runNotify.notify_all();
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
    dbg->breakType = DEBUGGER_BREAK_TYPE_RETURN;
    return 0;
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
    if (dbg->last_info == NULL) {
        lua_pushnil(L);
        return 1;
    }
    lua_Debug ar = *dbg->last_info;
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

const char * debugger_lib_keys[] = {
    "waitForBreak",
    "step",
    "continue",
    "stepOut",
    "getInfo",
    "setBreakpoint",
};

lua_CFunction debugger_lib_values[] = {
    debugger_lib_waitForBreak,
    debugger_lib_step,
    debugger_lib_continue,
    debugger_lib_stepOut,
    debugger_lib_getInfo,
    debugger_lib_setBreakpoint,
};

library_t debugger_lib = {"debugger", 6, debugger_lib_keys, debugger_lib_values, NULL, NULL};

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

void debugger::run() {
    std::mutex mtx;
    std::unique_lock<std::mutex> lock(mtx);
    while (running) {
        breakNotify.wait(lock);
        if (!running) break;
        printf("Did break\n");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        breakType = DEBUGGER_BREAK_TYPE_NONSTOP;
        runNotify.notify_all();
    }
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
    compThread = std::thread(debuggerThread, monitor);
    setThreadName(compThread, std::string("Computer " + std::to_string(computer->id) + " Thread (Debugger)").c_str());
    computerThreads.push_back(&compThread);
    if (computer->debugger != NULL) throw std::bad_exception();
    computer->debugger = this;
}

debugger::~debugger() {
    if (freedComputers.find(monitor) == freedComputers.end()) {
        monitor->running = 0;
        monitor->event_lock.notify_all();
        compThread.join();
        delete monitor;
    }
    if (compThread.joinable()) compThread.join();
    running = false;
    breakNotify.notify_all();
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

library_t debugger::methods = {"debugger", 2, debugger_keys, NULL, NULL, NULL};