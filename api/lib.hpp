/*
 * lib.hpp
 * CraftOS-PC 2
 * 
 * This file defines some functions and constants useful for plugins.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef CRAFTOS_PC_LIB_HPP
#define CRAFTOS_PC_LIB_HPP

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <string>
#include <functional>

#ifndef CRAFTOS_PC_COMPUTER_HPP
struct Computer;
#endif

/// The current version of plugin support.
#define PLUGIN_VERSION 10

/// Most OS's use UTF-8/ASCII for path storage; however, Windows is contrarian and uses UTF-16.
/// To abstract this difference in implementation, a path_t type is used to allow the correct
/// string type to be used on each platform. Since the type only changes when the platform changes,
/// this should not have a negative impact on ABI stability.
#ifdef WIN32
typedef std::wstring path_t;
#else
typedef std::string path_t;
#endif

// The library_t structure is used to hold information about an API.
struct Computer;
typedef struct library {
    const char * name;  // The name of the API
    luaL_Reg * functions; // The list of functions used in the API
    std::function<void(Computer*)> init;   // A function to call when opening the API
    std::function<void(Computer*)> deinit; // A function to call when closing the API
    ~library() {}
} library_t;

/**
 * Returns the associated Computer object pointer for a Lua state.
 * This is a bit slow, so try not to call it too much, or cache results.
 * (CraftOS-PC caches results internally.)
 * @param L The Lua state to get the computer for
 * @return The Computer object the state is running on
 */
inline Computer * get_comp(lua_State *L) {
    lua_pushinteger(L, 1);
    lua_gettable(L, LUA_REGISTRYINDEX);
    void * retval = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return (Computer*)retval;
}

#endif