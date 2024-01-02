/*
 * peripheral/tank.cpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the tank peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#include "tank.hpp"

int tank::tanks(lua_State *L) {
    lua_createtable(L, tankCount, 0);
    for (int i = 0; i < tankCount; i++) {
        lua_newtable(L);
        int j = 1;
        for (const auto& fluid : fluids[i]) {
            lua_createtable(L, 0, 2);
            lua_pushlstring(L, fluid.first.c_str(), fluid.first.size());
            lua_setfield(L, -2, "name");
            lua_pushinteger(L, fluid.second);
            lua_setfield(L, -2, "amount");
            lua_rawseti(L, -2, j++);
        }
        lua_rawseti(L, -2, i+1);
    }
    return 1;
}

int tank::addFluid(const std::string& name, int amount) {
    // First try to add all the fluid in a contiguous amount if possible,
    // and if not then add it split across the tanks.
    std::vector<int> freeSpace(tankCount, tankCapacity);
    for (int i = 0; i < tankCount; i++) {
        for (const auto& fluid : fluids[i]) freeSpace[i] -= fluid.second;
        if (freeSpace[i] >= amount) {
            for (auto& fluid : fluids[i]) {
                if (fluid.first == name) {
                    fluid.second += amount;
                    return amount;
                }
            }
            fluids[i][name] = amount;
            return amount;
        }
    }
    const int total = amount;
    for (int i = 0; i < tankCount && amount > 0; i++) {
        if (freeSpace[i] == 0) continue;
        int d = min(amount, freeSpace[i]);
        for (auto& fluid : fluids[i]) {
            if (fluid.first == name) {
                fluid.second += d;
                amount -= d;
                d = 0;
                break;
            }
        }
        if (d) {
            fluids[i][name] = d;
            amount -= d;
        }
    }
    return total - amount;
}

std::list<std::pair<std::string, int>> tank::removeFluid(const std::string& name, int amount) {
    std::list<std::pair<std::string, int>> retval;
    if (name == "") {
        for (int i = 0; i < tankCount && amount > 0; i++) {
            for (auto it = fluids[i].begin(); amount > 0 && it != fluids[i].end(); it = fluids[i].begin()) {
                if (amount >= it->second) {
                    retval.push_back(*it);
                    amount -= it->second;
                    fluids[i].erase(it);
                } else {
                    retval.push_back(std::make_pair(it->first, amount));
                    it->second -= amount;
                    amount = 0;
                    break;
                }
            }
        }
    } else {
        for (int i = 0; i < tankCount && amount > 0; i++) {
            for (auto& fluid : fluids[i]) {
                if (fluid.first == name) {
                    if (amount >= fluid.second) {
                        retval.push_back(fluid);
                        amount -= fluid.second;
                        fluids[i].erase(fluid.first);
                    } else {
                        retval.push_back(std::make_pair(fluid.first, amount));
                        fluid.second -= amount;
                        amount = 0;
                    }
                    break;
                }
            }
        }
    }
    return retval;
}

tank::tank(lua_State *L, const char * side) {
    tankCount = luaL_optinteger(L, 3, tankCount);
    tankCapacity = luaL_optinteger(L, 4, tankCapacity);
    fluids.resize(tankCount);
    if (lua_istable(L, 5)) {
        lua_pushinteger(L, 1);
        lua_gettable(L, 5);
        for (int i = 2; lua_isstring(L, -1); i++) {
            types.push_back(tostring(L, -1));
            lua_pop(L, 1);
            lua_pushinteger(L, i);
            lua_gettable(L, 5);
        }
        lua_pop(L, 1);
    } else if (!lua_isnoneornil(L, 5)) luaL_checktype(L, 5, LUA_TTABLE);
}

tank::~tank() {}

int tank::call(lua_State *L, const char * method) {
    std::string m(method);
    if (m == "addFluid") {
        lua_pushinteger(L, addFluid(checkstring(L, 1), luaL_checkinteger(L, 2)));
        return 1;
    } else if (m == "removeFluid") {
        size_t sz = 0;
        const char * s = luaL_optlstring(L, 1, "", &sz);
        auto retval = removeFluid(std::string(s, sz), luaL_optinteger(L, 2, INT_MAX));
        lua_newtable(L);
        int j = 1;
        for (const std::pair<std::string, int>& fluid : retval) {
            lua_createtable(L, 0, 2);
            lua_pushlstring(L, fluid.first.c_str(), fluid.first.size());
            lua_setfield(L, -2, "name");
            lua_pushinteger(L, fluid.second);
            lua_setfield(L, -2, "amount");
            lua_rawseti(L, -2, j++);
        }
        return 1;
    } else return fluid_storage::call(L, method);
}

static luaL_Reg tank_reg[] = {
    {"tanks", NULL},
    {"pushFluid", NULL},
    {"pullFluid", NULL},
    {"addFluid", NULL},
    {"removeFluid", NULL},
    {NULL, NULL}
};

library_t tank::methods = {"!!MULTITYPE", tank_reg, nullptr, nullptr};
