/*
 * apis/periphemu.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the periphemu API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
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

static std::unordered_map<std::string, peripheral_init> initializers = {
    {"monitor", &monitor::init},
    {"printer", &printer::init},
    {"computer", &computer::init},
    {"modem", &modem::init},
    {"drive", &drive::init},
#ifndef NO_MIXER
    {"speaker", &speaker::init}
#endif
};

void registerPeripheral(const std::string& name, const peripheral_init& initializer) {
    initializers[name] = initializer;
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

static int periphemu_create(lua_State* L) {
    lastCFunction = __func__;
    if (!lua_isstring(L, 1) && !lua_isnumber(L, 1)) return luaL_typerror(L, 1, "string or number");
    Computer * computer = get_comp(L);
    const std::string type = luaL_checkstring(L, 2);
    if (config.serverMode && type == "speaker") {
        lua_pushboolean(L, false);
        lua_pushstring(L, "No peripheral named speaker");
        return 2;
    }
    std::string side = lua_isnumber(L, 1) ? type + "_" + std::to_string(lua_tointeger(L, 1)) : lua_tostring(L, 1);
    if (std::all_of(side.begin(), side.end(), ::isdigit)) side = type + "_" + side;
    computer->peripherals_mutex.lock();
    if (computer->peripherals.find(side) != computer->peripherals.end()) {
        computer->peripherals_mutex.unlock();
        lua_pushboolean(L, false);
        lua_pushfstring(L, "Peripheral already attached on side %s", side.c_str());
        return 2;
    }
    computer->peripherals_mutex.unlock();
    try {
        peripheral * p;
        if (type == "debugger" && config.debug_enable) p = new debugger(L, side.c_str());
        else if (initializers.find(type) != initializers.end()) p = initializers[type](L, side.c_str());
        else {
            //fprintf(stderr, "not found: %s\n", type.c_str());
            lua_pushboolean(L, false);
            if (type == "debugger") lua_pushfstring(L, "Set debug_enable to true in the config to enable the debugger");
            else lua_pushfstring(L, "No peripheral named %s", type.c_str());
            return 2;
        }
        computer->peripherals_mutex.lock();
        try { computer->peripherals[side] = p; } catch (...) {}
        computer->peripherals_mutex.unlock();
    } catch (std::exception &e) {
        return luaL_error(L, "Error while creating peripheral: %s", e.what());
    }
    lua_pushboolean(L, true);
    std::string * sidearg = new std::string(side);
    queueEvent(computer, peripheral_attach, sidearg);
    return 1;
}

static int periphemu_remove(lua_State* L) {
    lastCFunction = __func__;
    Computer * computer = get_comp(L);
    const std::string side = luaL_checkstring(L, 1);
    peripheral * p;
    {
        std::lock_guard<std::mutex> lock(computer->peripherals_mutex);
        if (computer->peripherals.find(side) == computer->peripherals.end()) {
            lua_pushboolean(L, false);
            return 1;
        }
        if (std::string(computer->peripherals[side]->getMethods().name) == "drive")
            computer->peripherals[side]->call(L, "ejectDisk");
        else if (std::string(computer->peripherals[side]->getMethods().name) == "debugger")
            computer->peripherals[side]->call(L, "deinit");
        p = computer->peripherals[side];
        computer->peripherals.erase(side);
    }
    queueTask([ ](void* p)->void*{((peripheral*)p)->getDestructor()((peripheral*)p); return NULL;}, p);
    lua_pushboolean(L, true);
    std::string * sidearg = new std::string(side);
    queueEvent(computer, peripheral_detach, sidearg);
    return 1;
}

static int periphemu_names(lua_State *L) {
    lastCFunction = __func__;
    lua_createtable(L, initializers.size() + 1, 0);
    lua_pushinteger(L, 1);
    lua_pushstring(L, "debugger");
    lua_settable(L, -3);
    int i = 2;
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