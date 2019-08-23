/*
 * platform.hpp
 * CraftOS-PC 2
 * 
 * This file defines functions that differ based on the platform the program is
 * built for.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#ifndef PLATFORM_HPP
#define PLATFORM_HPP
extern "C" {
#include <lua.h>
}
#include "Computer.hpp"
#include <thread>
extern void setThreadName(std::thread &t, const char * name);
extern int createDirectory(const char * path);
extern unsigned long long getFreeSpace(char* path);
extern int removeDirectory(char* path);
extern void pushHostString(lua_State *L);
extern const char * bios_path;
extern const char * getBasePath();
extern const char * getROMPath();
extern char * getBIOSPath();
extern void platformFree();
extern void platformInit(Computer * comp);
#ifdef WIN32
extern char* basename(char* path);
extern char* dirname(char* path);
#endif
#endif