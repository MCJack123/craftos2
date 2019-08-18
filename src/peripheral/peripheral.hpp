/*
 * peripheral/peripheral.hpp
 * CraftOS-PC 2
 * 
 * This file defines the peripheral API, and creates the base class for
 * peripherals to inherit.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

class peripheral;
#ifndef PERIPHERAL_HPP
#define PERIPHERAL_HPP
#include "../lib.hpp"
extern library_t peripheral_lib;
extern void peripheral_update(Computer *comp);

class peripheral {
public:
    peripheral() {} // unused
    peripheral(lua_State *L, const char * side) {}
    virtual ~peripheral(){}
    virtual int call(lua_State *L, const char * method)=0;
    virtual void update()=0;
    virtual library_t getMethods()=0;
};
#endif