/*
 * peripheral/peripheral.cpp
 * CraftOS-PC 2
 * 
 * This file defines the functions in the peripheral API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "peripheral.hpp"
#include <unordered_map>
#include <string>

int peripheral_isPresent(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
    computer->peripherals_mutex.lock();
    lua_pushboolean(L, computer->peripherals.find(std::string(lua_tostring(L, -1))) != computer->peripherals.end());
    computer->peripherals_mutex.unlock();
    return 1;
}

int peripheral_getType(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
    std::string side(lua_tostring(L, 1));
    computer->peripherals_mutex.lock();
    if (computer->peripherals.find(side) != computer->peripherals.end())
        lua_pushstring(L, computer->peripherals[side]->getMethods().name);
    else { computer->peripherals_mutex.unlock(); return 0; }
    computer->peripherals_mutex.unlock();
    return 1;
}

int peripheral_getMethods(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
    std::string side(lua_tostring(L, 1));
    computer->peripherals_mutex.lock();
    if (computer->peripherals.find(side) == computer->peripherals.end()) { computer->peripherals_mutex.unlock(); return 0; }
    library_t methods = computer->peripherals[side]->getMethods();
    computer->peripherals_mutex.unlock();
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
    computer->peripherals_mutex.lock();
    if (computer->peripherals.find(side) == computer->peripherals.end()) { computer->peripherals_mutex.unlock(); return 0; }
    lua_State *param = lua_newthread(L);
    lua_xmove(L, param, lua_gettop(L)-2);
    lua_xmove(param, L, 1);
    int retval = computer->peripherals[side]->call(param, func.c_str());
    lua_xmove(param, L, lua_gettop(param));
    lua_remove(L, 3);
    computer->peripherals_mutex.unlock();
    //assert(lua_gettop(L) == top + retval);
    return retval;
}

void peripheral_update(Computer *comp) {
    comp->peripherals_mutex.lock();
    for (auto p : comp->peripherals) p.second->update();
    comp->peripherals_mutex.unlock();
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

library_t peripheral_lib = {"peripheral", 4, peripheral_keys, peripheral_values, NULL, NULL};