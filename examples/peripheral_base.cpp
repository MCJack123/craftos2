// SPDX-FileCopyrightText: 2019-2024 JackMacWindows
//
// SPDX-License-Identifier: MIT

/*
 * peripheral_base.cpp
 * CraftOS-PC 2
 * 
 * This file can be used as a template for new peripheral plugins.
 *
 * This code is released in the public domain.
 */

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <CraftOS-PC.hpp>

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
    destructor getDestructor() const override {return deinit;}
    int call(lua_State *L, const char * method) override {
        const std::string m(method);
        // Check the value of m for each method:
        // if (m == "a") return a(L);
        // etc...
        // else return luaL_error(L, "No such method");
    }
    void update() override {}
    library_t getMethods() const override {return methods;}
};

static luaL_Reg methods_reg[] = {
    // Insert the methods names here as {"func", NULL}...
    {NULL, NULL}
};
static PluginInfo info;
// Replace myperipheral with the name of your peripheral
library_t myperipheral::methods = {"myperipheral", methods_reg, nullptr, nullptr};

extern "C" {
DLLEXPORT PluginInfo * plugin_init(PluginFunctions * func, const path_t& path) {
    // Replace myperipheral with the name of your peripheral
    func->registerPeripheral("myperipheral", &myperipheral::init);
    return &info;
}
}