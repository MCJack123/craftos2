/*
 * plugin_base.cpp
 * CraftOS-PC 2
 * 
 * This file can be used as a template for new plugins.
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

extern "C" {
#ifdef _WIN32
_declspec(dllexport)
#endif
// replace "myplugin" with the plugin name
int luaopen_myplugin(lua_State *L) {
    luaL_register(L, "myplugin", M);
    return 1;
}

#ifdef _WIN32
_declspec(dllexport)
#endif
PluginInfo plugin_init(PluginFunctions * func, const path_t& path) {
    PluginInfo info;
    info.apiName = "myplugin"; // replace "myplugin" with the API name
    // configure any other information, or save the functions here...
    return info;
}
}