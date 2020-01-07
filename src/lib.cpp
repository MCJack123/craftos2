/*
 * lib.cpp
 * CraftOS-PC 2
 * 
 * This file implements convenience functions for libraries.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include "lib.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
extern "C" {
#include <lauxlib.h>
}
#include "Computer.hpp"

char computer_key = 'C';

void load_library(Computer *comp, lua_State *L, library_t lib) {
    lua_newtable(L); // create table
    for (int i = 0; i < lib.count; i++) {
        lua_pushstring(L, lib.keys[i]); // push index
        lua_pushcfunction(L, lib.values[i]); // push value
        lua_settable(L, -3); // add index/value to table
    }
    lua_setglobal(L, lib.name); // add table as global
    if (lib.init != NULL) lib.init(comp);
}

void bad_argument(lua_State *L, const char * type, int pos) {
    luaL_where(L, 2);
    lua_pushfstring(L, "bad argument #%d (expected %s, got %s)", pos, type, lua_typename(L, lua_type(L, pos)));
    lua_concat(L, 2);
    lua_error(L);
}