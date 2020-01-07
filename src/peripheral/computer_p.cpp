/*
 * peripheral/computer.cpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the computer peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include "computer.hpp"
#include "../os.hpp"
#include <regex>
#include <unordered_set>
#include <cstring>

extern std::unordered_set<Computer*> freedComputers;

int computer::turnOn(lua_State *L) {return 0;}

int computer::isOn(lua_State *L) {
    lua_pushboolean(L, freedComputers.find(comp) == freedComputers.end());
    return 1;
}

int computer::getID(lua_State *L) {
    if (freedComputers.find(comp) != freedComputers.end()) return 0;
    lua_pushinteger(L, comp->id);
    return 1;
}

int computer::shutdown(lua_State *L) {
    if (freedComputers.find(comp) != freedComputers.end()) return 0;
    comp->running = 0;
    return 0;
}

int computer::reboot(lua_State *L) {
    if (freedComputers.find(comp) != freedComputers.end()) return 0;
    comp->running = 2;
    return 0;
}

computer::computer(lua_State *L, const char * side) {
    if (std::string(side).substr(0, 9) != "computer_" || !std::all_of(side + 9, side + strlen(side), ::isdigit)) 
        throw std::invalid_argument("\"side\" parameter must be in the form of computer_[0-9]+");
    int id = atoi(&side[9]);
    comp = NULL;
    for (Computer * c : computers) if (c->id == id) comp = c;
    if (comp == NULL) comp = (Computer*)queueTask([ ](void* arg)->void*{return startComputer(*(int*)arg);}, &id);
    thiscomp = get_comp(L);
    comp->referencers.push_back(thiscomp);
}

computer::~computer() {
    if (thiscomp->peripherals_mutex.try_lock()) thiscomp->peripherals_mutex.unlock();
    else return;
    for (auto it = comp->referencers.begin(); it != comp->referencers.end(); it++) {
        if (*it == thiscomp) {
            it = comp->referencers.erase(it);
            if (it == comp->referencers.end()) break;
        }
    }
}

int computer::call(lua_State *L, const char * method) {
    std::string m(method);
    if (m == "turnOn") return turnOn(L);
    else if (m == "shutdown") return shutdown(L);
    else if (m == "reboot") return reboot(L);
    else if (m == "getID") return getID(L);
    else if (m == "isOn") return isOn(L);
    else return 0;
}

const char * computer_keys[5] = {
    "turnOn",
    "shutdown",
    "reboot",
    "getID",
    "isOn"
};

library_t computer::methods = {"computer", 5, computer_keys, NULL, nullptr, nullptr};