/*
 * redstone.hpp
 * CraftOS-PC 2
 * 
 * This file defines and implements the methods for the redstone/rs API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2020 JackMacWindows.
 */

#ifndef REDSTONE_HPP
#define REDSTONE_HPP
#include "lib.hpp"
static int rs_getSides(lua_State *L) {
    lua_newtable(L); 
    lua_pushinteger(L, 1);
    lua_pushstring(L, "bottom");
    lua_settable(L, -3);
    lua_pushinteger(L, 2);
    lua_pushstring(L, "top");
    lua_settable(L, -3);
    lua_pushinteger(L, 3);
    lua_pushstring(L, "back");
    lua_settable(L, -3);
    lua_pushinteger(L, 4);
    lua_pushstring(L, "front");
    lua_settable(L, -3);
    lua_pushinteger(L, 5);
    lua_pushstring(L, "right");
    lua_settable(L, -3);
    lua_pushinteger(L, 6);
    lua_pushstring(L, "left");
    lua_settable(L, -3);
    return 1;
}
static int rs_false(lua_State *L) {lua_pushboolean(L, 0); return 1;}
static int rs_0(lua_State *L) {lua_pushinteger(L, 0); return 1;}
static int rs_none(lua_State *L) {return 0;}
static const char * rs_keys[11] = {
    "getSides",
    "getInput",
    "setOutput",
    "getOutput",
    "getAnalogInput",
    "setAnalogOutput",
    "getAnalogOutput",
    "getBundledInput",
    "getBundledOutput",
    "setBundledOutput",
    "testBundledInput"
};
static lua_CFunction rs_values[11] = {
    rs_getSides,
    rs_false,
    rs_none,
    rs_false,
    rs_0,
    rs_none,
    rs_0,
    rs_0,
    rs_0,
    rs_none,
    rs_false
};
static library_t rs_lib = {"redstone", 11, rs_keys, rs_values, NULL, NULL};
#endif