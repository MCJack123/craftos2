extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "../src/Computer.hpp"

#define PLUGIN_VERSION 2

void bad_argument(lua_State *L, const char * type, int pos) {
    lua_pushfstring(L, "bad argument #%d (expected %s, got %s)", pos, type, lua_typename(L, lua_type(L, pos)));
    lua_error(L);
}

peripheral::~peripheral(){}

class example_peripheral: public peripheral {
    int add(lua_State *L) {
        if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
        if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
        lua_pushnumber(L, lua_tonumber(L, 1) + lua_tonumber(L, 2));
        return 1;
    }
    int subtract(lua_State *L) {
        if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
        if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
        lua_pushnumber(L, lua_tonumber(L, 1) - lua_tonumber(L, 2));
        return 1;
    }
    int ping(lua_State *L) {
        lua_pushstring(L, "pong");
        return 1;
    }
public:
    static library_t methods;
    example_peripheral(lua_State *L, const char * side) {}
    ~example_peripheral(){}
    int call(lua_State *L, const char * method) {
        std::string m(method);
        if (m == "add") return add(L);
        else if (m == "subtract") return subtract(L);
        else if (m == "ping") return ping(L);
        else return 0;
    }
    static peripheral * init(lua_State *L, const char * side) {return new example_peripheral(L, side);}
    static void deinit(peripheral * p) {delete (example_peripheral*)p;}
    destructor getDestructor() {return deinit;}
    void update(){}
    library_t getMethods() {return methods;}
};

const char * methods_keys[] = {
    "add",
    "subtract",
    "ping",
};
library_t example_peripheral::methods = {"example_peripheral", 3, methods_keys, NULL, nullptr, nullptr};

extern "C" {
#ifdef _WIN32
_declspec(dllexport)
#endif
int luaopen_example_peripheral(lua_State *L) {
    lua_pushnil(L);
    return 1;
}

int register_registerPeripheral(lua_State *L) {
    ((void(*)(std::string, peripheral_init))lua_touserdata(L, 1))("example_peripheral", &example_peripheral::init); 
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