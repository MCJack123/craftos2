extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <CraftOS-PC.hpp>

class example_peripheral: public peripheral {
    int add(lua_State *L) {
        lua_pushnumber(L, luaL_checknumber(L, 1) + luaL_checknumber(L, 2));
        return 1;
    }
    int subtract(lua_State *L) {
        lua_pushnumber(L, luaL_checknumber(L, 1) - luaL_checknumber(L, 2));
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

static luaL_Reg methods_reg[] = {
    "add",
    "subtract",
    "ping",
};
static PluginInfo info;
library_t example_peripheral::methods = {"example_peripheral", methods_reg, nullptr, nullptr};

extern "C" {
#ifdef _WIN32
_declspec(dllexport)
#endif
PluginInfo * plugin_init(PluginFunctions * func, const path_t& path) {
    func->registerPeripheral("example_peripheral", &example_peripheral::init);
    return &info;
}
}
