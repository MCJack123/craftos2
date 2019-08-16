/*
 * os.h
 * CraftOS-PC 2
 * 
 * This file defines the methods for the os API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#ifndef OS_H
#define OS_H
#include "lib.h"
#ifdef __cplusplus
extern "C" {
#endif
extern library_t os_lib;
extern int getNextEvent(lua_State* L, const char* filter);
extern void queueEvent(const char* name, lua_State* param);
extern int running;
#ifdef __cplusplus
}
#endif
#endif