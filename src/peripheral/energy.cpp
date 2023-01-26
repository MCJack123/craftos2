/*
 * peripheral/energy.cpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the energy peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2023 JackMacWindows.
 */

#include "energy.hpp"

int energy::getEnergy() {
    return currentEnergy;
}

int energy::getEnergyCapacity() {
    return maxEnergy;
}

int energy::setEnergy(lua_State *L) {
    currentEnergy = min(max((int)luaL_checkinteger(L, 1), 0), maxEnergy);
    return 0;
}

energy::energy(lua_State *L, const char * side) {
    maxEnergy = luaL_optinteger(L, 3, maxEnergy);
    if (lua_istable(L, 4)) {
        lua_pushinteger(L, 1);
        lua_gettable(L, 4);
        for (int i = 2; lua_isstring(L, -1); i++) {
            types.push_back(tostring(L, -1));
            lua_pop(L, 1);
            lua_pushinteger(L, i);
            lua_gettable(L, 4);
        }
        lua_pop(L, 1);
    } else if (!lua_isnoneornil(L, 4)) luaL_checktype(L, 4, LUA_TTABLE);
}

energy::~energy() {}

int energy::call(lua_State *L, const char * method) {
    const std::string m(method);
    if (m == "setEnergy") return setEnergy(L);
    else return energy_storage::call(L, method);
}

static luaL_Reg energy_reg[] = {
    {"getEnergy", NULL},
    {"getEnergyCapacity", NULL},
    {"setEnergy", NULL},
    {NULL, NULL}
};

library_t energy::methods = {"!!MULTITYPE", energy_reg, nullptr, nullptr};