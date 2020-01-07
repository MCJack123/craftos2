/*
 * peripheral/peripheral.hpp
 * CraftOS-PC 2
 * 
 * This file defines the peripheral API, and creates the base class for
 * peripherals to inherit.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

class peripheral;
#ifndef PERIPHERAL_HPP
#define PERIPHERAL_HPP
#include "../lib.hpp"
typedef peripheral*(*peripheral_init)(lua_State*, const char *);
extern library_t peripheral_lib;
extern void peripheral_update(Computer *comp);

class peripheral {
public:
    typedef void(*destructor)(peripheral*);
    peripheral() {} // unused
    peripheral(lua_State *L, const char * side) {}
    virtual ~peripheral()=0;
    virtual destructor getDestructor()=0;
    virtual int call(lua_State *L, const char * method)=0;
    virtual void update()=0;
    virtual library_t getMethods()=0;
};
#endif