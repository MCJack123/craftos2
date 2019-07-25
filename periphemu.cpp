#include "periphemu.h"
#include "peripheral/peripheral.h"
#include "peripheral/monitor.hpp"
#include "TerminalWindow.hpp"
#include <unordered_map>
#include <string>

extern std::unordered_map<std::string, peripheral*> peripherals;

extern "C" {

int periphemu_create(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    std::string side = lua_tostring(L, 1);
    std::string type = lua_tostring(L, 2);
    if (peripherals.find(side) != peripherals.end()) {
        lua_pushboolean(L, false);
        return 1;
    }
    lua_pop(L, 2);
    if (type == std::string("monitor")) peripherals[side] = new monitor(L, side.c_str());
    else {
        printf("not found: %s\n", type.c_str());
        lua_pushboolean(L, false);
        return 1;
    }
    lua_pushboolean(L, true);
    return 1;
}

int periphemu_remove(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::string side = lua_tostring(L, 1);
    if (peripherals.find(side) == peripherals.end()) {
        lua_pushboolean(L, false);
        return 1;
    }
    delete peripherals[side];
    peripherals.erase(side);
    lua_pushboolean(L, true);
    return 1;
}

}

const char * periphemu_keys[2] = {
    "create",
    "remove"
};

lua_CFunction periphemu_values[2] = {
    periphemu_create,
    periphemu_remove
};

library_t periphemu_lib = {"periphemu", 2, periphemu_keys, periphemu_values};