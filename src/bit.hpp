/*
 * bit.hpp
 * CraftOS-PC 2
 * 
 * This file defines and implements the methods for the bit API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef BIT_HPP
#define BIT_HPP
#include "lib.hpp"

static int bit_band(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    unsigned int a = lua_tointeger(L, 1);
    unsigned int b = lua_tointeger(L, 2);
    lua_pushinteger(L, a & b);
    return 1;
}

static int bit_bor(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    unsigned int a = lua_tointeger(L, 1);
    unsigned int b = lua_tointeger(L, 2);
    lua_pushinteger(L, a | b);
    return 1;
}

static int bit_bnot(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    unsigned int a = lua_tointeger(L, 1);
    lua_pushinteger(L, ~a & 0xFFFFFFFF);
    return 1;
}

static int bit_bxor(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    unsigned int a = lua_tointeger(L, 1);
    unsigned int b = lua_tointeger(L, 2);
    lua_pushinteger(L, a ^ b);
    return 1;
}

static int bit_blshift(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    unsigned int a = lua_tointeger(L, 1);
    unsigned int b = lua_tointeger(L, 2);
    lua_pushinteger(L, a << b);
    return 1;
}

static int bit_brshift(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    unsigned int a = lua_tointeger(L, 1);
    unsigned int b = lua_tointeger(L, 2) % 32;
    lua_pushinteger(L, a >> b | ((((a & 0x80000000) << b) - 1) << (32 - b)));
    return 1;
}

static int bit_blogic_rshift(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    unsigned int a = lua_tointeger(L, 1);
    unsigned int b = lua_tointeger(L, 2);
    lua_pushinteger(L, a >> b);
    return 1;
}

static int bit32_btest(lua_State *L) {
    unsigned int num = luaL_checkinteger(L, 1);
    for (int i = 2; i <= lua_gettop(L); i++) num &= luaL_checkinteger(L, i);
    lua_pushboolean(L, num);
    return 1;
}

static int bit32_extract(lua_State *L) {
    unsigned int n = luaL_checkinteger(L, 1);
    int field = luaL_checkinteger(L, 2);
    if (field < 0 || field > 31) luaL_error(L, "bad argument #2 (field out of range)");
    int width = luaL_optinteger(L, 3, 1);
    if (width < 1 || field + width - 1 > 31) luaL_error(L, "bad argument #3 (width out of range)");
    unsigned int mask = 0;
    for (int i = field; i < field + width; i++) mask |= 2^i;
    lua_pushinteger(L, n & mask);
    return 1;
}

static int bit32_replace(lua_State *L) {
    unsigned int n = luaL_checkinteger(L, 1);
    unsigned int v = luaL_checkinteger(L, 2);
    int field = luaL_checkinteger(L, 3);
    if (field < 0 || field > 31) luaL_error(L, "bad argument #3 (field out of range)");
    int width = luaL_optinteger(L, 4, 1);
    if (width < 1 || field + width - 1 > 31) luaL_error(L, "bad argument #4 (width out of range)");
    unsigned int mask = 0;
    for (int i = field; i < field + width; i++) mask |= 2^i;
    lua_pushinteger(L, (n & ~mask) | (v & mask));
    return 1;
}

static unsigned int rotate_impl(bool right, unsigned int n, unsigned int disp) {
    if (right) return ((n & ~((unsigned int)pow(2, disp) - 1)) >> disp) | ((n & ((unsigned int)pow(2, disp) - 1)) << (32 - disp));
    else return ((n & ~(((unsigned int)pow(2, disp) - 1) << (32 - disp))) << disp) | (((n & ((unsigned int)pow(2, disp) - 1)) << (32 - disp)) >> (32 - disp));
}

static int bit32_rrotate(lua_State *L) {
    unsigned int n = luaL_checkinteger(L, 1);
    int disp = luaL_checkinteger(L, 2) % 32;
    lua_pushinteger(L, rotate_impl(disp > 0, n, abs(disp)));
    return 1;
}

static int bit32_lrotate(lua_State *L) {
    unsigned int n = luaL_checkinteger(L, 1);
    int disp = luaL_checkinteger(L, 2) % 32;
    lua_pushinteger(L, rotate_impl(disp < 0, n, abs(disp)));
    return 1;
}

static const char * bit_keys[12] = {
    "band",
    "bor",
    "bnot",
    "bxor",
    "lshift",
    "arshift",
    "rshift",
    "btest",
    "extract",
    "replace",
    "rrotate",
    "lrotate"
};

static lua_CFunction bit_values[12] = {
    bit_band,
    bit_bor,
    bit_bnot,
    bit_bxor,
    bit_blshift,
    bit_brshift,
    bit_blogic_rshift,
    bit32_btest,
    bit32_extract,
    bit32_replace,
    bit32_rrotate,
    bit32_lrotate
};

static library_t bit_lib = {"bit32", 12, bit_keys, bit_values, nullptr, nullptr};
#endif