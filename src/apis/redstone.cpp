/*
 * apis/redstone.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the redstone/rs API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include "../util.hpp"
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
static luaL_reg rs_reg[] = {
    {"getSides", rs_getSides},
    {"getInput", rs_false},
    {"setOutput", rs_none},
    {"getOutput", rs_false},
    {"getAnalogInput", rs_0},
    {"setAnalogOutput", rs_none},
    {"getAnalogOutput", rs_0},
    {"getBundledInput", rs_0},
    {"getBundledOutput", rs_0},
    {"setBundledOutput", rs_none},
    {"testBundledInput", rs_false},
    {NULL, NULL}
};
library_t rs_lib = {"redstone", rs_reg, NULL, NULL};