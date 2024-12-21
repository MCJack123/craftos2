// Copyright (c) 2019-2024 JackMacWindows.
// SPDX-FileCopyrightText: 2019-2024 JackMacWindows
//
// SPDX-License-Identifier: MIT

#ifndef CRAFTOS_PC_LIB_HPP
#define CRAFTOS_PC_LIB_HPP

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}
#include <functional>
#include <string>

#ifndef CRAFTOS_PC_COMPUTER_HPP
struct Computer;
#endif

/// The current version of plugin support.
#if defined(_WIN32) && defined(_DEBUG)
#define PLUGIN_VERSION 100012
#else
#define PLUGIN_VERSION 12
#endif

/// Most OS's use UTF-8/ASCII for path storage; however, Windows is contrarian and uses UTF-16.
/// To abstract this difference in implementation, a path_t type is used to allow the correct
/// string type to be used on each platform. Since the type only changes when the platform changes,
/// this should not have a negative impact on ABI stability.
/// @deprecated This is retained for ABI compatibility - the next API will use std::filesystem::path instead.
#ifdef _WIN32
typedef std::wstring _path_t;
#define _to_path_t std::to_wstring
#else
typedef std::string _path_t;
#define _to_path_t std::to_string
#endif

// The library_t structure is used to hold information about an API.
struct Computer;
struct library_t {
    const char * name;  // The name of the API
    luaL_Reg * functions; // The list of functions used in the API
    std::function<void(Computer*)> init;   // A function to call when opening the API
    std::function<void(Computer*)> deinit; // A function to call when closing the API
};

#ifdef CRAFTOS_PC_HPP
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

#endif