/*
 * mounter.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the mounter API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#define CRAFTOSPC_INTERNAL
#include "../fs_standalone.hpp"
#include "../runtime.hpp"
#include "../platform.hpp"
#include "../terminal/SDLTerminal.hpp"
#include <string>
#include <vector>
#include <list>
#include <sstream>
#include <map>
#include <tuple>
#include <algorithm>
#include <sys/stat.h>
#include <cassert>
#ifdef WIN32
#include <dirent.h>
#define PATH_SEP L"\\"
#else
#include <libgen.h>
#define PATH_SEP "/"
#endif

extern std::string script_file;

static int mounter_mount(lua_State *L) {
    if (config.mount_mode == MOUNT_MODE_NONE) luaL_error(L, "Mounting is disabled");
    bool read_only = config.mount_mode != MOUNT_MODE_RW;
    if (lua_isboolean(L, 3) && config.mount_mode != MOUNT_MODE_RO_STRICT) read_only = lua_toboolean(L, 3);
    lua_pushboolean(L, addMount(get_comp(L), wstr(luaL_checkstring(L, 2)), luaL_checkstring(L, 1), read_only));
    return 1;
}

static int mounter_unmount(lua_State *L) {
    if (config.mount_mode == MOUNT_MODE_NONE) luaL_error(L, "Mounting is disabled");
    Computer * computer = get_comp(L);
    const char * comp_path = luaL_checkstring(L, 1);
    std::vector<std::string> elems = split(comp_path, '/');
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") { 
            if (pathc.size() < 1) luaL_error(L, "Not a directory");
            else pathc.pop_back();
        } 
        else if (s != "." && s != "") pathc.push_back(s);
    }
    if (pathc.front() == "rom" && config.romReadOnly) {
        lua_pushboolean(L, false);
        return 1;
    }
    bool found = false;
    for (auto it = computer->mounts.begin(); it != computer->mounts.end(); it++) {
        if (pathc.size() == std::get<0>(*it).size() && std::equal(std::get<0>(*it).begin(), std::get<0>(*it).end(), pathc.begin())) {
            it = computer->mounts.erase(it);
            if (it == computer->mounts.end()) break;
            found = true;
        }
    }
    lua_pushboolean(L, found);
    return 1;
}

static int mounter_list(lua_State *L) {
    Computer * computer = get_comp(L);
    lua_newtable(L); // table
    for (auto m : computer->mounts) {
        std::stringstream ss;
        for (std::string s : std::get<0>(m)) ss << (ss.tellp() == 0 ? "" : "/") << s;
        lua_pushstring(L, ss.str().c_str()); // table, key
        lua_gettable(L, -2); // table, value
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1); // table
            lua_newtable(L); // table, entries
        }
        lua_pushinteger(L, lua_objlen(L, -1) + 1); // table, entries, index
        lua_pushstring(L, astr(std::get<1>(m)).c_str()); // table, entries, index, value
        lua_settable(L, -3); // table, entries
        lua_pushstring(L, ss.str().c_str()); // table, entries, key
        lua_pushvalue(L, -2); // table, entries, key, entries
        lua_remove(L, -3); // table, key, entries
        lua_settable(L, -3); // table
    }
    return 1;
}

static int mounter_isReadOnly(lua_State *L) {
    Computer * computer = get_comp(L);
    const char * comp_path = luaL_checkstring(L, 1);
    std::vector<std::string> elems = split(comp_path, '/');
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") {
            if (pathc.size() < 1) luaL_error(L, "Not a directory");
            else pathc.pop_back();
        } else if (s != "." && s != "") pathc.push_back(s);
    }
    for (auto it = computer->mounts.begin(); it != computer->mounts.end(); it++) {
        if (std::get<0>(*it).size() == pathc.size() && std::equal(std::get<0>(*it).begin(), std::get<0>(*it).end(), pathc.begin())) {
            lua_pushboolean(L, std::get<2>(*it));
            return 1;
        }
    }
    luaL_error(L, "%s: Not mounted", comp_path);
    return 0; // redundant
}

extern "C" FILE* mounter_fopen(lua_State *L, const char * filename, const char * mode) {
    if (!((mode[0] == 'r' || mode[0] == 'w' || mode[0] == 'a') && (mode[1] == '\0' || mode[1] == 'b' || mode[1] == '+') && (mode[1] == '\0' || mode[2] == '\0' || mode[2] == 'b' || mode[2] == '+') && (mode[1] == '\0' || mode[2] == '\0' || mode[3] == '\0'))) 
        luaL_error(L, "Unsupported mode");
    if (get_comp(L)->files_open >= config.maximumFilesOpen) { errno = EMFILE; return NULL; }
    struct_stat st;
    path_t newpath = mode[0] == 'r' ? fixpath(get_comp(L), lua_tostring(L, 1), true) : fixpath_mkdir(get_comp(L), lua_tostring(L, 1));
    if ((mode[0] == 'w' || mode[0] == 'a' || (mode[0] == 'r' && (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')))) && fixpath_ro(get_comp(L), filename)) 
        { errno = EACCES; return NULL; }
    if (platform_stat(newpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) { errno = EISDIR; return NULL; }
    FILE* retval;
    if (mode[1] == 'b' && mode[2] == '+') retval = platform_fopen(newpath.c_str(), std::string(mode).substr(0, 2).c_str());
    else if (mode[1] == '+') {
        std::string mstr = mode;
        mstr.erase(mstr.begin() + 1);
        retval = platform_fopen(newpath.c_str(), mstr.c_str());
    } else retval = platform_fopen(newpath.c_str(), mode);
    if (retval != NULL) get_comp(L)->files_open++;
    return retval;
}

extern "C" int mounter_fclose(lua_State *L, FILE * stream) {
    int retval = fclose(stream);
    if (retval == 0 && get_comp(L)->files_open > 0) get_comp(L)->files_open--;
    return retval;
}

static luaL_Reg mounter_reg[] = {
    {"mount", mounter_mount},
    {"unmount", mounter_unmount},
    {"list", mounter_list},
    {"isReadOnly", mounter_isReadOnly},
    {NULL, NULL}
};

library_t mounter_lib = {"mounter", mounter_reg, nullptr, nullptr};