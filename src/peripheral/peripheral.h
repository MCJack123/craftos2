/*
 * peripheral.h
 * CraftOS-PC 2
 * 
 * This file defines the peripheral API, and creates the base class for
 * peripherals to inherit.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#ifndef PERIPHERAL_H
#define PERIPHERAL_H
#ifdef __cplusplus
extern "C" {
#endif
#include "../lib.h"
extern library_t peripheral_lib;
extern void peripheral_update();

#ifdef __cplusplus
}
class peripheral {
public:
    peripheral() {} // unused
    peripheral(lua_State *L, const char * side) {}
    virtual ~peripheral(){
        
    }
    virtual int call(lua_State *L, const char * method)=0;
    virtual void update()=0;
    virtual library_t getMethods()=0;
};
#endif
#endif