/*
 * apis/periphemu.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the periphemu API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#include <algorithm>
#include <Computer.hpp>
#include <Terminal.hpp>
#include "../peripheral/computer.hpp"
#include "../peripheral/debugger.hpp"
#include "../peripheral/drive.hpp"
#include "../peripheral/modem.hpp"
#include "../peripheral/monitor.hpp"
#include "../peripheral/printer.hpp"
#include "../peripheral/speaker.hpp"
#include "../runtime.hpp"
#include "../util.hpp"

static std::unordered_map<std::string, peripheral_init_fn> initializers = {
    {"monitor", peripheral_init_fn(monitor::init)},
    {"computer", peripheral_init_fn(computer::init)},
    {"debugger", peripheral_init_fn(debugger::_init)},
    {"printer", peripheral_init_fn(printer::init)},
    {"modem", peripheral_init_fn(modem::init)},
    {"drive", peripheral_init_fn(drive::init)},
#ifndef NO_MIXER
    {"speaker", peripheral_init_fn(speaker::init)}
#endif
};

void registerPeripheral(const std::string& name, const peripheral_init_fn& initializer) {
    initializers[name] = initializer;
}

void clearPeripherals() {
    initializers.clear();
}

static std::string peripheral_attach(lua_State *L, void* arg) {
    std::string * side = (std::string*)arg;
    lua_pushstring(L, side->c_str());
    delete side;
    return "peripheral";
}

static std::string peripheral_detach(lua_State *L, void* arg) {
    std::string * side = (std::string*)arg;
    lua_pushstring(L, side->c_str());
    delete side;
    return "peripheral_detach";
}

peripheral* attachPeripheral(Computer * computer, const std::string& side, const std::string& type, std::string * errorReturn, const char * format, ...) {
    if (config.serverMode && type == "speaker") {
        if (errorReturn != NULL) *errorReturn = "No peripheral named speaker";
        return NULL;
    }
    {
        std::lock_guard<std::mutex> lock(computer->peripherals_mutex);
        if (computer->peripherals.find(side) != computer->peripherals.end()) {
            if (errorReturn != NULL) *errorReturn = "Peripheral already attached on side " + side;
            return NULL;
        }
    }
    lua_State *L;
    int idx = -1;
    va_list arg;
    va_start(arg, format);
    if (*format == 'L') {
        L = va_arg(arg, lua_State*);
    } else {
        L = lua_newthread(computer->L);
        idx = lua_gettop(computer->L);
        lua_pushstring(L, side.c_str());
        lua_pushstring(L, type.c_str());
        while (*format) {
            switch (*format) {
            case 'i': lua_pushinteger(L, va_arg(arg, lua_Integer)); break;
            case 'n': lua_pushnumber(L, va_arg(arg, lua_Number)); break;
            case 's': lua_pushstring(L, va_arg(arg, const char *)); break;
            case 'b': lua_pushboolean(L, va_arg(arg, int)); break;
            case 'N': lua_pushnil(L); va_arg(arg, void*); break;
            default: throw std::invalid_argument(std::string("Invalid format specifier ") + *format);
            }
        }
    }
    peripheral * p;
    if (initializers.find(type) != initializers.end()) p = initializers[type](L, side.c_str());
    else {
        //fprintf(stderr, "not found: %s\n", type.c_str());
        if (errorReturn != NULL) *errorReturn = "No peripheral named " + type;
        return NULL;
    }
    computer->peripherals_mutex.lock();
    try { computer->peripherals[side] = p; } catch (...) {}
    computer->peripherals_mutex.unlock();
    if (idx != -1) {
        if (lua_gettop(computer->L) == idx) lua_pop(computer->L, 1);
        else lua_remove(computer->L, idx);
    }
    std::string * sidearg = new std::string(side);
    queueEvent(computer, peripheral_attach, sidearg);
    va_end(arg);
    return p;
}

bool detachPeripheral(Computer * computer, const std::string& side) {
    peripheral * p;
    {
        std::lock_guard<std::mutex> lock(computer->peripherals_mutex);
        if (computer->peripherals.find(side) == computer->peripherals.end()) return false;
        if (std::string(computer->peripherals[side]->getMethods().name) == "drive")
            computer->peripherals[side]->call(computer->L, "ejectDisk");
        else if (std::string(computer->peripherals[side]->getMethods().name) == "debugger")
            computer->peripherals[side]->call(NULL, "deinit");
        p = computer->peripherals[side];
        computer->peripherals.erase(side);
    }
    queueTask([ ](void* p)->void*{((peripheral*)p)->getDestructor()((peripheral*)p); return NULL;}, p);
    std::string * sidearg = new std::string(side);
    queueEvent(computer, peripheral_detach, sidearg);
    return true;
}

static int periphemu_create(lua_State* L) {
    lastCFunction = __func__;
    if (!lua_isstring(L, 1) && !lua_isnumber(L, 1)) return luaL_typerror(L, 1, "string or number");
    Computer * computer = get_comp(L);
    const std::string type = luaL_checkstring(L, 2);
    std::string side = lua_isnumber(L, 1) ? type + "_" + std::to_string(lua_tointeger(L, 1)) : lua_tostring(L, 1);
    if (std::all_of(side.begin(), side.end(), ::isdigit)) side = type + "_" + side;
    peripheral * p;
    std::string err;
    try {
        p = attachPeripheral(computer, side, type, &err, "L", L);
    } catch (std::exception &e) {
        return luaL_error(L, "Error while creating peripheral: %s", e.what());
    }
    if (p == NULL) {
        lua_pushboolean(L, false);
        lua_pushstring(L, err.c_str());
        return 2;
    }
    lua_pushboolean(L, true);
    return 1;
}

static int periphemu_remove(lua_State* L) {
    lastCFunction = __func__;
    Computer * computer = get_comp(L);
    const std::string side = luaL_checkstring(L, 1);
    lua_pushboolean(L, detachPeripheral(computer, side));
    return 1;
}

static int periphemu_names(lua_State *L) {
    lastCFunction = __func__;
    lua_createtable(L, initializers.size() + 1, 0);
    int i = 1;
    for (const auto& entry : initializers) {
        lua_pushinteger(L, i++);
        lua_pushstring(L, entry.first.c_str());
        lua_settable(L, -3);
    }
    return 1;
}

static luaL_Reg periphemu_reg[] = {
    {"create", periphemu_create},
    {"remove", periphemu_remove},
    {"names", periphemu_names},
    {NULL, NULL}
};

library_t periphemu_lib = { "periphemu", periphemu_reg, nullptr, nullptr };
