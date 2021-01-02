/*
 * peripheral.hpp
 * CraftOS-PC 2
 * 
 * This file creates the base class for all peripherals to inherit.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#ifndef CRAFTOS_PC_PERIPHERAL_HPP
#define CRAFTOS_PC_PERIPHERAL_HPP
#include "lib.hpp"

class peripheral;
// This function type is used to create a new instance of a peripheral. It takes
// the Lua state and side that should be passed to the constructor, and returns
// a new peripheral pointer object. This function template is the one that is
// passed to the registerPeripheral function. 
typedef peripheral*(*peripheral_init)(lua_State*, const char *);

// This class is the main interface class that is overridden when making custom
// peripherals. To create a peripheral type, just inherit this class as public
// and define a constructor, getDestructor, call, and getMethods.
class peripheral {
public:
    typedef void(*destructor)(peripheral*);
    peripheral() = default; // unused
    // This is the main constructor that is used to create a peripheral.
    // You must provide your own version of this constructor.
    peripheral(lua_State *L, const char * side) {}
    // You also should define a destructor for your peripheral.
    virtual ~peripheral()=0;
    // This function should return a function that takes a peripheral pointer
    // and correctly frees the object with delete. This is necessary when loading
    // a peripheral class from a plugin.
    // An example of this would be to define a static void function in the class:
    //   static void deinit(peripheral * p) {delete (myperipheral*)p;}
    // then return that from getDestructor:
    //   destructor getDestructor() {return deinit;}
    virtual destructor getDestructor() const=0;
    // This is the main function that is used to call methods on peripherals.
    // The function should act like a normal Lua function, except that it also
    // gets the method name as a parameter. If you're defining the function calls
    // separately in the class, you just need to convert the method name string
    // into the proper function calls: 
    //   if (m == "a") return a(L); else if (m == "b") return b(L); ...
    virtual int call(lua_State *L, const char * method)=0;
    // This function is called every render tick on the render thread. This can
    // be used for anything that requires a constant update cycle.
    virtual void update() {}
    // This function should return a library_t containing the names of all of the
    // methods available to the peripheral. Only the keys, name, and size members
    // are accessed, so there is no need to fill in the other members.
    virtual library_t getMethods() const=0;
    // This function is called whenever the computer reboots with the peripheral
    // still attached from a previous boot. This can be used to fix any Lua state
    // references that were destroyed when the previous Lua state was closed.
    // The state this function is called with is the computer's global state.
    virtual void reinitialize(lua_State *L) {}
};
inline peripheral::~peripheral() {}
#endif