/*
 * platform/android.cpp
 * CraftOS-PC 2
 * 
 * This file implements functions specific to Android.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#ifdef __ANDROID__ // disable error checking on Windows
extern "C" {
#include <lua.h>
}
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <dlfcn.h>
#include <libgen.h>
#include <pthread.h>
#include <SDL2/SDL_syswm.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <ucontext.h>
#include <unistd.h>
#include "../platform.hpp"
#include "../util.hpp"

std::string base_path;
std::string rom_path;

void setBasePath(const char * path) {
    base_path = path;
}

void setROMPath(const char * path) {
    rom_path = path;
}

std::string getBasePath() {
    if (base_path.empty()) base_path = std::string(SDL_AndroidGetInternalStoragePath()) + "/user-data/";
    return base_path;
}

std::string getROMPath() {
    if (rom_path.empty()) rom_path = std::string(SDL_AndroidGetInternalStoragePath()) + "/assets/";
    return rom_path;
}
std::string getPlugInPath() {
    if (rom_path.empty()) rom_path = std::string(SDL_AndroidGetInternalStoragePath()) + "/assets/";
    return rom_path + "/plugins/";
}

std::string getMCSavePath() {
    return "";
}

void setThreadName(std::thread &t, const std::string& name) {
    pthread_setname_np(t.native_handle(), name.c_str());
    printf("Set name of thread %lx to '%s'\n", t.native_handle(), name.c_str());
}

int createDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0 ) return !S_ISDIR(st.st_mode);
    if (mkdir(path.c_str(), 0777) != 0) {
        if (errno == ENOENT && path != "/" && !path.empty()) {
            if (createDirectory(path.substr(0, path.find_last_of('/')).c_str())) return 1;
            mkdir(path.c_str(), 0777);
        } else if (errno != EEXIST) return 1;
    }
    return 0;
}

int removeDirectory(const std::string& path) {
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

unsigned long long getFreeSpace(const std::string& path) {
    struct statvfs st;
    if (statvfs(path.c_str(), &st) != 0) {
        if (path.find_last_of("/") == std::string::npos || path.substr(0, path.find_last_of("/")-1).empty()) return 0;
        else return getFreeSpace(path.substr(0, path.find_last_of("/")-1));
    }
    return st.f_bavail * st.f_bsize;
}

unsigned long long getCapacity(const std::string& path) {
    struct statvfs st;
    if (statvfs(path.c_str(), &st) != 0) {
        if (path.find_last_of("/") == std::string::npos || path.substr(0, path.find_last_of("/")-1).empty()) return 0;
        else return getCapacity(path.substr(0, path.find_last_of("/")-1));
    }
    return st.f_blocks * st.f_frsize;
}

void updateNow(const std::string& tag_name) {
    
}

int recursiveCopyPlatform(const std::string& fromDir, const std::string& toDir) {
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
                    r = recursiveCopyPlatform(fromDir + "/" + std::string(p->d_name), toDir + "/" + std::string(p->d_name));
                }
                closedir(d);
            }
            if (!r) r = rmdir(fromDir.c_str());
            return r;
        } else return rename(fromDir.c_str(), toDir.c_str());
    } else return -1;
}

void migrateOldData() {
    
}

void copyImage(SDL_Surface* surf) {
    fprintf(stderr, "Warning: Android does not support taking screenshots to the clipboard.\n");
}

void setupCrashHandler() {

}

void setFloating(SDL_Window* win, bool state) {}

#ifdef __INTELLISENSE__
#region Mobile API
#endif

static int mobile_openKeyboard(lua_State *L) {
    if (lua_isnone(L, 1) || lua_toboolean(L, 1)) SDL_StartTextInput();
    else SDL_StopTextInput();
    return 0;
}

static int mobile_isKeyboardOpen(lua_State *L) {
    lua_pushboolean(L, SDL_IsTextInputActive());
    return 1;
}

static int mobile_sendNotification(lua_State *L) {
    const char * message = luaL_checkstring(L, 2);
    luaL_error(L, "Not implemented yet");
    return 1;
}

static luaL_Reg mobile_reg[] = {
    {"openKeyboard", mobile_openKeyboard},
    {"isKeyboardOpen", mobile_isKeyboardOpen},
    {"sendNotification", mobile_sendNotification},
    {NULL, NULL}
};

static luaL_Reg android_reg[] = {
    {NULL, NULL}
};

int mobile_luaopen(lua_State *L) {
    luaL_register(L, "mobile", mobile_reg);
    lua_pushstring(L, "android");
    lua_newtable(L);
    for (luaL_Reg* r = android_reg; r->name && r->func; r++) {
        lua_pushstring(L, r->name);
        lua_pushcfunction(L, r->func);
        lua_settable(L, -3);
    }
    lua_settable(L, -3);
    return 1;
}

#ifdef __INTELLISENSE__
#endregion
#endif

#endif // __INTELLISENSE__
