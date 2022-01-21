/*
 * plugin_base.cpp
 * CraftOS-PC 2
 * 
 * This file can be used as a template for new plugins.
 *
 * This code is released in the public domain.
 */

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <CraftOS-PC.hpp>

// add your functions here...

static luaL_reg M[] = {
    // add functions here as {name, function}...
    {NULL, NULL}
};

// replace "myplugin" with the plugin name as used in `luaopen_*` below
static PluginInfo info("myplugin");

extern "C" {
// replace "myplugin" with the plugin name
DLLEXPORT int luaopen_myplugin(lua_State *L) {
    luaL_register(L, "myplugin", M);
    return 1;
}

DLLEXPORT PluginInfo * plugin_init(PluginFunctions * func, const path_t& path) {
    // configure any other information, or save the functions here...
    return &info;
}
}