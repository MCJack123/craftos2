/*
 * platform_linux.cpp
 * CraftOS-PC 2
 * 
 * This file implements functions specific to Linux.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#ifdef __linux__ // disable error checking on Windows
extern "C" {
#include <lua.h>
}
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#include <stdio.h>
#include <libgen.h>
#include <unistd.h>
//#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <glob.h>
#include <dirent.h>
#include <pthread.h>
#include <dlfcn.h>
#include <string>
#include <vector>
#include <sstream>
#include "mounter.hpp"
#include "platform.hpp"

#ifdef CUSTOM_ROM_DIR
const char * rom_path = CUSTOM_ROM_DIR;
std::string rom_path_expanded;
#else
const char * rom_path = "/usr/share/craftos";
#endif
#ifdef FS_ROOT
const char * base_path = "";
#else
const char * base_path = "$XDG_DATA_HOME/craftos-pc";
#endif
std::string base_path_expanded;

std::string getBasePath() {
    if (!base_path_expanded.empty()) return base_path_expanded;
    wordexp_t p;
    wordexp(base_path, &p, 0);
    base_path_expanded = p.we_wordv[0];
    for (unsigned i = 1; i < p.we_wordc; i++) base_path_expanded += p.we_wordv[i];
    wordfree(&p);
    if (base_path_expanded == "/craftos-pc") {
        wordexp("$HOME/.local/share/craftos-pc", &p, 0);
        base_path_expanded = p.we_wordv[0];
        for (unsigned i = 1; i < p.we_wordc; i++) base_path_expanded += p.we_wordv[i];
        wordfree(&p);
    }
    return base_path_expanded;
}

#ifdef CUSTOM_ROM_DIR
std::string getROMPath() {
    if (!rom_path_expanded.empty()) return rom_path_expanded;
    wordexp_t p;
    wordexp(rom_path, &p, 0);
    rom_path_expanded = p.we_wordv[0];
    for (unsigned i = 1; i < p.we_wordc; i++) rom_path_expanded += p.we_wordv[i];
    wordfree(&p);
    return rom_path_expanded;
}

std::string getPlugInPath() { return getROMPath() + "/plugins/"; }
#else
std::string getROMPath() { return rom_path; }
std::string getPlugInPath() { return std::string(rom_path) + "/plugins/"; }
#endif

void setThreadName(std::thread &t, std::string name) {
    pthread_setname_np(*(pthread_t*)t.native_handle(), name.c_str());
}

int createDirectory(std::string path) {
    if (mkdir(path.c_str(), 0777) != 0) {
        if (errno == ENOENT && path != "/") {
            if (createDirectory(path.substr(0, path.find_last_of('/')).c_str())) return 1;
            mkdir(path.c_str(), 0777);
        } else if (errno != EEXIST) return 1;
    }
    return 0;
}

int removeDirectory(std::string path) {
    struct stat statbuf;
    if (!stat(path.c_str(), &statbuf)) {
        if (S_ISDIR(statbuf.st_mode)) {
            DIR *d = opendir(path.c_str());
            int r = -1;
            if (d) {
                struct dirent *p;
                r = 0;
                while (!r && (p=readdir(d))) {
                    /* Skip the names "." and ".." as we don't want to recurse on them. */
                    if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;
                    r = removeDirectory(path + "/" + std::string(p->d_name));
                }
                closedir(d);
            }
            if (!r) r = rmdir(path.c_str());
            return r;
        } else return unlink(path.c_str());
    } else return -1;
}

unsigned long long getFreeSpace(std::string path) {
	struct statvfs st;
	if (statvfs(path.c_str(), &st) != 0) return 0;
	return st.f_bavail * st.f_bsize;
}

#if defined(__i386__) || defined(__i386) || defined(i386)
#define ARCHITECTURE "i386"
#elif defined(__amd64__) || defined(__amd64)
#define ARCHITECTURE "amd64"
#elif defined(__x86_64__) || defined(__x86_64)
#define ARCHITECTURE "x86_64"
#elif defined(__ia64__) || defined(__ia64) || defined(_IA64)
#define ARCHITECTURE "ia64"
#elif defined(__arm__)
#define ARCHITECTURE "armv7"
#elif defined(__aarch64__) || defined(__aarch64)
#define ARCHITECTURE "aarch64"
#elif defined(__arm64__) || defined(__arm64)
#define ARCHITECTURE "arm64"
#elif defined(__powerpc__) || defined(__powerpc) || defined(__ppc__)
#define ARCHITECTURE "powerpc"
#elif defined(__powerpc64__) || defined(__ppc64)
#define ARCHITECTURE "ppc64"
#else
#define ARCHITECTURE "unknown"
#endif

void pushHostString(lua_State *L) {
    struct utsname host;
    uname(&host);
    lua_pushfstring(L, "%s %s %s", host.sysname, ARCHITECTURE, host.release);
}

void updateNow(std::string tag_name) {
    
}

int recursiveCopy(std::string fromDir, std::string toDir) {
    struct stat statbuf;
    if (!stat(fromDir.c_str(), &statbuf)) {
        if (S_ISDIR(statbuf.st_mode)) {
            createDirectory(toDir);
            DIR *d = opendir(fromDir.c_str());
            int r = -1;
            if (d) {
                struct dirent *p;
                r = 0;
                while (!r && (p=readdir(d))) {
                    /* Skip the names "." and ".." as we don't want to recurse on them. */
                    if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;
                    r = recursiveCopy(fromDir + "/" + std::string(p->d_name), toDir + "/" + std::string(p->d_name));
                }
                closedir(d);
            }
            if (!r) r = rmdir(fromDir.c_str());
            return r;
        } else return rename(fromDir.c_str(), toDir.c_str());
    } else return -1;
}

void migrateData() {
    wordexp_t p;
    struct stat st;
    wordexp("$HOME/.craftos", &p, 0);
    std::string oldpath = p.we_wordv[0];
    for (unsigned i = 1; i < p.we_wordc; i++) oldpath += p.we_wordv[i];
    wordfree(&p);
    if (stat(oldpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && stat(getBasePath().c_str(), &st) != 0) 
        recursiveCopy(oldpath, getBasePath());
}

std::unordered_map<std::string, void*> dylibs;

void * loadSymbol(std::string path, std::string symbol) {
    void * handle;
    if (dylibs.find(path) == dylibs.end()) dylibs[path] = dlopen(path.c_str(), RTLD_LAZY);
    handle = dylibs[path];
    return dlsym(handle, symbol.c_str());
}

void unloadLibraries() {
    for (auto lib : dylibs) dlclose(lib.second);
}

#endif // __INTELLISENSE__