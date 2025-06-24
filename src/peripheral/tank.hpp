// Copyright (c) 2019-2024 JackMacWindows.
// SPDX-FileCopyrightText: 2019-2024 JackMacWindows
//
// SPDX-License-Identifier: MIT

#ifndef PERIPHERAL_TANK_HPP
#define PERIPHERAL_TANK_HPP
#include "../util.hpp"
#include <generic_peripheral/fluid_storage.hpp>

class tank: public fluid_storage {
    int tankCount = 1;
    int tankCapacity = 256;
    std::vector<std::unordered_map<std::string, int>> fluids;
    std::vector<std::string> types = {"tank", "fluid_storage"};
protected:
    int tanks(lua_State *L) override;
    int addFluid(const std::string& fluid, int amount) override;
    std::list<std::pair<std::string, int>> removeFluid(const std::string& fluid, int amount) override;
public:
    static library_t methods;
    static peripheral * init(lua_State *L, const char * side) {return new tank(L, side);}
    static void deinit(peripheral * p) {delete (tank*)p;}
    destructor getDestructor() const override {return deinit;}
    library_t getMethods() const override {return methods;}
    std::vector<std::string> getTypes() const override {return types;}
    tank(lua_State *L, const char * side);
    ~tank();
    int call(lua_State *L, const char * method) override;
};

#endif