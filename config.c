#include "config.h"
#include <string.h>

struct configuration config = {
    true,
    false,
    "",
    false,
    false,
    false,
    1000000,
    128,
    17000,
    8,
    20
};

int config_get(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * name = lua_tostring(L, 1);
    if (strcmp(name, "http_enable") == 0)
        lua_pushboolean(L, config.http_enable);
    else if (strcmp(name, "disable_lua51_features") == 0)
        lua_pushboolean(L, config.disable_lua51_features);
    else if (strcmp(name, "default_computer_settings") == 0)
        lua_pushstring(L, config.default_computer_settings);
    else if (strcmp(name, "logPeripheralErrors") == 0)
        lua_pushboolean(L, config.logPeripheralErrors);
    else if (strcmp(name, "computerSpaceLimit") == 0)
        lua_pushinteger(L, config.computerSpaceLimit);
    else if (strcmp(name, "maximumFilesOpen") == 0)
        lua_pushinteger(L, config.maximumFilesOpen);
    else if (strcmp(name, "maxNotesPerTick") == 0)
        lua_pushinteger(L, config.maxNotesPerTick);
    else if (strcmp(name, "clockSpeed") == 0)
        lua_pushinteger(L, config.clockSpeed);
    /*else if (strcmp(name, "http_whitelist") == 0)
        lua_push(L, config.http_whitelist);
    else if (strcmp(name, "http_blacklist") == 0)
        lua_push(L, config.http_blacklist);*/
    else if (strcmp(name, "showFPS") == 0)
        lua_pushboolean(L, config.showFPS);
    else if (strcmp(name, "readFail") == 0)
        lua_pushboolean(L, config.readFail);
    else if (strcmp(name, "abortTimeout") == 0)
        lua_pushinteger(L, config.abortTimeout);
    else return 0;
    return 1;
}

int config_set(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * name = lua_tostring(L, 1);
    if (strcmp(name, "http_enable") == 0)
        config.http_enable = lua_toboolean(L, 2);
    else if (strcmp(name, "disable_lua51_features") == 0)
        config.disable_lua51_features = lua_toboolean(L, 2);
    else if (strcmp(name, "default_computer_settings") == 0)
        config.default_computer_settings = lua_tostring(L, 2);
    else if (strcmp(name, "logPeripheralErrors") == 0)
        config.logPeripheralErrors = lua_toboolean(L, 2);
    else if (strcmp(name, "computerSpaceLimit") == 0)
        config.computerSpaceLimit = lua_tointeger(L, 2);
    else if (strcmp(name, "maximumFilesOpen") == 0)
        config.maximumFilesOpen = lua_tointeger(L, 2);
    else if (strcmp(name, "maxNotesPerTick") == 0)
        config.maxNotesPerTick = lua_tointeger(L, 2);
    else if (strcmp(name, "clockSpeed") == 0)
        config.clockSpeed = lua_tointeger(L, 2);
    /*else if (strcmp(name, "http_whitelist") == 0)
        config.http_whitelist = lua_to(L, 2);
    else if (strcmp(name, "http_blacklist") == 0)
        config.http_blacklist = lua_to(L, 2);*/
    else if (strcmp(name, "showFPS") == 0)
        config.showFPS = lua_tointeger(L, 2);
    else if (strcmp(name, "abortTimeout") == 0)
        config.abortTimeout = lua_tointeger(L, 2);
    return 0;
}

int config_list(lua_State *L) {
    lua_newtable(L);
    lua_pushnumber(L, 1);
    lua_pushstring(L, "http_enable");
    lua_settable(L, -3);

    lua_pushnumber(L, 2);
    lua_pushstring(L, "disable_lua51_features");
    lua_settable(L, -3);

    lua_pushnumber(L, 3);
    lua_pushstring(L, "default_computer_settings");
    lua_settable(L, -3);

    lua_pushnumber(L, 4);
    lua_pushstring(L, "logPeripheralErrors");
    lua_settable(L, -3);

    lua_pushnumber(L, 5);
    lua_pushstring(L, "computerSpaceLimit");
    lua_settable(L, -3);

    lua_pushnumber(L, 6);
    lua_pushstring(L, "maximumFilesOpen");
    lua_settable(L, -3);

    lua_pushnumber(L, 7);
    lua_pushstring(L, "maxNotesPerTick");
    lua_settable(L, -3);

    lua_pushnumber(L, 8);
    lua_pushstring(L, "clockSpeed");
    lua_settable(L, -3);

    lua_pushnumber(L, 9);
    lua_pushstring(L, "showFPS");
    lua_settable(L, -3);

    lua_pushnumber(L, 10);
    lua_pushstring(L, "readFail");
    lua_settable(L, -3);

    lua_pushnumber(L, 11);
    lua_pushstring(L, "abortTimeout");
    lua_settable(L, -3);
    return 1;
}

int config_getType(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * name = lua_tostring(L, 1);
    if (strcmp(name, "http_enable") == 0)
        lua_pushinteger(L, 0);
    else if (strcmp(name, "disable_lua51_features") == 0)
        lua_pushinteger(L, 0);
    else if (strcmp(name, "default_computer_settings") == 0)
        lua_pushinteger(L, 1);
    else if (strcmp(name, "logPeripheralErrors") == 0)
        lua_pushinteger(L, 0);
    else if (strcmp(name, "computerSpaceLimit") == 0)
        lua_pushinteger(L, 2);
    else if (strcmp(name, "maximumFilesOpen") == 0)
        lua_pushinteger(L, 2);
    else if (strcmp(name, "maxNotesPerTick") == 0)
        lua_pushinteger(L, 2);
    else if (strcmp(name, "clockSpeed") == 0)
        lua_pushinteger(L, 2);
    /*else if (strcmp(name, "http_whitelist") == 0)
        lua_pushinteger(L, 3);
    else if (strcmp(name, "http_blacklist") == 0)
        lua_pushinteger(L, 3);*/
    else if (strcmp(name, "showFPS") == 0)
        lua_pushinteger(L, 0);
    else if (strcmp(name, "abortTimeout") == 0)
        lua_pushinteger(L, 2);
    else lua_pushinteger(L, -1);
    return 1;
}

void config_init(void) {
    // todo
}

void config_deinit(void) {
    // todo
}

const char * config_keys[4] = {
    "get",
    "set",
    "list",
    "getType"
};

lua_CFunction config_values[4] = {
    config_get,
    config_set,
    config_list,
    config_getType
};

library_t config_lib = {"config", 4, config_keys, config_values, config_init, config_deinit};