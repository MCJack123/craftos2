/*
 * apis/peripheral.cpp
 * CraftOS-PC 2
 * 
 * This file defines the functions in the peripheral API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include <Computer.hpp>
#include <peripheral.hpp>
#include "../util.hpp"

static int peripheral_isPresent(lua_State *L) {
    luaL_checkstring(L, 1);
    Computer * computer = get_comp(L);
    std::lock_guard<std::mutex> lock(computer->peripherals_mutex);
    lua_pushboolean(L, computer->peripherals.find(std::string(lua_tostring(L, -1))) != computer->peripherals.end());
    return 1;
}

static int peripheral_getType(lua_State *L) {
    Computer * computer = get_comp(L);
    const std::string side(luaL_checkstring(L, 1));
    std::lock_guard<std::mutex> lock(computer->peripherals_mutex);
    if (computer->peripherals.find(side) != computer->peripherals.end())
        lua_pushstring(L, computer->peripherals[side]->getMethods().name);
    else return 0;
    return 1;
}

static int peripheral_getMethods(lua_State *L) {
    Computer * computer = get_comp(L);
    const std::string side(luaL_checkstring(L, 1));
    std::lock_guard<std::mutex> lock(computer->peripherals_mutex);
    if (computer->peripherals.find(side) == computer->peripherals.end()) return 0;
    const library_t methods = computer->peripherals[side]->getMethods();
    lua_newtable(L);
    for (int i = 0; methods.functions[i].name; i++) {
        lua_pushinteger(L, i+1);
        lua_pushstring(L, methods.functions[i].name);
        lua_settable(L, -3);
    }
    return 1;
}

static int peripheral_call(lua_State *L) {
    Computer * computer = get_comp(L);
    const std::string side(luaL_checkstring(L, 1));
    const std::string func(luaL_checkstring(L, 2));
    peripheral * p;
    {
        std::lock_guard<std::mutex> lock(computer->peripherals_mutex);
        if (computer->peripherals.find(side) == computer->peripherals.end()) return 0;
        lua_remove(L, 1);
        lua_remove(L, 1);
        p = computer->peripherals[side];
    }
    return p->call(L, func.c_str());
}

void peripheral_update(Computer *comp) {
    std::lock_guard<std::mutex> lock(comp->peripherals_mutex);
    for (auto p : comp->peripherals) p.second->update();
}

static luaL_Reg peripheral_reg[] = {
    {"isPresent", peripheral_isPresent},
    {"getType", peripheral_getType},
    {"getMethods", peripheral_getMethods},
    {"call", peripheral_call},
    {NULL, NULL}
};

library_t peripheral_lib = {"peripheral", peripheral_reg, nullptr, nullptr};