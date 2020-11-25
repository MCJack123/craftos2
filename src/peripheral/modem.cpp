/*
 * peripheral/modem.cpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the modem peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include "../runtime.hpp"
static std::string modem_message(lua_State *message, void* data);
#include "modem.hpp"
#include <list>
#include <unordered_map>
#include <configuration.hpp>
#include "../apis.hpp"

extern "C" {
    extern void _lua_lock(lua_State *L);
    extern void _lua_unlock(lua_State *L);
}

static std::unordered_map<int, std::list<modem*>> network;

// todo: probably check port range

int modem::isOpen(lua_State *L) {
    lua_pushboolean(L, openPorts.find((uint16_t)luaL_checkinteger(L, 1)) != openPorts.end());
    return 1;
}

int modem::open(lua_State *L) {
    luaL_checknumber(L, 1); // argument error > too many open channels
    if (openPorts.size() >= (size_t)config.maxOpenPorts) luaL_error(L, "Too many open channels");
    openPorts.insert((uint16_t)lua_tointeger(L, 1));
    return 0;
}

int modem::close(lua_State *L) {
    openPorts.erase((uint16_t)luaL_checkinteger(L, 1));
    return 0;
}

int modem::closeAll(lua_State *L) {
    openPorts.clear();
    return 0;
}

int modem::transmit(lua_State *L) {
    luaL_checkinteger(L, 2);
    luaL_checkany(L, 3);
    lua_settop(L, 3);
    const uint16_t port = (uint16_t)luaL_checkinteger(L, 1);
    std::lock_guard<std::mutex> lock(eventQueueMutex);
    if (!idsToDelete.empty()) {
        for (int i : idsToDelete) {
            lua_pushinteger(eventQueue, i);
            lua_pushnil(eventQueue);
            lua_settable(eventQueue, 1);
        }
        idsToDelete.clear();
    }
    const int id = (int)lua_objlen(eventQueue, 1) + 1;
    int * refc = new int(0);
    lua_pushinteger(eventQueue, id);
    lua_newtable(eventQueue);
    lua_pushlightuserdata(eventQueue, refc);
    lua_setfield(eventQueue, -2, "refcount");
    lua_xmove(L, eventQueue, 1);
    lua_setfield(eventQueue, -2, "data");
    lua_settable(eventQueue, 1);
    for (modem* m : network[netID]) if (m != this && m->openPorts.find(port) != m->openPorts.end()) {
        m->receive(port, (uint16_t)lua_tointeger(L, 2), id, this);
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
    lua_pushboolean(L, false);
    return 1;
}

int modem::getNamesRemote(lua_State *L) {
    lua_newtable(L);
    int i = 1;
    std::lock_guard<std::mutex> lock(comp->peripherals_mutex);
    for (const auto& p : comp->peripherals) {
        if (p.first != "top" && p.first != "bottom" && p.first != "left" && p.first != "right" && p.first != "front" && p.first != "back") {
            lua_pushinteger(L, i++);
            lua_pushstring(L, p.first.c_str());
            lua_settable(L, -3);
        }
    }
    return 1;
}

int modem::getTypeRemote(lua_State *L) {
    if (strcmp(peripheral_lib.functions[1].name, "getType") == 0) return peripheral_lib.functions[1].func(L);
    for (int i = 0; peripheral_lib.functions[i].name; i++) if (strcmp(peripheral_lib.functions[i].name, "getType") == 0) return peripheral_lib.functions[i].func(L);
    return luaL_error(L, "Internal error");
}

int modem::isPresentRemote(lua_State *L) {
    if (strcmp(peripheral_lib.functions[0].name, "isPresent") == 0) return peripheral_lib.functions[0].func(L);
    for (int i = 0; peripheral_lib.functions[i].name; i++) if (strcmp(peripheral_lib.functions[i].name, "isPresent") == 0) return peripheral_lib.functions[i].func(L);
    return luaL_error(L, "Internal error");
}

int modem::getMethodsRemote(lua_State *L) {
    if (strcmp(peripheral_lib.functions[2].name, "getMethods") == 0) return peripheral_lib.functions[2].func(L);
    for (int i = 0; peripheral_lib.functions[i].name; i++) if (strcmp(peripheral_lib.functions[i].name, "getMethods") == 0) return peripheral_lib.functions[i].func(L);
    return luaL_error(L, "Internal error");
}

int modem::callRemote(lua_State *L) {
    if (strcmp(peripheral_lib.functions[3].name, "call") == 0) return peripheral_lib.functions[3].func(L);
    for (int i = 0; peripheral_lib.functions[i].name; i++) if (strcmp(peripheral_lib.functions[i].name, "call") == 0) return peripheral_lib.functions[i].func(L);
    return luaL_error(L, "Internal error");
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
    default:
        lua_pushnil(T);
        break;
    }
}

/* table is in the stack at index 't' */
static void xcopy(lua_State *L, lua_State *T, int t) {
    lua_newtable(T);
    int w = lua_gettop(T);
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

static std::string modem_message(lua_State *message, void* data) {
    struct modem_message_data * d = (modem_message_data*)data;
    if (d->sender == NULL) {
        fprintf(stderr, "Modem message event is missing sender, skipping event");
        delete d;
        return NULL;
    }
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
    d->m->modemMessages.erase((void*)d);
    delete d;
    lua_pushinteger(message, 0);
    return "modem_message";
};

void modem::receive(uint16_t port, uint16_t replyPort, int id, modem * sender) {
    struct modem_message_data * d = new struct modem_message_data;
    d->id = id;
    d->port = port;
    d->replyPort = replyPort;
    d->m = this;
    d->sender = sender;
    modemMessages.insert((void*)d);
    queueEvent(comp, modem_message, d);
}

modem::modem(lua_State *L, const char * side) {
    if (lua_isnumber(L, 3)) netID = (int)lua_tointeger(L, 3);
    comp = get_comp(L);
    eventQueue = lua_newthread(comp->L);
    lua_newtable(eventQueue);
    this->side = side;
    if (network.find(netID) == network.end()) network[netID] = std::list<modem*>();
    network[netID].push_back(this);
}

void modem::reinitialize(lua_State *L) {
    // eventQueue should be freed and inaccessible since the Lua state was closed
    eventQueue = lua_newthread(L);
    lua_newtable(eventQueue);
}

modem::~modem() {
    for (std::list<modem*>::iterator it = network[netID].begin(); it != network[netID].end(); ++it) {if (*it == this) {network[netID].erase(it); return;}}
    std::lock_guard<std::mutex> lock(eventQueueMutex);
    for (void* d : modemMessages) {
        ((modem_message_data*)d)->sender = NULL;
        lua_pushinteger(eventQueue, ((modem_message_data*)d)->id);
        lua_gettable(eventQueue, 1);
        lua_getfield(eventQueue, -1, "refcount");
        delete (int*)lua_touserdata(eventQueue, -1);
        lua_pop(eventQueue, 2);
    }
    lua_pop(eventQueue, 1);
    for (int i = 1; i < lua_gettop(comp->L); i++) if (lua_type(comp->L, i) == LUA_TTHREAD && lua_tothread(comp->L, i) == eventQueue) lua_remove(comp->L, i--);
}

int modem::call(lua_State *L, const char * method) {
    const std::string m(method);
    if (m == "isOpen") return isOpen(L);
    else if (m == "open") return open(L);
    else if (m == "close") return close(L);
    else if (m == "closeAll") return closeAll(L);
    else if (m == "transmit") return transmit(L);
    else if (m == "isWireless") return isWireless(L);
    else if (m == "getNamesRemote") return getNamesRemote(L);
    else if (m == "getTypeRemote") return getTypeRemote(L);
    else if (m == "isPresentRemote") return isPresentRemote(L);
    else if (m == "getMethodsRemote") return getMethodsRemote(L);
    else if (m == "callRemote") return callRemote(L);
    else return 0;
}

static luaL_Reg modem_reg[] = {
    {"isOpen", NULL},
    {"open", NULL},
    {"close", NULL},
    {"closeAll", NULL},
    {"transmit", NULL},
    {"isWireless", NULL},
    {"getNamesRemote", NULL},
    {"getTypeRemote", NULL},
    {"isPresentRemote", NULL},
    {"getMethodsRemote", NULL},
    {"callRemote", NULL},
    {NULL, NULL}
};

library_t modem::methods = {"modem", modem_reg, nullptr, nullptr};
