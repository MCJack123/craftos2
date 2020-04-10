/*
 * peripheral/modem.cpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the modem peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#define CRAFTOSPC_INTERNAL
#include "modem.hpp"
#include "../term.hpp"
#include <list>
#include <unordered_map>

std::unordered_map<int, std::list<modem*>> network;

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
    std::lock_guard<std::mutex> lock(eventQueueMutex);
    if (idsToDelete.size() > 0) {
        for (int i : idsToDelete) {
            lua_pushinteger(eventQueue, i);
            lua_pushnil(eventQueue);
            lua_settable(eventQueue, 1);
        }
        idsToDelete.clear();
    }
    int id = lua_objlen(eventQueue, 1) + 1;
    int * refc = new int(0);
    lua_pushinteger(eventQueue, id);
    lua_newtable(eventQueue);
    lua_pushlightuserdata(eventQueue, refc);
    lua_setfield(eventQueue, -2, "refcount");
    lua_xmove(L, eventQueue, 1);
    lua_setfield(eventQueue, -2, "data");
    lua_settable(eventQueue, 1);
    for (modem* m : network[netID]) if (m != this && m->openPorts.find(port) != m->openPorts.end()) {
        m->receive(port, lua_tointeger(L, 2), id, this);
        (*refc)++;
    }
    if (*refc == 0) {
        lua_pushinteger(eventQueue, id);
        lua_pushnil(eventQueue);
        lua_settable(eventQueue, 1);
        delete refc;
    }
    return 0;
}

int modem::isWireless(lua_State *L) {
    lua_pushboolean(L, true);
    return 1;
}

struct modem_message_data {
    modem * m;
    modem * sender;
    int id;
    uint16_t port;
    uint16_t replyPort;
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
    //case LUA_TLIGHTUSERDATA:
    //    lua_pushlightuserdata(T, (void *)lua_touserdata(L, n));
    //    break;
    //case LUA_TFUNCTION:
    //    if (lua_iscfunction(L, n)) lua_pushcfunction(T, lua_tocfunction(L, n));
    //    else lua_pushnil(T);
    //    break;
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
    while (lua_next(L, t-(t<0)) != 0) {
        xcopy1(L, T, -2);
        if (lua_type(L, -1) == LUA_TTABLE)
            xcopy(L, T, lua_gettop(L));
        else
            xcopy1(L, T, -1);
        lua_settable(T, w);
        lua_pop(L, 1);
    }
}

const char * modem_message(lua_State *message, void* data) {
    struct modem_message_data * d = (struct modem_message_data*)data;
    lua_pushstring(message, d->m->side.c_str());
    lua_pushinteger(message, d->port);
    lua_pushinteger(message, d->replyPort);
    std::lock_guard<std::mutex> lock(d->sender->eventQueueMutex);
    lua_pushinteger(d->sender->eventQueue, d->id);
    lua_gettable(d->sender->eventQueue, 1);
    if (lua_isnil(d->sender->eventQueue, -1)) {
        fprintf(stderr, "Missing event data for id %d\n", d->id);
        delete d;
        return NULL;
    } 
    lua_getfield(d->sender->eventQueue, -1, "data");
    if (lua_type(d->sender->eventQueue, -1) == LUA_TNUMBER) lua_pushnumber(message, lua_tonumber(d->sender->eventQueue, -1));
    else if (lua_type(d->sender->eventQueue, -1) == LUA_TSTRING) lua_pushlstring(message, lua_tostring(d->sender->eventQueue, -1), lua_strlen(d->sender->eventQueue, -1));
    else if (lua_type(d->sender->eventQueue, -1) == LUA_TBOOLEAN) lua_pushboolean(message, lua_toboolean(d->sender->eventQueue, -1));
    else if (lua_type(d->sender->eventQueue, -1) == LUA_TLIGHTUSERDATA) lua_pushlightuserdata(message, lua_touserdata(d->sender->eventQueue, -1));
    else if (lua_type(d->sender->eventQueue, -1) == LUA_TFUNCTION && lua_iscfunction(d->sender->eventQueue, -1)) lua_pushcfunction(message, lua_tocfunction(d->sender->eventQueue, -1));
    else if (lua_type(d->sender->eventQueue, -1) == LUA_TTABLE) xcopy(d->sender->eventQueue, message, -1);
    else lua_pushnil(message);
    lua_pop(d->sender->eventQueue, 1);
    lua_getfield(d->sender->eventQueue, -1, "refcount");
    int * refc = (int*)lua_touserdata(d->sender->eventQueue, -1);
    lua_pop(d->sender->eventQueue, 2);
    if (!--(*refc)) {
        delete refc;
        d->sender->idsToDelete.insert(d->id);
    }
    delete d;
    return "modem_message";
};

void modem::receive(uint16_t port, uint16_t replyPort, int id, modem * sender) {
    struct modem_message_data * d = new struct modem_message_data;
    d->id = id;
    d->port = port;
    d->replyPort = replyPort;
    d->m = this;
    d->sender = sender;
    termQueueProvider(comp, modem_message, d);
}

modem::modem(lua_State *L, const char * side) {
    if (lua_isnumber(L, 3)) netID = lua_tointeger(L, 3);
    comp = get_comp(L);
    eventQueue = lua_newthread(comp->L);
    lua_newtable(eventQueue);
    this->side = side;
    if (network.find(netID) == network.end()) network[netID] = std::list<modem*>();
    network[netID].push_back(this);
}

modem::~modem() {
    for (std::list<modem*>::iterator it = network[netID].begin(); it != network[netID].end(); it++) {if (*it == this) {network[netID].erase(it); return;}}
    lua_pop(eventQueue, 1);
    for (int i = 1; i < lua_gettop(comp->L); i++) if (lua_type(comp->L, i) == LUA_TTHREAD && lua_tothread(comp->L, i) == eventQueue) lua_remove(comp->L, i--);
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