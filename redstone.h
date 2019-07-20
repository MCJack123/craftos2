#include "lib.h"
static int rs_getSides(lua_State *L) {lua_newtable(L); return 1;}
static int rs_false(lua_State *L) {lua_pushboolean(L, 0); return 1;}
static int rs_0(lua_State *L) {lua_pushinteger(L, 0); return 1;}
static int rs_none(lua_State *L) {return 0;}
static const char * rs_keys[11] = {
    "getSides",
    "getInput",
    "setOutput",
    "getOutput",
    "getAnalogInput",
    "setAnalogOutput",
    "getAnalogOutput",
    "getBundledInput",
    "getBundledOutput",
    "setBundledOutput",
    "testBundledInput"
};
static lua_CFunction rs_values[11] = {
    rs_getSides,
    rs_false,
    rs_none,
    rs_false,
    rs_0,
    rs_none,
    rs_0,
    rs_0,
    rs_0,
    rs_none,
    rs_false
};
static library_t rs_lib = {"redstone", 11, rs_keys, rs_values};