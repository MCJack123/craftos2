/*
 * peripheral/modem.cpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the modem peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#include "../runtime.hpp"
static std::string modem_message(lua_State *message, void* data);
#include "modem.hpp"
#include <list>
#include <unordered_map>
#include <configuration.hpp>
#include "../apis.hpp"

static std::unordered_map<int, std::list<modem*>> network;
static std::function<double(const Computer *, const Computer *)> distanceCallback = [](const Computer *, const Computer *)->double {return 0;};

/* extern */ void setDistanceProvider(const std::function<double(const Computer *, const Computer *)>& func) {
    distanceCallback = func;
}

// todo: probably check port range

int modem::isOpen(lua_State *L) {
    lastCFunction = __func__;
    if (luaL_checkinteger(L, 1) < 0 || lua_tointeger(L, 1) > 65535) luaL_error(L, "bad argument #1 (channel out of range)");
    lua_pushboolean(L, openPorts.find((uint16_t)lua_tointeger(L, 1)) != openPorts.end());
    return 1;
}

int modem::open(lua_State *L) {
    lastCFunction = __func__;
    if (luaL_checkinteger(L, 1) < 0 || lua_tointeger(L, 1) > 65535) luaL_error(L, "bad argument #1 (channel out of range)"); // argument error > too many open channels
    if (openPorts.size() >= (size_t)config.maxOpenPorts) luaL_error(L, "Too many open channels");
    openPorts.insert((uint16_t)lua_tointeger(L, 1));
    return 0;
}

int modem::close(lua_State *L) {
    lastCFunction = __func__;
    if (luaL_checkinteger(L, 1) < 0 || lua_tointeger(L, 1) > 65535) luaL_error(L, "bad argument #1 (channel out of range)");
    openPorts.erase((uint16_t)lua_tointeger(L, 1));
    return 0;
}

int modem::closeAll(lua_State *L) {
    lastCFunction = __func__;
    openPorts.clear();
    return 0;
}

int modem::transmit(lua_State *L) {
    lastCFunction = __func__;
    luaL_checkinteger(L, 2);
    luaL_checkany(L, 3);
    if (luaL_checkinteger(L, 1) < 0 || lua_tointeger(L, 1) > 65535) luaL_error(L, "bad argument #1 (channel out of range)");
    const uint16_t port = (uint16_t)lua_tointeger(L, 1);
    for (modem* m : network[netID]) if (m != this && m->openPorts.find(port) != m->openPorts.end()) {
        lua_pushvalue(L, 3);
        m->receive(L, port, (uint16_t)luaL_checkinteger(L, 2), this);
    }
    return 0;
}

int modem::isWireless(lua_State *L) {
    lastCFunction = __func__;
    lua_pushboolean(L, false);
    return 1;
}

int modem::getNamesRemote(lua_State *L) {
    lastCFunction = __func__;
    int i = 1;
    std::lock_guard<std::mutex> lock(comp->peripherals_mutex);
    lua_createtable(L, comp->peripherals.size(), 0);
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
    lastCFunction = __func__;
    if (strcmp(peripheral_lib.functions[1].name, "getType") == 0) return peripheral_lib.functions[1].func(L);
    for (int i = 0; peripheral_lib.functions[i].name; i++) if (strcmp(peripheral_lib.functions[i].name, "getType") == 0) return peripheral_lib.functions[i].func(L);
    return luaL_error(L, "Internal error");
}

int modem::isPresentRemote(lua_State *L) {
    lastCFunction = __func__;
    if (strcmp(peripheral_lib.functions[0].name, "isPresent") == 0) return peripheral_lib.functions[0].func(L);
    for (int i = 0; peripheral_lib.functions[i].name; i++) if (strcmp(peripheral_lib.functions[i].name, "isPresent") == 0) return peripheral_lib.functions[i].func(L);
    return luaL_error(L, "Internal error");
}

int modem::getMethodsRemote(lua_State *L) {
    lastCFunction = __func__;
    if (strcmp(peripheral_lib.functions[2].name, "getMethods") == 0) return peripheral_lib.functions[2].func(L);
    for (int i = 0; peripheral_lib.functions[i].name; i++) if (strcmp(peripheral_lib.functions[i].name, "getMethods") == 0) return peripheral_lib.functions[i].func(L);
    return luaL_error(L, "Internal error");
}

