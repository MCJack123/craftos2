/*
 * platform.hpp
 * CraftOS-PC 2
 * 
 * This file defines functions that differ based on the platform the program is
 * built for.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef PLATFORM_HPP
#define PLATFORM_HPP
extern "C" {
#include <lua.h>
}
#include "Computer.hpp"
#include <thread>
extern void setThreadName(std::thread &t, std::string name);
extern int createDirectory(std::string path);
extern unsigned long long getFreeSpace(std::string path);
extern unsigned long long getCapacity(std::string path);
extern int removeDirectory(std::string path);
extern void setBasePath(const char * path);
extern void setROMPath(const char * path);
extern std::string getBasePath();
extern std::string getROMPath();
extern std::string getPlugInPath();
extern std::string getMCSavePath();
extern void updateNow(std::string tag_name);
extern void migrateData();
extern void copyImage(SDL_Surface* surf);
extern void setupCrashHandler();
#ifdef WIN32
extern char* basename(char* path);
extern char* dirname(char* path);
#endif
#endif