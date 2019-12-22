/*
 * peripheral_base.cpp
 * CraftOS-PC 2
 * 
 * This file can be used as a template for new peripheral plugins.
 */

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

peripheral::~peripheral(){}

// Replace myperipheral with the name of your peripheral
class myperipheral: public peripheral {
    // Define your peripheral methods here
public:
    static library_t methods;
    // Replace myperipheral with the name of your peripheral
    myperipheral(lua_State *L, const char * side) {}
    ~myperipheral(){}
    static peripheral * init(lua_State *L, const char * side) {return new myperipheral(L, side);}
    static void deinit(peripheral * p) {delete (myperipheral*)p;}
    destructor getDestructor() {return deinit;}
    int call(lua_State *L, const char * method) {
        std::string m(method);
        // Check the value of m for each method:
        // if (m == "a") return a(L);
        // etc...
        // else return 0;
    }
    void update(){}
    library_t getMethods() {return methods;}
};

const char * methods_keys[] = {
    // Insert the method names here
};
// Replace myperipheral with the name of your peripheral
library_t myperipheral::methods = {"myperipheral", sizeof(methods_keys)/sizeof(const char*), methods_keys, NULL, nullptr, nullptr};

extern "C" {
#ifdef _WIN32
_declspec(dllexport)
#endif
int luaopen_example_peripheral(lua_State *L) {
    lua_pushnil(L);
    return 1;
}

int register_registerPeripheral(lua_State *L) {
    // Replace myperipheral with the name of your peripheral
    ((void(*)(std::string, peripheral_init))lua_touserdata(L, 1))("myperipheral", &myperipheral::init); 
    return 0;
}

#ifdef _WIN32
_declspec(dllexport)
#endif
int plugin_info(lua_State *L) {
    lua_newtable(L);
    lua_pushinteger(L, PLUGIN_VERSION);
    lua_setfield(L, -2, "version");
    lua_pushcfunction(L, register_registerPeripheral);
    lua_setfield(L, -2, "register_registerPeripheral");
    return 1;
}
}