/*
 * term.hpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the term API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#ifndef TERM_HPP
#define TERM_HPP
#include "lib.hpp"
typedef const char * (*event_provider)(lua_State *L, void* data);
extern library_t term_lib;
extern const char * termGetEvent(lua_State *L);
extern void* termRenderLoop(void*);
extern void termHook(lua_State *L, lua_Debug *ar);
extern void termQueueProvider(event_provider p, void* data);
#endif