#include "lib.h"

static int bit_band(lua_State *L) {
    unsigned int a = lua_tointeger(L, 1);
    unsigned int b = lua_tointeger(L, 2);
    lua_pushinteger(L, a & b);
    return 1;
}

static int bit_bor(lua_State *L) {
    unsigned int a = lua_tointeger(L, 1);
    unsigned int b = lua_tointeger(L, 2);
    lua_pushinteger(L, a | b);
    return 1;
}

static int bit_bnot(lua_State *L) {
    unsigned int a = lua_tointeger(L, 1);
    lua_pushinteger(L, ~a & 0xFFFFFFFF);
    return 1;
}

static int bit_bxor(lua_State *L) {
    unsigned int a = lua_tointeger(L, 1);
    unsigned int b = lua_tointeger(L, 2);
    lua_pushinteger(L, a ^ b);
    return 1;
}

static int bit_blshift(lua_State *L) {
    unsigned int a = lua_tointeger(L, 1);
    unsigned int b = lua_tointeger(L, 2);
    lua_pushinteger(L, a << b);
    return 1;
}

static int bit_brshift(lua_State *L) {
    unsigned int a = lua_tointeger(L, 1);
    unsigned int b = lua_tointeger(L, 2);
    lua_pushinteger(L, a >> b | ((((a & 0x80000000) << b) - 1) << (32 - b)));
    return 1;
}

static int bit_blogic_rshift(lua_State *L) {
    unsigned int a = lua_tointeger(L, 1);
    unsigned int b = lua_tointeger(L, 2);
    lua_pushinteger(L, a >> b);
    return 1;
}

static const char * bit_keys[7] = {
    "band",
    "bor",
    "bnot",
    "bxor",
    "blshift",
    "brshift",
    "blogic_rshift"
};

static lua_CFunction bit_values[7] = {
    bit_band,
    bit_bor,
    bit_bnot,
    bit_bxor,
    bit_blshift,
    bit_brshift,
    bit_blogic_rshift
};

static library_t bit_lib = {"bit", 7, bit_keys, bit_values};