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

#ifdef WIN32
typedef std::wstring path_t;
#define pathstream_t std::wstringstream
#define platform_stat _wstat
#define platform_access _waccess
extern FILE* platform_fopen(const wchar_t* path, const char * mode);
#define platform_remove _wremove
#define platform_rename _wrename
#define platform_opendir _wopendir
#define platform_readdir _wreaddir
#define platform_closedir _wclosedir
#define platform_DIR WDIR
#define struct_dirent struct _wdirent
#define struct_stat struct _stat
extern path_t wstr(std::string str);
extern std::string astr(path_t str);
#define to_path_t std::to_wstring
#define WS(s) L##s
#define pathcmp wcscmp

extern char* basename(char* path);
extern char* dirname(char* path);
#else
typedef std::string path_t;
#define pathstream_t std::stringstream
#define platform_stat stat
#define platform_access access
#define platform_fopen fopen
#define platform_remove remove
#define platform_rename rename
#define platform_opendir opendir
#define platform_readdir readdir
#define platform_closedir closedir
#define platform_DIR DIR
#define struct_dirent struct dirent
#define struct_stat struct stat
#define wstr(s) (s)
#define astr(s) (s)
#define to_path_t std::to_string
#define WS(s) s
#define pathcmp strcmp
#endif

extern void setThreadName(std::thread &t, std::string name);
extern int createDirectory(path_t path);
extern unsigned long long getFreeSpace(path_t path);
extern unsigned long long getCapacity(path_t path);
extern int removeDirectory(path_t path);
extern void setBasePath(const char * path);
extern void setROMPath(const char * path);
extern path_t getBasePath();
extern path_t getROMPath();
extern path_t getPlugInPath();
extern path_t getMCSavePath();
extern void updateNow(std::string tag_name);
extern void migrateData();
extern void copyImage(SDL_Surface* surf);
extern void setupCrashHandler();
#endif