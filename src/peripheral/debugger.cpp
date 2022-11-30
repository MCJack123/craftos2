/*
 * peripheral/debugger.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the debugger peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#include <string>
struct Computer;
static void debuggerThread(Computer * comp, void * dbgv, std::string side);
#include "../runtime.hpp"
static int debugger_lib_getInfo(lua_State *L);
#include "debugger.hpp"
#include "debug_adapter.hpp"
#include <FileEntry.hpp>
#include "../platform.hpp"
#include "../terminal/CLITerminal.hpp"
#include "../termsupport.hpp"

#ifdef __ANDROID__
extern "C" {extern int Android_JNI_SetupThread(void);}
#endif

#ifdef STANDALONE_ROM
extern FileEntry standaloneROM;
extern FileEntry standaloneDebug;
extern std::string standaloneBIOS;
#endif

static void debuggerThread(Computer * comp, void * dbgv, std::string side) {
    debugger * dbg = (debugger*)dbgv;
#ifdef __APPLE__
    pthread_setname_np(std::string("Computer " + std::to_string(comp->id) + " Thread (Debugger)").c_str());
#endif
#ifdef __ANDROID__
    Android_JNI_SetupThread();
#endif
    // in case the allocator decides to reuse pointers
    if (freedComputers.find(comp) != freedComputers.end()) freedComputers.erase(comp);
#ifdef STANDALONE_ROM
    runComputer(comp, "debug/bios.lua", standaloneDebug["bios.lua"].data);
#else
    runComputer(comp, path_t("debug")/"bios.lua");
#endif
    freedComputers.insert(comp);
    {
        LockGuard lock(computers);
        for (auto it = computers->begin(); it != computers->end(); ++it) {
            if (*it == comp) {
                it = computers->erase(it);
                if (it == computers->end()) break;
            }
        }
    }
    delete (library_t*)comp->debugger;
    if (!dbg->deleteThis) {
        dbg->breakMask = 0;
        dbg->didBreak = false;
        dbg->running = false;
        {
            std::lock_guard<std::mutex> lock(dbg->computer->peripherals_mutex);
            dbg->computer->peripherals.erase(side);
        }
        dbg->computer->shouldDeinitDebugger = true;
        queueTask([comp](void*)->void*{delete comp; return NULL;}, NULL);
    }
}

static std::string debugger_break(lua_State *L, void* userp) {return "debugger_break";}

static int debugger_lib_waitForBreak(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    Computer * comp = get_comp(L);
    if (dbg->waitingForBreak) return 0;
    std::thread th([dbg](Computer*comp){
        dbg->didBreak = false;
        dbg->waitingForBreak = true;
        dbg->breakNotify.notify_all();
        std::unique_lock<std::mutex> lock(dbg->breakMutex);
        dbg->breakNotify.wait(lock);
        dbg->didBreak = true;
        dbg->confirmBreak = false;
        while (!dbg->confirmBreak) {
            if (freedComputers.find(comp) == freedComputers.end())
                queueEvent(comp, debugger_break, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        dbg->waitingForBreak = false;
    }, comp);
    th.detach();
    return 0;
}

static int debugger_lib_confirmBreak(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    dbg->confirmBreak = true;
    return 0;
}

static int debugger_lib_step(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    dbg->breakType = DEBUGGER_BREAK_TYPE_LINE;
    dbg->stepCount = lua_isnumber(L, 1) ? (int)lua_tointeger(L, 1) : 0;
    return 0;
}

static int debugger_lib_continue(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    dbg->breakType = DEBUGGER_BREAK_TYPE_NONSTOP;
    return 0;
}

static int debugger_lib_stepOut(lua_State *L) {
    lastCFunction = __func__;
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

static int debugger_lib_getInfo(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    if (dbg->thread == NULL) {
        lua_pushnil(L);
        return 1;
    }
    lua_Debug ar;
    if (!lua_getstack(dbg->thread, lua_isnumber(L, 1) ? (int)lua_tointeger(L, 1) : 0, &ar)) lua_pushnil(L);
    else {
        lua_getinfo(dbg->thread, "nSliu", &ar);
        lua_createtable(L, 0, 12);
        settabss(L, "source", ar.source);
        settabss(L, "short_src", ar.short_src);
        settabsi(L, "linedefined", ar.linedefined);
        settabsi(L, "lastlinedefined", ar.lastlinedefined);
        settabss(L, "what", ar.what);
        settabsi(L, "currentline", ar.currentline);
        settabss(L, "name", ar.name);
        settabss(L, "namewhat", ar.namewhat);
        settabsi(L, "instruction", ar.instruction);
        settabsi(L, "nups", ar.nups);
        settabsi(L, "nparams", ar.nparams);
        lua_pushboolean(L, ar.isvararg);
        lua_setfield(L, -2, "isvararg");
    }
    return 1;
}

static int debugger_lib_setBreakpoint(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    const int id = !dbg->computer->breakpoints.empty() ? dbg->computer->breakpoints.rbegin()->first + 1 : 1;
    dbg->computer->breakpoints[id] = std::make_pair("@/" + fixpath(dbg->computer, lua_tostring(L, 1), false, false).string(), lua_tointeger(L, 2));
    dbg->computer->hasBreakpoints = true;
    lua_pushinteger(L, id);
    return 1;
}

static int debugger_lib_setFunctionBreakpoint(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    const int id = !dbg->computer->breakpoints.empty() ? dbg->computer->breakpoints.rbegin()->first + 1 : 1;
    dbg->computer->breakpoints[id] = std::make_pair(tostring(L, 1), -1);
    dbg->computer->hasBreakpoints = true;
    lua_pushinteger(L, id);
    return 1;
}

static int debugger_lib_unsetBreakpoint(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    if (dbg->computer->breakpoints.find((int)lua_tointeger(L, 1)) != dbg->computer->breakpoints.end()) {
        dbg->computer->breakpoints.erase((int)lua_tointeger(L, 1));
        if (dbg->computer->breakpoints.empty())
            dbg->computer->hasBreakpoints = false;
        lua_pushboolean(L, true);
    } else lua_pushboolean(L, false);
    return 1;
}

static int debugger_lib_listBreakpoints(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_createtable(L, dbg->computer->breakpoints.size(), 0);
    for (const auto& bp : dbg->computer->breakpoints) {
        lua_pushinteger(L, bp.first);
        lua_createtable(L, 0, 2);
        lua_pushstring(L, "file");
        lua_pushstring(L, bp.second.first.c_str());
        lua_settable(L, -3);
        lua_pushstring(L, "line");
        lua_pushinteger(L, bp.second.second);
        lua_settable(L, -3);
        lua_settable(L, -3);
    }
    return 1;
}

static int debugger_lib_local_index(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_State *thread = (dbg == NULL) ? L : dbg->thread;
    lua_Debug ar;
    if (lua_isnumber(L, 2)) {
        if (!lua_getstack(thread, (int)lua_tointeger(L, 2), &ar)) { lua_pushnil(L); return 1; }
        const char * name;
        lua_newtable(L);
        for (int i = 1; (name = lua_getlocal(thread, &ar, i)) != NULL; i++) {
            if (std::string(name) == "(*temporary)") {
                lua_pop(thread, 1);
                continue;
            }
            lua_pushstring(L, name);
            if (L == thread) lua_pushvalue(L, -2);
            else {
                xcopy(thread, L, 1);
                lua_pop(thread, 1);
            }
            lua_settable(L, L == thread ? -4 : -3);
            if (L == thread) lua_pop(L, 1);
        }
        return 1;
    } else if (lua_isstring(L, 2)) {
        const char * name, *search = lua_tostring(L, 2);
        for (int i = 0; lua_getstack(thread, i, &ar); i++) {
            for (int j = 1; (name = lua_getlocal(thread, &ar, j)) != NULL; j++) {
                if (strcmp(search, name) == 0) {
                    if (L != thread) {
                        xcopy(thread, L, 1);
                        lua_pop(thread, 1);
                    }
                    return 1;
                }
                else lua_pop(thread, 1);
            }
        }
        lua_pushnil(L);
        return 1;
    } else {
        lua_pushnil(L);
        return 1;
    }
}

static int debugger_lib_local_newindex(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_State *thread = (dbg == NULL) ? L : dbg->thread;
    lua_Debug ar;
    const char * name, *search = luaL_checkstring(L, 2);
    for (int i = 0; lua_getstack(thread, i, &ar); i++) {
        for (int j = 1; (name = lua_getlocal(thread, &ar, j)) != NULL; j++) {
            if (strcmp(search, name) == 0) {
                if (L != thread) xcopy(L, thread, 1);
                lua_setlocal(thread, &ar, j);
                return 0;
            }
            else lua_pop(thread, 1);
        }
    }
    return 0;
}

static int debugger_lib_local_tostring(lua_State *L) {
    lastCFunction = __func__;
    lua_pushstring(L, "locals table");
    return 1;
}

static int debugger_lib_upvalue_index(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_State *thread = (dbg == NULL) ? L : dbg->thread;
    if (lua_isnumber(L, 2)) {
        const char * name;
        lua_newtable(L);
        for (int i = 1; (name = lua_getupvalue(thread, (int)lua_tointeger(L, 2), i)) != NULL; i++) {
            lua_pushstring(L, name);
            if (L == thread) lua_pushvalue(L, -2);
            else {
                xcopy(thread, L, 1);
                lua_pop(thread, 1);
            }
            lua_settable(L, L == thread ? -4 : -3);
            if (L == thread) lua_pop(L, 1);
        }
        return 1;
    } else if (lua_isstring(L, 2)) {
        const char * name, *search = lua_tostring(L, 2);
        lua_Debug ar;
        for (int i = 0; lua_getstack(thread, i, &ar); i++) {
            for (int j = 1; (name = lua_getupvalue(thread, i, j)) != NULL; j++) {
                if (strcmp(search, name) == 0) {
                    if (L != thread) {
                        xcopy(thread, L, 1);
                        lua_pop(thread, 1);
                    }
                    return 1;
                }
                else lua_pop(thread, 1);
            }
        }
        lua_pushnil(L);
        return 1;
    } else {
        lua_pushnil(L);
        return 1;
    }
}

static int debugger_lib_upvalue_newindex(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_State *thread = (dbg == NULL) ? L : dbg->thread;
    lua_Debug ar;
    const char * name, *search = luaL_checkstring(L, 2);
    for (int i = 0; lua_getstack(thread, i, &ar); i++) {
        for (int j = 1; (name = lua_getupvalue(thread, i, j)) != NULL; j++) {
            if (strcmp(search, name) == 0) {
                if (L != thread) xcopy(L, thread, 1);
                lua_setupvalue(thread, i, j);
                return 0;
            }
            else lua_pop(thread, 1);
        }
    }
    return 0;
}

static int debugger_lib_upvalue_tostring(lua_State *L) {
    lastCFunction = __func__;
    lua_pushstring(L, "upvalues table");
    return 1;
}

static int _echo(lua_State *L) {return lua_gettop(L);}

static int debugger_lib_run(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_Debug ar;
    memset(&ar, 0, sizeof(lua_Debug));
    lua_settop(L, 1);
    const int top = lua_gettop(dbg->thread); // ...
    luaL_loadstring(dbg->thread, lua_tostring(L, 1)); // ..., func
    luaL_loadstring(dbg->thread, "local locals, upvalues = ... local t = setmetatable({_echo = function(...) return ... end, locals = locals, upvalues = upvalues, getfenv = getfenv, _ENV = getfenv(2)}, {__index = function(_, idx) if locals[idx] ~= nil then return locals[idx] elseif upvalues[idx] ~= nil then return upvalues[idx] else return getfenv(2)[idx] end end}) return t"); // ..., func, getenv
    lua_newuserdata(dbg->thread, 0); // ..., func, getenv, locals
    lua_createtable(dbg->thread, 0, 3); // ..., func, getenv, locals, mt
    lua_pushcfunction(dbg->thread, debugger_lib_local_index); // ..., func, getenv, locals, mt, __index
    lua_setfield(dbg->thread, -2, "__index"); // ..., func, getenv, locals, mt
    lua_pushcfunction(dbg->thread, debugger_lib_local_newindex); // ..., func, getenv, locals, mt, __newindex
    lua_setfield(dbg->thread, -2, "__newindex"); // ..., func, getenv, locals, mt
    lua_pushcfunction(dbg->thread, debugger_lib_local_tostring); // ..., func, getenv, locals, mt, __tostring
    lua_setfield(dbg->thread, -2, "__tostring"); // ..., func, getenv, locals, mt
    lua_setmetatable(dbg->thread, -2); // ..., func, getenv, locals (w/mt)
    lua_newuserdata(dbg->thread, 0); // ..., func, getenv, locals (w/mt), upvalues
    lua_createtable(dbg->thread, 0, 3); // ..., func, getenv, locals (w/mt), upvalues, mt
    lua_pushcfunction(dbg->thread, debugger_lib_upvalue_index); // ..., func, getenv, locals (w/mt), upvalues, mt, __index
    lua_setfield(dbg->thread, -2, "__index"); // ..., func, getenv, locals (w/mt), upvalues, mt
    lua_pushcfunction(dbg->thread, debugger_lib_upvalue_newindex); // ..., func, locals (w/mt), getenv, upvalues, mt, __newindex
    lua_setfield(dbg->thread, -2, "__newindex"); // ..., func, getenv, locals (w/mt), upvalues, mt
    lua_pushcfunction(dbg->thread, debugger_lib_upvalue_tostring); // ..., func, getenv, upvalues (w/mt), upvalues, mt, __tostring
    lua_setfield(dbg->thread, -2, "__tostring"); // ..., func, getenv, locals (w/mt), upvalues, mt
    lua_setmetatable(dbg->thread, -2); // ..., func, getenv, locals (w/mt), upvalues (w/mt)
    const int status = lua_pcall(dbg->thread, 2, 1, 0); // ..., func, env
    if (status != 0) {
        fprintf(stderr, "Error while loading debug environment: %s\n", lua_tostring(dbg->thread, -1));
        lua_pop(dbg->thread, 1);
        lua_createtable(dbg->thread, 0, 1);
        lua_pushcfunction(dbg->thread, _echo);
        lua_setfield(dbg->thread, -2, "_echo");
    }
    lua_setfenv(dbg->thread, -2); // ..., func (w/env)
    lua_pushboolean(L, !lua_pcall(dbg->thread, 0, LUA_MULTRET, 0)); // ..., results...
    const int top2 = lua_gettop(dbg->thread) - top; // #{..., results...} - #{...} = #{results...}
    xcopy(dbg->thread, L, top2); // ...
    return top2 + 1;
}

static int debugger_lib_status(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_pushboolean(L, dbg->thread != NULL);
    if (dbg->breakType == DEBUGGER_BREAK_TYPE_NONSTOP) lua_pushstring(L, "nonstop");
    else if (dbg->breakType == DEBUGGER_BREAK_TYPE_LINE) lua_pushstring(L, "line");
    else if (dbg->breakType == DEBUGGER_BREAK_TYPE_RETURN) lua_pushstring(L, "return");
    else lua_pushnil(L);
    return 2;
}

static int debugger_lib_startProfiling(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    if (lua_toboolean(L, 1)) dbg->profile.clear();
    dbg->isProfiling = lua_toboolean(L, 1);
    return 0;
}

static int debugger_lib_profile(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_createtable(L, 0, dbg->profile.size());
    for (const auto& e : dbg->profile) {
        if (!e.second.empty()) {
            lua_pushstring(L, e.first.c_str());
            lua_createtable(L, 0, e.second.size());
            for (const auto& ee : e.second) {
                lua_pushstring(L, ee.first.c_str());
                lua_createtable(L, 0, 2);
                lua_pushinteger(L, ee.second.count);
                lua_setfield(L, -2, "count");
                lua_pushnumber(L, std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(ee.second.time).count());
                lua_setfield(L, -2, "time");
                lua_settable(L, -3);
            }
            lua_settable(L, -3);
        }
    }
    return 1;
}

static int debugger_lib_getfenv(lua_State *L) {
    lastCFunction = __func__;
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

static int debugger_lib_unblock(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    queueEvent(dbg->computer, debugger_break, NULL);
    return 0;
}

static int debugger_lib_getReason(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_pushstring(L, dbg->breakReason.c_str());
    return 1;
}

static int debugger_lib_getLocals(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_State *thread = (dbg == NULL) ? L : dbg->thread;
    lua_Debug ar;
    if (!lua_getstack(thread, luaL_optinteger(L, 1, 1), &ar)) {
        lua_newtable(L);
        return 1;
    }
    lua_newtable(L);
    const char * name;
    for (int i = 1; (name = lua_getlocal(thread, &ar, i)) != NULL; i++) {
        if (std::string(name) == "(*temporary)") {
            lua_pop(thread, 1);
            continue;
        }
        lua_pushstring(L, name);
        if (thread != L) xcopy(thread, L, 1);
        else { lua_pushvalue(L, -2); lua_remove(L, -3); }
        lua_settable(L, -3);
    }
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, debugger_lib_local_index);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    return 1;
}

static int debugger_lib_getUpvalues(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_State *thread = (dbg == NULL) ? L : dbg->thread;
    lua_Debug ar;
    if (!lua_getstack(thread, luaL_optinteger(L, 1, 1), &ar)) {
        lua_newtable(L);
        return 1;
    }
    lua_newtable(L);
    const char * name;
    for (int i = 1; (name = lua_getupvalue(thread, luaL_optinteger(L, 1, 1), i)) != NULL; i++) {
        lua_pushstring(L, name);
        if (thread != L) xcopy(thread, L, 1);
        else { lua_pushvalue(L, -2); lua_remove(L, -3); }
        lua_settable(L, -3);
    }
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, debugger_lib_upvalue_index);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    return 1;
}

static int debugger_lib_setLocal(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_State *thread = (dbg == NULL) ? L : dbg->thread;
    lua_Debug ar;
    if (!lua_getstack(thread, luaL_optinteger(L, 1, 1), &ar)) return 0;
    std::string key = tostring(L, 2);
    const char * name;
    for (int i = 1; (name = lua_getlocal(thread, &ar, i)) != NULL; i++) {
        if (name == key) {
            lua_pushvalue(L, 3);
            if (L != thread) xcopy(L, thread, 1);
            lua_setlocal(thread, &ar, i);
            lua_pop(thread, 1);
            return 0;
        }
        lua_pop(thread, 1);
    }
    return 0;
}

static int debugger_lib_setUpvalue(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_State *thread = (dbg == NULL) ? L : dbg->thread;
    lua_Debug ar;
    if (!lua_getstack(thread, luaL_optinteger(L, 1, 1), &ar)) return 0;
    std::string key = tostring(L, 2);
    const char * name;
    for (int i = 1; (name = lua_getupvalue(thread, luaL_optinteger(L, 1, 1), i)) != NULL; i++) {
        if (name == key) {
            lua_pushvalue(L, 3);
            if (L != thread) xcopy(L, thread, 1);
            lua_setupvalue(thread, luaL_optinteger(L, 1, 1), i);
            lua_pop(thread, 1);
            return 0;
        }
        lua_pop(thread, 1);
    }
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, debugger_lib_upvalue_index);
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);
    return 1;
}

static int debugger_lib_catch(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    if (std::string(lua_tostring(L, 1)) == "error") dbg->breakMask |= DEBUGGER_BREAK_FUNC_ERROR;
    else if (std::string(lua_tostring(L, 1)) == "load") dbg->breakMask |= DEBUGGER_BREAK_FUNC_LOAD;
    else if (std::string(lua_tostring(L, 1)) == "run") dbg->breakMask |= DEBUGGER_BREAK_FUNC_RUN;
    else if (std::string(lua_tostring(L, 1)) == "resume") dbg->breakMask |= DEBUGGER_BREAK_FUNC_RESUME;
    else if (std::string(lua_tostring(L, 1)) == "yield") dbg->breakMask |= DEBUGGER_BREAK_FUNC_YIELD;
    return 0;
}

static int debugger_lib_uncatch(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    if (std::string(lua_tostring(L, 1)) == "error") dbg->breakMask &= ~DEBUGGER_BREAK_FUNC_ERROR;
    else if (std::string(lua_tostring(L, 1)) == "load") dbg->breakMask &= ~DEBUGGER_BREAK_FUNC_LOAD;
    else if (std::string(lua_tostring(L, 1)) == "run") dbg->breakMask &= ~DEBUGGER_BREAK_FUNC_RUN;
    else if (std::string(lua_tostring(L, 1)) == "resume") dbg->breakMask &= ~DEBUGGER_BREAK_FUNC_RESUME;
    else if (std::string(lua_tostring(L, 1)) == "yield") dbg->breakMask &= ~DEBUGGER_BREAK_FUNC_YIELD;
    return 0;
}

static int debugger_lib_useDAP(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    lua_pushboolean(L, dynamic_cast<debug_adapter*>(dbg) != NULL);
    return 1;
}

static int debugger_lib_sendDAPData(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debug_adapter * dbg = dynamic_cast<debug_adapter*>((debugger*)lua_touserdata(L, -1));
    if (dbg == NULL) return 0;
    dbg->sendData(checkstring(L, 1));
    return 0;
}

static int debugger_startupCode(lua_State *L) {
    lastCFunction = __func__;
    std::string * str = (std::string*)lua_touserdata(L, 1);
    lua_pushlstring(L, str->c_str(), str->size());
    lua_setglobal(L, "_CCPC_STARTUP_SCRIPT");
    delete str;
    lua_pushboolean(L, true);
    return 1;
}

static int debugger_lib_setStartupCode(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    dbg->computer->startupCallbacks.push_back(std::make_pair(debugger_startupCode, new std::string(checkstring(L, 1))));
    return 0;
}

static int debugger_lib_getPath(lua_State *L) {
    lastCFunction = __func__;
    std::string path = fixpath(get_comp(L), checkstring(L, 1), true).string();
    pushstring(L, path);
    return 1;
}

static int debugger_lib_getInternalPath(lua_State *L) {
    lastCFunction = __func__;
    lua_getfield(L, LUA_REGISTRYINDEX, "_debugger");
    debugger * dbg = (debugger*)lua_touserdata(L, -1);
    path_t::string_type path = path_t(checkstring(L, 1)).native();
    std::tuple<std::list<std::string>, path_t, bool> maxPath = std::make_tuple(std::list<std::string>(), "", false);
    {
        path_t::string_type p = path_t(dbg->computer->dataDir).native();
        if (path.substr(0, p.size()) == p && p.size() > std::get<1>(maxPath).native().size()) maxPath = std::make_tuple(std::list<std::string>(), dbg->computer->dataDir, false);
    }
    for (const auto& mount : dbg->computer->mounts) {
        path_t::string_type p = path_t(std::get<1>(mount)).native();
        if (path.substr(0, p.size()) == p && p.size() > std::get<1>(maxPath).native().size()) maxPath = mount;
    }
    if (std::get<1>(maxPath).empty()) lua_pushnil(L);
    else {
        bool begin = true;
        for (const std::string& p : std::get<0>(maxPath)) {
            if (begin) {
                begin = false;
                pushstring(L, p);
            } else {
                lua_pushliteral(L, "/");
                pushstring(L, p);
                lua_concat(L, 3);
            }
        }
        std::string rest = path_t(path.substr(std::get<1>(maxPath).native().size())).string();
        for (int i = 0; i < rest.size(); i++) if (rest[i] == '\\') rest[i] = '/';
        pushstring(L, rest);
        if (!begin) lua_concat(L, 2);
    }
    return 1;
}

static luaL_Reg debugger_lib_reg[] = {
    {"waitForBreak", debugger_lib_waitForBreak},
    {"confirmBreak", debugger_lib_confirmBreak},
    {"step", debugger_lib_step},
    {"continue", debugger_lib_continue},
    {"stepOut", debugger_lib_stepOut},
    {"getInfo", debugger_lib_getInfo},
    {"setBreakpoint", debugger_lib_setBreakpoint},
    {"setFunctionBreakpoint", debugger_lib_setFunctionBreakpoint},
    {"unsetBreakpoint", debugger_lib_unsetBreakpoint},
    {"listBreakpoints", debugger_lib_listBreakpoints},
    {"run", debugger_lib_run},
    {"status", debugger_lib_status},
    {"startProfiling", debugger_lib_startProfiling},
    {"profile", debugger_lib_profile},
    {"getfenv", debugger_lib_getfenv},
    {"unblock", debugger_lib_unblock},
    {"getReason", debugger_lib_getReason},
    {"getLocals", debugger_lib_getLocals},
    {"getUpvalues", debugger_lib_getUpvalues},
    {"catch", debugger_lib_catch},
    {"uncatch", debugger_lib_uncatch},
    {"useDAP", debugger_lib_useDAP},
    {"sendDAPData", debugger_lib_sendDAPData},
    {"setStartupCode", debugger_lib_setStartupCode},
    {"getPath", debugger_lib_getPath},
    {"getInternalPath", debugger_lib_getInternalPath},
    {NULL, NULL}
};

static library_t debugger_lib = {"debugger", debugger_lib_reg, nullptr, nullptr};

library_t * debugger::createDebuggerLibrary() {
    library_t * lib = new library_t;
    memcpy(lib, &debugger_lib, sizeof(library_t));
    lib->init = [this](Computer * comp){init(comp);};
    return lib;
}

void debugger::init(Computer * comp) {
    lua_pushlightuserdata(comp->L, this);
    lua_setfield(comp->L, LUA_REGISTRYINDEX, "_debugger");
}

int debugger::_break(lua_State *L) {
    lastCFunction = __func__;
    breakType = DEBUGGER_BREAK_TYPE_LINE;
    return 0;
}

int debugger::setBreakpoint(lua_State *L) {
    lastCFunction = __func__;
    Computer * computer = get_comp(L);
    const int id = !computer->breakpoints.empty() ? computer->breakpoints.rbegin()->first + 1 : 1;
    computer->breakpoints[id] = std::make_pair("@/" + fixpath(computer, luaL_checkstring(L, 1), false, false).string(), luaL_checkinteger(L, 2));
    computer->hasBreakpoints = true;
    lua_pushinteger(L, id);
    return 1;
}

static std::string debugger_print(lua_State *L, void* arg) {
    std::string * str = (std::string*)arg;
    lua_pushlstring(L, str->c_str(), str->size());
    delete str;
    return "debugger_print";
}

static int lua_converttostring (lua_State *L) {
  if (lua_icontext(L)) return 1;
  luaL_checkany(L, 1);
  if (luaL_getmetafield(L, 1, "__tostring")) {
    lua_pushvalue(L, 1);
    lua_icall(L, 1, 1, 1);  /* call metamethod */
    return 1;
  }
  switch (lua_type(L, 1)) {
    case LUA_TNUMBER:
      lua_pushstring(L, lua_tostring(L, 1));
      break;
    case LUA_TSTRING:
      lua_pushvalue(L, 1);
      break;
    case LUA_TBOOLEAN:
      lua_pushstring(L, (lua_toboolean(L, 1) ? "true" : "false"));
      break;
    case LUA_TNIL:
      lua_pushliteral(L, "nil");
      break;
    default:
      lua_pushfstring(L, "%s: %p", luaL_typename(L, 1), lua_topointer(L, 1));
      break;
  }
  return 1;
}

