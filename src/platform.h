/*
 * platform.h
 * CraftOS-PC 2
 * 
 * This file defines functions that differ based on the platform the program is
 * built for.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#ifndef PLATFORM_H
#define PLATFORM_H
#ifdef __cplusplus
extern "C" {
#endif
#include <lua.h>
extern void * createThread(void*(*func)(void*), void* arg);
extern void joinThread(void * thread);
extern int createDirectory(const char * path);
extern void msleep(unsigned long time);
extern unsigned long long getFreeSpace(char* path);
extern void platform_fs_find(lua_State* L, char* wildcard);
extern int removeDirectory(char* path);
extern void pushHostString(lua_State *L);
extern const char * bios_path;
extern const char * getBasePath();
extern const char * getROMPath();
extern void platformFree();
extern void platformInit();
#ifdef WIN32
extern char* basename(char* path);
extern char* dirname(char* path);
#endif
#ifdef __cplusplus
}
#endif
#endif