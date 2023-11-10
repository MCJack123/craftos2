/*
 * generic_peripheral/fluid_storage.hpp
 * CraftOS-PC 2
 * 
 * This file defines a generic peripheral class for tank-like peripherals
 * to inherit from.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2023 JackMacWindows.
 */

#ifndef CRAFTOS_PC_GENERIC_PERIPHERAL_FLUID_STORAGE_HPP
#define CRAFTOS_PC_GENERIC_PERIPHERAL_FLUID_STORAGE_HPP
#include "../Computer.hpp"
#include "../peripheral.hpp"

class fluid_storage : public peripheral {
protected:
    virtual int tanks(lua_State *L) = 0; // Implements tanks() as a Lua function: https://tweaked.cc/generic_peripheral/fluid_storage.html#v:tanks
    virtual int addFluid(const std::string& fluid, int amount) = 0; // Adds the specified amount of fluid to a tank. Returns the amount actually added.
    virtual std::list<std::pair<std::string, int>> removeFluid(const std::string& fluid, int amount) = 0; // Removes the specified amount of fluid from a tank. If fluid is empty, remove any type of fluid; otherwise only remove that type. Returns a list of each fluid type & amount removed. (If no fluid is removed, return an empty list.)
public:
    virtual int call(lua_State *L, const char * method) override {
        const std::string m(method);
        if (m == "tanks") return tanks(L);
        else if (m == "pushFluid" || m == "pullFluid") {
            Computer * comp = get_comp(L);
            const char * side = luaL_checkstring(L, 1);
            const int limit = luaL_optinteger(L, 2, INT_MAX);
            const std::string name = luaL_optstring(L, 3, "");

            if (comp->peripherals.find(side) == comp->peripherals.end()) return luaL_error(L, "Target '%s' does not exist", side);
            fluid_storage * p = dynamic_cast<fluid_storage*>(comp->peripherals[side]);
            if (p == NULL) return luaL_error(L, "Target '%s' is not an tank", side); // grammar mistake intentional
            fluid_storage *src, *dest;
            if (m == "pushFluid") src = this, dest = p;
            else src = p, dest = this;
            if (limit <= 0) return luaL_error(L, "Limit must be > 0");

            const auto removed = src->removeFluid(name, limit);
            if (removed.empty()) {
                lua_pushinteger(L, 0);
                return 1;
            }
            int added = 0;
            std::list<std::pair<std::string, int>> returnedFluid;
            for (const std::pair<std::string, int>& type : removed) {
                const int add = dest->addFluid(type.first, type.second);
                added += add;
                if (add < type.second) returnedFluid.push_back(std::make_pair(type.first, type.second - add));
            }
            for (const std::pair<std::string, int>& type : returnedFluid) src->addFluid(type.first, type.second);

            lua_pushinteger(L, added);
            return 1;
        } else return luaL_error(L, "No such method");
    }
    void update() override {}
    library_t getMethods() const override {
        static luaL_Reg reg[] = {
            {"tanks", NULL},
            {"pushFluid", NULL},
            {"pullFluid", NULL},
            {NULL, NULL}
        };
        static library_t methods = {"fluid_storage", reg, nullptr, nullptr};
        return methods;
    }
};

#endif
