/*
 * generic_peripheral/energy_storage.hpp
 * CraftOS-PC 2
 * 
 * This file defines a generic peripheral class for energy-storing peripherals
 * to inherit from.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#ifndef CRAFTOS_PC_GENERIC_PERIPHERAL_ENERGY_STORAGE_HPP
#define CRAFTOS_PC_GENERIC_PERIPHERAL_ENERGY_STORAGE_HPP
#include "../peripheral.hpp"

class energy_storage : public peripheral {
protected:
    virtual int getEnergy() = 0; // Returns the current energy of the peripheral.
    virtual int getEnergyCapacity() = 0; // Returns the total energy capacity of the peripheral.
public:
    virtual int call(lua_State *L, const char * method) override {
        const std::string m(method);
        if (m == "getEnergy") lua_pushinteger(L, getEnergy());
        else if (m == "getEnergyCapacity") lua_pushinteger(L, getEnergyCapacity());
        else return luaL_error(L, "No such method");
        return 1;
    }
    void update() override {}
    library_t getMethods() const override {
        static luaL_Reg reg[] = {
            {"getEnergy", NULL},
            {"getEnergyCapacity", NULL},
            {NULL, NULL}
        };
        static library_t methods = {"energy_storage", reg, nullptr, nullptr};
        return methods;
    }
};

#endif
