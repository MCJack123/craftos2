/*
 * peripheral/modem.cpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the modem peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include "modem.hpp"
#include "../term.hpp"
#include <list>

std::list<modem*> network;

int modem::isOpen(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    lua_pushboolean(L, openPorts.find(lua_tointeger(L, 1)) != openPorts.end());
    return 1;
}

int modem::open(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    openPorts.insert(lua_tointeger(L, 1));
    return 0;
}

int modem::close(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    openPorts.erase(lua_tointeger(L, 1));
    return 0;
}

int modem::closeAll(lua_State *L) {
    openPorts.clear();
    return 0;
}

int modem::transmit(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
    if (lua_isnone(L, 3)) bad_argument(L, "value", 3);
    lua_settop(L, 3);
    uint16_t port = lua_tointeger(L, 1);
    for (modem* m : network) if (m != this && m->openPorts.find(port) != m->openPorts.end()) m->receive(port, lua_tointeger(L, 2), L);
    return 0;
}

int modem::isWireless(lua_State *L) {
    lua_pushboolean(L, true);
    return 1;
}

const char * modem_message(lua_State *L, void* data) {
    lua_State *message = (lua_State*)data;
    lua_xmove(message, L, 4);
    modem * m = (modem*)lua_touserdata(message, 1);
    lua_remove(m->eventQueue, 1);
    return "modem_message";
};

static void xcopy1(lua_State *L, lua_State *T, int n) {
    switch (lua_type(L, n)) {
    case LUA_TBOOLEAN:
        lua_pushboolean(T, lua_toboolean(L, n));
        break;
    case LUA_TNUMBER:
        lua_pushnumber(T, lua_tonumber(L, n));
        break;
    case LUA_TSTRING:
        lua_pushlstring(T, lua_tostring(L, n), lua_strlen(L, n));
        break;
    case LUA_TLIGHTUSERDATA:
        lua_pushlightuserdata(T, (void *)lua_touserdata(L, n));
        break;
    case LUA_TFUNCTION:
        if (lua_iscfunction(L, n)) lua_pushcfunction(T, lua_tocfunction(L, n));
        else lua_pushnil(T);
        break;
    default:
        lua_pushnil(T);
        break;
    }
}

/* table is in the stack at index 't' */
static void xcopy(lua_State *L, lua_State *T, int t) {
    int w;
    lua_newtable(T);
    w = lua_gettop(T);
    lua_pushnil(L); /* first key */
    while (lua_next(L, t) != 0) {
        xcopy1(L, T, -2);
        if (lua_type(L, -1) == LUA_TTABLE)
            xcopy(L, T, lua_gettop(L));
        else
            xcopy1(L, T, -1);
        lua_settable(T, w);
        lua_pop(L, 1);
    }
}

void modem::receive(uint16_t port, uint16_t replyPort, lua_State *param) {
    lua_checkstack(eventQueue, 5);
    lua_State *message = lua_newthread(eventQueue);
    lua_pushlightuserdata(message, this);
    lua_pushstring(message, side.c_str());
    lua_pushinteger(message, lua_tointeger(param, 1));
    lua_pushinteger(message, lua_tointeger(param, 2));
    if (lua_type(param, 3) == LUA_TNUMBER) lua_pushnumber(message, lua_tonumber(param, 3));
    else if (lua_type(param, 3) == LUA_TSTRING) lua_pushlstring(message, lua_tostring(param, 3), lua_strlen(param, 3));
    else if (lua_type(param, 3) == LUA_TBOOLEAN) lua_pushboolean(message, lua_toboolean(param, 3));
    else if (lua_type(param, 3) == LUA_TLIGHTUSERDATA) lua_pushlightuserdata(message, lua_touserdata(param, 3));
    else if (lua_type(param, 3) == LUA_TFUNCTION && lua_iscfunction(param, 3)) lua_pushcfunction(message, lua_tocfunction(param, 3));
    else if (lua_type(param, 3) == LUA_TTABLE) xcopy(param, message, 3);
    else lua_pushnil(message);
    termQueueProvider(comp, modem_message, message);
}

modem::modem(lua_State *L, const char * side) {
    comp = get_comp(L);
    eventQueue = lua_newthread(comp->L);
    this->side = side;
    network.push_back(this);
}

modem::~modem() {
    for (std::list<modem*>::iterator it = network.begin(); it != network.end(); it++) {if (*it == this) {network.erase(it); return;}}
}

int modem::call(lua_State *L, const char * method) {
    std::string m(method);
    if (m == "isOpen") return isOpen(L);
    else if (m == "open") return open(L);
    else if (m == "close") return close(L);
    else if (m == "closeAll") return closeAll(L);
    else if (m == "transmit") return transmit(L);
    else if (m == "isWireless") return isWireless(L);
    else return 0;
}

const char * modem_keys[6] = {
    "isOpen",
    "open",
    "close",
    "closeAll",
    "transmit",
    "isWireless"
};

library_t modem::methods = {"modem", 6, modem_keys, NULL, nullptr, nullptr};