extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "../src/Computer.hpp"

// change this to the latest version when compiling (see DOCUMENTATION.md for more details)
#define PLUGIN_VERSION 2

void bad_argument(lua_State *L, const char * type, int pos) {
    lua_pushfstring(L, "bad argument #%d (expected %s, got %s)", pos, type, lua_typename(L, lua_type(L, pos)));
    lua_error(L);
}

Computer * get_comp(lua_State *L) {
    lua_pushstring(L, "computer");
    lua_gettable(L, LUA_REGISTRYINDEX);
    void * retval = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return (Computer*)retval;
}

// add your functions here...

struct luaL_reg M[] = {
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
int plugin_info(lua_State *L) {
    lua_newtable(L);
    lua_pushinteger(L, PLUGIN_VERSION);
    lua_setfield(L, -2, "version");
    return 1;
}
}