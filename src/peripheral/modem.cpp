/*
 * peripheral/modem.cpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the modem peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019 JackMacWindows.
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
    if (lua_gettop(L) > 3) lua_pop(L, lua_gettop(L) - 3);
    lua_State *param = lua_newthread(L);
    lua_xmove(L, param, 2);
    lua_xmove(param, L, 1);
    uint16_t port = lua_tointeger(L, 1);
    for (modem* m : network) if (m != this && m->openPorts.find(port) != m->openPorts.end()) m->receive(port, lua_tointeger(L, 2), param);
    return 0;
}

int modem::isWireless(lua_State *L) {
    lua_pushboolean(L, true);
    return 1;
}

struct modem_message_data {
    const char * side;
    uint16_t port;
    uint16_t replyPort;
    lua_State *param;
};

const char * modem_message(lua_State *L, void* data) {
    struct modem_message_data * message = (struct modem_message_data*)data;
    lua_pushstring(L, message->side);
    lua_pushinteger(L, message->port);
    lua_pushinteger(L, message->replyPort);
    lua_xmove(message->param, L, 1);
    delete message;
    return "modem_message";
};

void modem::receive(uint16_t port, uint16_t replyPort, lua_State *param) {
    struct modem_message_data * message = new struct modem_message_data;
    message->side = side.c_str();
    message->port = port;
    message->replyPort = replyPort;
    message->param = param;
    termQueueProvider(comp, modem_message, message);
}

modem::modem(lua_State *L, const char * side) {
    comp = get_comp(L);
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

library_t modem::methods = {"modem", 6, modem_keys, NULL, NULL, NULL};