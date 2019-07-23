#include "peripheral.h"
#include <unordered_map>
#include <string>

std::unordered_map<std::string, peripheral*> peripherals;

library_t peripheral::methods;

extern "C" {

int peripheral_isPresent(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    lua_pushboolean(L, peripherals.find(std::string(lua_tostring(L, -1))) != peripherals.end());
    return 1;
}

int peripheral_getType(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::string side(lua_tostring(L, -1));
    if (peripherals.find(side) != peripherals.end())
        lua_pushstring(L, peripherals[side]->methods.name);
    else return 0;
    return 1;
}

int peripheral_getMethods(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::string side(lua_tostring(L, -1));
    if (peripherals.find(side) == peripherals.end()) return 0;
    library_t methods = peripherals[side]->methods;
    lua_newtable(L);
    for (int i = 0; i < methods.count; i++) {
        lua_pushnumber(L, i+1);
        lua_pushstring(L, methods.keys[i]);
        lua_settable(L, -3);
    }
    return 1;
}

int peripheral_call(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    std::string side(lua_tostring(L, -1));
    std::string func(lua_tostring(L, -2));
    if (peripherals.find(side) == peripherals.end()) return 0;
    lua_pop(L, 2);
    return peripherals[side]->call(L, func.c_str());
}

extern "C" void peripheral_update() {for (auto p : peripherals) p.second->update();}

}

const char * peripheral_keys[4] = {
    "isPresent",
    "getType",
    "getMethods",
    "call"
};

lua_CFunction peripheral_values[4] = {
    peripheral_isPresent,
    peripheral_getType,
    peripheral_getMethods,
    peripheral_call
};

library_t peripheral_lib = {"peripheral", 4, peripheral_keys, peripheral_values};