/*
 * peripheral/chest.cpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the chest peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#include "chest.hpp"

int chest::size() {
    if (isDouble) return 54;
    else return 27;
}

void chest::getItemDetail(lua_State *L, int slot) {
    if (slot < 1 || slot > (isDouble ? 54 : 27)) {lua_pushnil(L); return;}
    if (items[slot-1].count == 0) {lua_pushnil(L); return;}
    lua_createtable(L, 0, 2);
    lua_pushlstring(L, items[slot-1].name.c_str(), items[slot-1].name.size());
    lua_setfield(L, -2, "name");
    lua_pushinteger(L, items[slot-1].count);
    lua_setfield(L, -2, "count");
}

int chest::addItems(lua_State *L, int slot, int count) {
    lua_getfield(L, -1, "name");
    if (!lua_isstring(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }
    std::string name = tostring(L, -1);
    lua_pop(L, 1);
    if (slot == 0) for (slot = 1; slot <= (isDouble ? 54 : 27) && items[slot-1].count > 0 && items[slot-1].name != name; slot++);
    if (slot < 0 || slot > (isDouble ? 54 : 27) || (items[slot-1].name != name && !items[slot-1].name.empty())) return 0;
    uint8_t d = min(items[slot-1].count + count, 64) - items[slot-1].count;
    if (d > 0) items[slot-1].name = name;
    items[slot-1].count += d;
    return d;
}

int chest::removeItems(int slot, int count) {
    if (slot < 0 || slot > (isDouble ? 54 : 27) || count <= 0) return 0;
    count = min(count, (int)items[slot-1].count);
    items[slot-1].count -= count;
    if (items[slot-1].count == 0) items[slot-1].name = "";
    return count;
}

int chest::setItem(lua_State *L) {
    int slot = luaL_checknumber(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    lua_getfield(L, 2, "name");
    if (!lua_isstring(L, -1)) luaL_error(L, "bad field 'name' (expected string, got %s)", lua_typename(L, lua_type(L, -1)));
    lua_pop(L, 1);
    lua_getfield(L, 2, "count");
    if (!lua_isnumber(L, -1)) luaL_error(L, "bad field 'count' (expected number, got %s)", lua_typename(L, lua_type(L, -1)));
    int count = lua_tointeger(L, -1);
    lua_settop(L, 2);
    lua_pushinteger(L, addItems(L, slot, count));
    return 1;
}

chest::chest(lua_State *L, const char * side) {
    if (lua_toboolean(L, 3)) isDouble = true;
}

chest::~chest() {}

int chest::call(lua_State *L, const char * method) {
    const std::string m(method);
    if (m == "setItem") return setItem(L);
    else return inventory::call(L, method);
}

static luaL_Reg chest_reg[] = {
    {"size", NULL},
    {"list", NULL},
    {"getItemDetail", NULL},
    {"pushItems", NULL},
    {"pullItems", NULL},
    {"setItem", NULL},
    {NULL, NULL}
};

library_t chest::methods = {"!!MULTITYPE", chest_reg, nullptr, nullptr};
std::vector<std::string> chest::types = {"minecraft:chest", "inventory", "chest"};