int modem::callRemote(lua_State *L) {
    lastCFunction = __func__;
    if (strcmp(peripheral_lib.functions[3].name, "call") == 0) return peripheral_lib.functions[3].func(L);
    for (int i = 0; peripheral_lib.functions[i].name; i++) if (strcmp(peripheral_lib.functions[i].name, "call") == 0) return peripheral_lib.functions[i].func(L);
    return luaL_error(L, "Internal error");
}

int modem::hasTypeRemote(lua_State *L) {
    lastCFunction = __func__;
    if (strcmp(peripheral_lib.functions[4].name, "hasType") == 0) return peripheral_lib.functions[1].func(L);
    for (int i = 0; peripheral_lib.functions[i].name; i++) if (strcmp(peripheral_lib.functions[i].name, "hasType") == 0) return peripheral_lib.functions[i].func(L);
    return luaL_error(L, "Internal error");
}

struct modem_message_data {
    modem * m;
    int pos;
};

static std::string modem_message(lua_State *L, void* data) {
    modem_message_data * d = (modem_message_data*)data;
    std::lock_guard<std::mutex> lock(d->m->eventQueueMutex);
    lua_State *message = lua_tothread(d->m->eventQueue, d->pos);
    lua_checkstack(L, 5);
    lua_xmove(message, L, 5);
    if (lua_gettop(d->m->eventQueue) == d->pos) lua_pop(d->m->eventQueue, 1);
    else {
        lua_checkstack(d->m->eventQueue, 1);
        lua_pushnil(d->m->eventQueue);
        lua_replace(d->m->eventQueue, d->pos);
    }
    delete d;
    return "modem_message";
};

void modem::receive(lua_State *data, uint16_t port, uint16_t replyPort, modem * sender) {
    std::lock_guard<std::mutex> lock(eventQueueMutex);
    for (int i = lua_gettop(eventQueue); i > 0 && lua_isnil(eventQueue, i); i--) lua_pop(eventQueue, 1);
    lua_checkstack(eventQueue, 1);
    lua_State *message = lua_newthread(eventQueue);
    int id = 1, top = lua_gettop(eventQueue);
    while (id < top && !lua_isnil(eventQueue, id)) id++;
    if (id < top) lua_replace(eventQueue, id);
    lua_checkstack(message, 5);
    lua_pushstring(message, side.c_str());
    lua_pushinteger(message, port);
    lua_pushinteger(message, replyPort);
    xcopy(data, message, 1);
    lua_pushnumber(message, distanceCallback(sender->comp, comp));
    modem_message_data * d = new modem_message_data;
    d->m = this;
    d->pos = id;
    queueEvent(comp, modem_message, d);
}

modem::modem(lua_State *L, const char * side) {
    if (lua_isnumber(L, 3)) netID = (int)lua_tointeger(L, 3);
    comp = get_comp(L);
    eventQueue = lua_newthread(comp->L);
    eventQueuePosition = lua_gettop(comp->L);
    this->side = side;
    if (network.find(netID) == network.end()) network[netID] = std::list<modem*>();
    network[netID].push_back(this);
    lua_setlockstate(L, true);
}

void modem::reinitialize(lua_State *L) {
    // eventQueue should be freed and inaccessible since the Lua state was closed
    eventQueue = lua_newthread(L);
    eventQueuePosition = lua_gettop(comp->L);
    lua_setlockstate(L, true);
}

modem::~modem() {
    for (std::list<modem*>::iterator it = network[netID].begin(); it != network[netID].end(); ++it) {if (*it == this) {network[netID].erase(it); return;}}
    std::lock_guard<std::mutex> lock(eventQueueMutex);
    if (lua_gettop(comp->L) == eventQueuePosition) lua_pop(comp->L, 1);
    else {
        lua_pushnil(comp->L);
        lua_replace(comp->L, eventQueuePosition);
    }
    for (const auto& p : comp->peripherals)
        if (std::string(p.second->getMethods().name) == "modem" && p.second != this)
            return;
    lua_setlockstate(comp->L, false);
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
    else if (m == "hasTypeRemote") return hasTypeRemote(L);
    else return luaL_error(L, "No such method");
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
    {"hasTypeRemote", NULL},
    {NULL, NULL}
};

library_t modem::methods = {"modem", modem_reg, nullptr, nullptr};
