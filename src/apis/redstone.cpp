/*
 * apis/redstone.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the redstone/rs API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#include "../util.hpp"

static std::vector<std::string> sides = {
    "bottom",
    "top",
    "back",
    "front",
    "right",
    "left"
};

static int rs_getSides(lua_State *L) {
    lua_createtable(L, 6, 0); 
    for (int i = 0; i < 6; i++) {
        lua_pushinteger(L, i + 1);
        lua_pushstring(L, sides[i].c_str());
        lua_settable(L, -3);
    }
    return 1;
}

// I'd normally use the standard luaL_check* functions, but because of how the redstone API
// uses backwards error messages, and errors about numbers not being strings, I'm forced to
// do all of the type checking manually.

static int rs_getInput(lua_State *L) {
    Computer * comp = get_comp(L);
    if (lua_type(L, 1) != LUA_TSTRING) return luaL_error(L, "bad argument #1 (string expected, got %s)", luaL_typename(L, 1));
    std::string sidestr = lua_tostring(L, 1);
    std::transform(sidestr.begin(), sidestr.end(), sidestr.begin(), [](unsigned char c) {return std::tolower(c); });
    int side = -1;
    for (int i = 0; i < 6; i++) {
        if (sides[i] == sidestr) {
            side = i;
            break;
        }
    }
    if (side == -1) return luaL_error(L, "bad argument #1 (unknown option %s)", sidestr.c_str());
    lua_pushboolean(L, comp->redstoneInputs[side]);
    return 1;
}

static int rs_getOutput(lua_State *L) {
    Computer * comp = get_comp(L);
    if (lua_type(L, 1) != LUA_TSTRING) return luaL_error(L, "bad argument #1 (string expected, got %s)", luaL_typename(L, 1));
    std::string sidestr = lua_tostring(L, 1);
    std::transform(sidestr.begin(), sidestr.end(), sidestr.begin(), [](unsigned char c) {return std::tolower(c); });
    int side = -1;
    for (int i = 0; i < 6; i++) {
        if (sides[i] == sidestr) {
            side = i;
            break;
        }
    }
    if (side == -1) return luaL_error(L, "bad argument #1 (unknown option %s)", sidestr.c_str());
    lua_pushboolean(L, comp->redstoneOutputs[side]);
    return 1;
}

static int rs_setOutput(lua_State *L) {
    Computer * comp = get_comp(L);
    if (lua_type(L, 1) != LUA_TSTRING) return luaL_error(L, "bad argument #1 (string expected, got %s)", luaL_typename(L, 1));
    std::string sidestr = lua_tostring(L, 1);
    std::transform(sidestr.begin(), sidestr.end(), sidestr.begin(), [](unsigned char c) {return std::tolower(c); });
    int side = -1;
    for (int i = 0; i < 6; i++) {
        if (sides[i] == sidestr) {
            side = i;
            break;
        }
    }
    if (side == -1) return luaL_error(L, "bad argument #1 (unknown option %s)", sidestr.c_str());
    comp->redstoneOutputs[side] = lua_toboolean(L, 2) ? 15 : 0;
    return 0;
}

static int rs_getAnalogInput(lua_State *L) {
    Computer * comp = get_comp(L);
    if (lua_type(L, 1) != LUA_TSTRING) return luaL_error(L, "bad argument #1 (string expected, got %s)", luaL_typename(L, 1));
    std::string sidestr = lua_tostring(L, 1);
    std::transform(sidestr.begin(), sidestr.end(), sidestr.begin(), [](unsigned char c) {return std::tolower(c); });
    int side = -1;
    for (int i = 0; i < 6; i++) {
        if (sides[i] == sidestr) {
            side = i;
            break;
        }
    }
    if (side == -1) return luaL_error(L, "bad argument #1 (unknown option %s)", sidestr.c_str());
    lua_pushinteger(L, comp->redstoneInputs[side]);
    return 1;
}

static int rs_getAnalogOutput(lua_State *L) {
    Computer * comp = get_comp(L);
    if (lua_type(L, 1) != LUA_TSTRING) return luaL_error(L, "bad argument #1 (string expected, got %s)", luaL_typename(L, 1));
    std::string sidestr = lua_tostring(L, 1);
    std::transform(sidestr.begin(), sidestr.end(), sidestr.begin(), [](unsigned char c) {return std::tolower(c); });
    int side = -1;
    for (int i = 0; i < 6; i++) {
        if (sides[i] == sidestr) {
            side = i;
            break;
        }
    }
    if (side == -1) return luaL_error(L, "bad argument #1 (unknown option %s)", sidestr.c_str());
    lua_pushinteger(L, comp->redstoneOutputs[side]);
    return 1;
}

static int rs_setAnalogOutput(lua_State *L) {
    Computer * comp = get_comp(L);
    if (lua_type(L, 1) != LUA_TSTRING) return luaL_error(L, "bad argument #1 (string expected, got %s)", luaL_typename(L, 1));
    std::string sidestr = lua_tostring(L, 1);
    std::transform(sidestr.begin(), sidestr.end(), sidestr.begin(), [](unsigned char c) {return std::tolower(c); });
    int side = -1;
    for (int i = 0; i < 6; i++) {
        if (sides[i] == sidestr) {
            side = i;
            break;
        }
    }
    if (side == -1) return luaL_error(L, "bad argument #1 (unknown option %s)", sidestr.c_str());
    if (!lua_isnumber(L, 2)) return luaL_error(L, "bad argument #2 (number expected, got %s)", luaL_typename(L, 2));
    const lua_Number strength = lua_tonumber(L, 2);
    if (isnan(strength)) return luaL_argerror(L, 2, "number expected, got nan");
    if (isinf(strength)) return luaL_argerror(L, 2, "number expected, got inf");
    if (strength < 0 || strength > 15) return luaL_error(L, "Expected number in range 0-15");
    comp->redstoneOutputs[side] = (uint8_t)strength;
    return 0;
}

static int rs_getBundledInput(lua_State *L) {
    Computer * comp = get_comp(L);
    if (lua_type(L, 1) != LUA_TSTRING) return luaL_error(L, "bad argument #1 (string expected, got %s)", luaL_typename(L, 1));
    std::string sidestr = lua_tostring(L, 1);
    std::transform(sidestr.begin(), sidestr.end(), sidestr.begin(), [](unsigned char c) {return std::tolower(c); });
    int side = -1;
    for (int i = 0; i < 6; i++) {
        if (sides[i] == sidestr) {
            side = i;
            break;
        }
    }
    if (side == -1) return luaL_error(L, "bad argument #1 (unknown option %s)", sidestr.c_str());
    lua_pushinteger(L, comp->bundledRedstoneInputs[side]);
    return 1;
}

static int rs_getBundledOutput(lua_State *L) {
    Computer * comp = get_comp(L);
    if (lua_type(L, 1) != LUA_TSTRING) return luaL_error(L, "bad argument #1 (string expected, got %s)", luaL_typename(L, 1));
    std::string sidestr = lua_tostring(L, 1);
    std::transform(sidestr.begin(), sidestr.end(), sidestr.begin(), [](unsigned char c) {return std::tolower(c); });
    int side = -1;
    for (int i = 0; i < 6; i++) {
        if (sides[i] == sidestr) {
            side = i;
            break;
        }
    }
    if (side == -1) return luaL_error(L, "bad argument #1 (unknown option %s)", sidestr.c_str());
    lua_pushinteger(L, comp->bundledRedstoneOutputs[side]);
    return 1;
}

static int rs_setBundledOutput(lua_State *L) {
    Computer * comp = get_comp(L);
    if (lua_type(L, 1) != LUA_TSTRING) return luaL_error(L, "bad argument #1 (string expected, got %s)", luaL_typename(L, 1));
    std::string sidestr = lua_tostring(L, 1);
    std::transform(sidestr.begin(), sidestr.end(), sidestr.begin(), [](unsigned char c) {return std::tolower(c); });
    int side = -1;
    for (int i = 0; i < 6; i++) {
        if (sides[i] == sidestr) {
            side = i;
            break;
        }
    }
    if (side == -1) return luaL_error(L, "bad argument #1 (unknown option %s)", sidestr.c_str());
    if (!lua_isnumber(L, 2)) return luaL_error(L, "bad argument #2 (number expected, got %s)", luaL_typename(L, 2));
    const lua_Number strength = lua_tonumber(L, 2);
    if (isnan(strength)) return luaL_argerror(L, 2, "number expected, got nan");
    if (isinf(strength)) return luaL_argerror(L, 2, "number expected, got inf");
    if (strength < 0 || strength > 65535) return luaL_error(L, "Expected number in range 0-65535");
    comp->bundledRedstoneOutputs[side] = (uint16_t)strength;
    return 0;
}

static int rs_testBundledInput(lua_State *L) {
    Computer * comp = get_comp(L);
    if (lua_type(L, 1) != LUA_TSTRING) return luaL_error(L, "bad argument #1 (string expected, got %s)", luaL_typename(L, 1));
    std::string sidestr = lua_tostring(L, 1);
    std::transform(sidestr.begin(), sidestr.end(), sidestr.begin(), [](unsigned char c) {return std::tolower(c); });
    int side = -1;
    for (int i = 0; i < 6; i++) {
        if (sides[i] == sidestr) {
            side = i;
            break;
        }
    }
    if (side == -1) return luaL_error(L, "bad argument #1 (unknown option %s)", sidestr.c_str());
    if (!lua_isnumber(L, 2)) return luaL_error(L, "bad argument #2 (number expected, got %s)", luaL_typename(L, 2));
    const lua_Number mask = lua_tonumber(L, 2);
    if (isnan(mask)) return luaL_argerror(L, 2, "number expected, got nan");
    if (isinf(mask)) return luaL_argerror(L, 2, "number expected, got inf");
    if (mask < 0 || mask > 65535) return luaL_error(L, "Expected number in range 0-65535");
    lua_pushboolean(L, (comp->bundledRedstoneOutputs[side] & (uint16_t)mask) == (uint16_t)mask);
    return 1;
}

static luaL_reg rs_reg[] = {
    {"getSides", rs_getSides},
    {"getInput", rs_getInput},
    {"getOutput", rs_getOutput},
    {"setOutput", rs_setOutput},
    {"getAnalogInput", rs_getAnalogInput},
    {"getAnalogOutput", rs_getAnalogOutput},
    {"setAnalogOutput", rs_setAnalogOutput},
    {"getAnalogueInput", rs_getAnalogInput},
    {"getAnalogueOutput", rs_getAnalogOutput},
    {"setAnalogueOutput", rs_setAnalogOutput},
    {"getBundledInput", rs_getBundledInput},
    {"getBundledOutput", rs_getBundledOutput},
    {"setBundledOutput", rs_setBundledOutput},
    {"testBundledInput", rs_testBundledInput},
    {NULL, NULL}
};
library_t rs_lib = {"redstone", rs_reg, NULL, NULL};