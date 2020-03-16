/*
 * peripheral/peripheral.cpp
 * CraftOS-PC 2
 * 
 * This file defines the functions in the peripheral API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include "peripheral.hpp"
#include <unordered_map>
#include <string>

peripheral::~peripheral(){}

int peripheral_isPresent(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
    std::lock_guard<std::mutex> lock(computer->peripherals_mutex);
    lua_pushboolean(L, computer->peripherals.find(std::string(lua_tostring(L, -1))) != computer->peripherals.end());
    return 1;
}

int peripheral_getType(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
    std::string side(lua_tostring(L, 1));
    std::lock_guard<std::mutex> lock(computer->peripherals_mutex);
    if (computer->peripherals.find(side) != computer->peripherals.end())
        lua_pushstring(L, computer->peripherals[side]->getMethods().name);
    else return 0;
    return 1;
}

int peripheral_getMethods(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
    std::string side(lua_tostring(L, 1));
    std::lock_guard<std::mutex> lock(computer->peripherals_mutex);
    if (computer->peripherals.find(side) == computer->peripherals.end()) return 0;
    library_t methods = computer->peripherals[side]->getMethods();
    lua_newtable(L);
    for (int i = 0; i < methods.count; i++) {
        lua_pushnumber(L, i+1);
        lua_pushstring(L, methods.keys[i]);
        lua_settable(L, -3);
    }
    return 1;
}

int peripheral_call(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    Computer * computer = get_comp(L);
    std::string side(lua_tostring(L, 1));
    std::string func(lua_tostring(L, 2));
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

const char * peripheral_keys[4] = {
    "isPresent",
    "getType",
    "getMethods",
    "call"
};

lua_CFunction peripheral_values[4] = {
    peripheral_isPresent,
    peripheral_getType,
    peripheral_getMethods,
    peripheral_call
};

library_t peripheral_lib = {"peripheral", 4, peripheral_keys, peripheral_values, nullptr, nullptr};