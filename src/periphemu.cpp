/*
 * periphemu.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the periphemu API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#define CRAFTOSPC_INTERNAL
#include "periphemu.hpp"
#include "peripheral/peripheral.hpp"
#include "peripheral/computer.hpp"
#include "peripheral/debugger.hpp"
#include "peripheral/drive.hpp"
#include "peripheral/modem.hpp"
#include "peripheral/monitor.hpp"
#include "peripheral/printer.hpp"
#include "peripheral/speaker.hpp"
#include "terminal/Terminal.hpp"
#include "term.hpp"
#include "os.hpp"
#include <unordered_map>
#include <string>
#include <algorithm>

monitor * findMonitorFromWindowID(Computer *comp, unsigned id, std::string& sideReturn) {
	std::lock_guard<std::mutex> lock(comp->peripherals_mutex);
    for (auto p : comp->peripherals) {
        if (p.second != NULL && strcmp(p.second->getMethods().name, "monitor") == 0) {
            monitor * m = (monitor*)p.second;
            if (m->term->id == id) {
                sideReturn.assign(p.first);
                return m;
            }
        }
    }
    return NULL;
}

std::unordered_map<std::string, peripheral_init> initializers = {
	{"monitor", &monitor::init},
	{"printer", &printer::init},
	{"computer", &computer::init},
	{"modem", &modem::init},
	{"drive", &drive::init},
#ifndef NO_MIXER
	{"speaker", &speaker::init}
#endif
};

void registerPeripheral(std::string name, peripheral_init initializer) {
	initializers[name] = initializer;
}

const char * peripheral_attach(lua_State *L, void* arg) {
    std::string * side = (std::string*)arg;
    lua_pushstring(L, side->c_str());
    delete side;
    return "peripheral";
}

const char * peripheral_detach(lua_State *L, void* arg) {
    std::string * side = (std::string*)arg;
    lua_pushstring(L, side->c_str());
    delete side;
    return "peripheral_detach";
}

int periphemu_create(lua_State* L) {
	if (!lua_isstring(L, 1) && !lua_isnumber(L, 1)) bad_argument(L, "string", 1);
	if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
	Computer * computer = get_comp(L);
	std::string type = lua_tostring(L, 2);
	std::string side = lua_isnumber(L, 1) ? type + "_" + std::to_string(lua_tointeger(L, 1)) : lua_tostring(L, 1);
	if (std::all_of(side.begin(), side.end(), ::isdigit)) side = type + "_" + side;
    computer->peripherals_mutex.lock();
	if (computer->peripherals.find(side) != computer->peripherals.end()) {
		computer->peripherals_mutex.unlock();
		lua_pushboolean(L, false);
		return 1;
	}
	computer->peripherals_mutex.unlock();
	//lua_pop(L, 2);
	try {
		peripheral * p;
		if (type == std::string("debugger") && computer->debugger == NULL && config.debug_enable) p = new debugger(L, side.c_str());
		else if (initializers.find(type) != initializers.end()) p = initializers[type](L, side.c_str());
        else {
			printf("not found: %s\n", type.c_str());
			lua_pushboolean(L, false);
			return 1;
		}
		computer->peripherals_mutex.lock();
		computer->peripherals[side] = p;
		computer->peripherals_mutex.unlock();
	} catch (std::exception &e) {
		return luaL_error(L, "Error while creating peripheral: %s", e.what());
	}
	lua_pushboolean(L, true);
    std::string * sidearg = new std::string(side);
    termQueueProvider(computer, peripheral_attach, sidearg);
	return 1;
}

int periphemu_remove(lua_State* L) {
	if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
	Computer * computer = get_comp(L);
	std::string side = lua_tostring(L, 1);
    std::lock_guard<std::mutex> lock(computer->peripherals_mutex);
	if (computer->peripherals.find(side) == computer->peripherals.end()) {
		lua_pushboolean(L, false);
		return 1;
	}
    if (std::string(computer->peripherals[side]->getMethods().name) == "drive")
        computer->peripherals[side]->call(L, "ejectDisk");
    else if (std::string(computer->peripherals[side]->getMethods().name) == "debugger")
		computer->peripherals[side]->call(L, "deinit");
	queueTask([ ](void* p)->void*{((peripheral*)p)->getDestructor()((peripheral*)p); return NULL;}, computer->peripherals[side]);
	computer->peripherals.erase(side);
	lua_pushboolean(L, true);
    std::string * sidearg = new std::string(side);
    termQueueProvider(computer, peripheral_detach, sidearg);
	return 1;
}

int periphemu_names(lua_State *L) {
    lua_newtable(L);
	lua_pushinteger(L, 1);
	lua_pushstring(L, "debugger");
	lua_settable(L, -3);
    int i = 2;
    for (auto entry : initializers) {
        lua_pushinteger(L, i++);
        lua_pushstring(L, entry.first.c_str());
        lua_settable(L, -3);
    }
    return 1;
}

const char* periphemu_keys[3] = {
	"create",
	"remove",
    "names",
};

lua_CFunction periphemu_values[3] = {
	periphemu_create,
	periphemu_remove,
    periphemu_names,
};

library_t periphemu_lib = { "periphemu", 3, periphemu_keys, periphemu_values, nullptr, nullptr };