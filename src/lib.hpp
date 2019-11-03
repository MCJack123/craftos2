/*
 * lib.hpp
 * CraftOS-PC 2
 * 
 * This file defines the library structure and some convenience functions for
 * libraries (APIs).
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#ifndef LIB_HPP
#define LIB_HPP
#include <functional>
extern "C" {
#include <lua.h>
#include <lualib.h>
}

#define CRAFTOSPC_VERSION "v2.1.2"

class Computer;
typedef struct library {
    const char * name;
    int count;
    const char ** keys;
    lua_CFunction * values;
    std::function<void(Computer*)> init;
    std::function<void(Computer*)> deinit;
    ~library() {}
} library_t;

#include "Computer.hpp"

extern void load_library(Computer *comp, lua_State *L, library_t lib);
extern void bad_argument(lua_State *L, const char * type, int pos);
extern Computer * get_comp(lua_State *L);
#endif