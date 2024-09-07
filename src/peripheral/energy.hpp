// Copyright (c) 2019-2024 JackMacWindows.
// SPDX-FileCopyrightText: 2019-2024 JackMacWindows
//
// SPDX-License-Identifier: MIT

#ifndef PERIPHERAL_ENERGY_HPP
#define PERIPHERAL_ENERGY_HPP
#include "../util.hpp"
#include <generic_peripheral/energy_storage.hpp>

class energy: public energy_storage {
    int currentEnergy = 0;
    int maxEnergy = 1'000'000'000;
    std::vector<std::string> types = {"energy", "energy_storage"};
    int setEnergy(lua_State *L);
protected:
    int getEnergy() override;
    int getEnergyCapacity() override;
public:
    static library_t methods;
    static peripheral * init(lua_State *L, const char * side) {return new energy(L, side);}
    static void deinit(peripheral * p) {delete (energy*)p;}
    destructor getDestructor() const override {return deinit;}
    library_t getMethods() const override {return methods;}
    std::vector<std::string> getTypes() const override {return types;}
    energy(lua_State *L, const char * side);
    ~energy();
    int call(lua_State *L, const char * method) override;
};

#endif