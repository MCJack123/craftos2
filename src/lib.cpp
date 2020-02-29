/*
 * lib.cpp
 * CraftOS-PC 2
 * 
 * This file implements convenience functions for libraries.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#define CRAFTOSPC_INTERNAL
#include "lib.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <sstream>
#include <Poco/Base64Decoder.h>
#include <Poco/Base64Encoder.h>
extern "C" {
#include <lauxlib.h>
}
#include "Computer.hpp"

char computer_key = 'C';
void* getCompCache_glob = NULL;
Computer * getCompCache_comp = NULL;

Computer * _get_comp(lua_State *L) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, 1);
    void * retval = lua_touserdata(L, -1);
    lua_pop(L, 1);
    getCompCache_glob = *(void**)(((ptrdiff_t)L) + sizeof(int) + sizeof(void*)*3 + 4);
    getCompCache_comp = (Computer*)retval;
    return (Computer*)retval;
}

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

std::string b64encode(std::string orig) {
    std::stringstream ss;
    Poco::Base64Encoder enc(ss);
    enc.write(orig.c_str(), orig.size());
    enc.close();
    return ss.str();
}

std::string b64decode(std::string orig) {
    std::stringstream ss;
    std::stringstream out(orig);
    Poco::Base64Decoder dec(out);
    std::copy(std::istreambuf_iterator<char>(dec), std::istreambuf_iterator<char>(), std::ostreambuf_iterator<char>(ss));
    return ss.str();
}