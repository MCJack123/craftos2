/*
 * os.hpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the os API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#ifndef OS_HPP
#define OS_HPP
#include "lib.hpp"
extern library_t os_lib;
extern int getNextEvent(lua_State* L, const char* filter);
extern void* queueTask(void*(*func)(void*), void* arg);
#endif