int debugger::print(lua_State *L) {
    lastCFunction = __func__;
    lua_settop(L, 1);
    if (!lua_isstring(L, -1)) {
        lua_pushcfunction(L, lua_converttostring);
        lua_pushvalue(L, -2);
        lua_call(L, 1, 1);
        luaL_checkstring(L, -1);
    }
    if (monitor != NULL) {
        std::string * str = new std::string(tostring(L, -1));
        queueEvent(monitor, debugger_print, str);
    }
    return 0;
}

struct debugger_param {
    int id;
    std::string err;
};

debugger::debugger(lua_State *L, const char * side) {
    if (SDL_GetCurrentVideoDriver() != NULL && (std::string(SDL_GetCurrentVideoDriver()) == "KMSDRM" || std::string(SDL_GetCurrentVideoDriver()) == "KMSDRM_LEGACY"))
        throw std::runtime_error("Debuggers are not available when using the Linux framebuffer");
    didBreak = false;
    running = true;
    computer = get_comp(L);
    if (computer->debugger != NULL) throw std::runtime_error("A debugger is already attached to this computer");
    debugger_param * p = new debugger_param;
    p->id = computer->id;
    monitor = (Computer*)queueTask([](void*computer)->void*{try {return new Computer(((debugger_param*)computer)->id, true);} catch (std::exception &e) {((debugger_param*)computer)->err = e.what(); return NULL;}}, p);
    if (monitor == NULL) {
        const std::string exc = "Could not start debugger session: " + std::string(p->err);
        delete p;
        throw std::runtime_error(exc.c_str());
    }
    delete p;
    monitor->debugger = createDebuggerLibrary();
    {
        LockGuard lock(computers);
        computers->push_back(monitor);
    }
    compThread = new std::thread(debuggerThread, monitor, this, std::string(side));
    setThreadName(*compThread, "Computer " + std::to_string(computer->id) + " Thread (Debugger)");
    computerThreads.push_back(compThread);
    computer->shouldDeinitDebugger = false;
    computer->debugger = this;
    lua_sethook(computer->L, termHook, LUA_MASKLINE | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 0);
    lua_sethook(computer->coro, termHook, LUA_MASKLINE | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 0);
    lua_sethook(L, termHook, LUA_MASKLINE | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 0);
    lua_getfield(L, LUA_REGISTRYINDEX, "_coroutine_stack");
    for (size_t i = 1; i <= lua_objlen(L, -1); i++) {
        lua_rawgeti(L, -1, (int)i);
        if (lua_isthread(L, -1)) lua_sethook(lua_tothread(L, -1), termHook, LUA_MASKLINE | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 0);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

void debugger::reinitialize(lua_State *L) {
    lua_sethook(computer->coro, termHook, LUA_MASKLINE | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 0);
    lua_sethook(L, termHook, LUA_MASKLINE | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 0);
}

debugger::~debugger() {
    deleteThis = true;
    breakType = DEBUGGER_BREAK_TYPE_NONSTOP;
    didBreak = false;
    if (monitor != NULL && freedComputers.find(monitor) == freedComputers.end()) {
        monitor->running = 0;
        monitor->event_lock.notify_all();
        compThread->join();
        delete monitor;
    }
    running = false;
    while (thread != NULL || waitingForBreak) {
        //std::lock_guard<std::mutex> guard(breakMutex);
        breakNotify.notify_all(); 
        confirmBreak = true;
        std::this_thread::yield();
    }
    if (compThread != NULL && compThread->joinable()) compThread->join();
    computer->shouldDeleteDebugger = false;
    computer->shouldDeinitDebugger = true;
}

int debugger::call(lua_State *L, const char * method) {
    const std::string m(method);
    if (m == "stop" || m == "break") return _break(L);
    else if (m == "setBreakpoint") return setBreakpoint(L);
    else if (m == "print") return print(L);
    else if (m == "deinit") return _deinit(L);
    else return luaL_error(L, "No such method");
}

int debugger::_deinit(lua_State *L) {
    if (!computer->hasBreakpoints) {
        lua_sethook(computer->L, termHook, LUA_MASKCOUNT | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
        lua_sethook(computer->coro, termHook, LUA_MASKCOUNT | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
        if (L) lua_sethook(L, termHook, LUA_MASKCOUNT | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
    }
    return 0;
}

static luaL_Reg debugger_reg[] = {
    {"stop", NULL},
    {"setBreakpoint", NULL},
    {"print", NULL},
    {NULL, NULL}
};

library_t debugger::methods = {"debugger", debugger_reg, nullptr, nullptr};