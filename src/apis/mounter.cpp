/*
 * apis/mounter.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the mounter API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#include <algorithm>
#include <sstream>
#include <sys/stat.h>
#include <FileEntry.hpp>
#include "../platform.hpp"
#include "../runtime.hpp"
#include "../terminal/SDLTerminal.hpp"
#ifdef WIN32
#include <dirent.h>
#else
#include <libgen.h>
#endif

#ifdef STANDALONE_ROM
extern FileEntry standaloneROM;
extern FileEntry standaloneDebug;
extern std::string standaloneBIOS;
#endif

static int mounter_mount(lua_State *L) {
    lastCFunction = __func__;
    if (config.mount_mode == MOUNT_MODE_NONE) luaL_error(L, "Mounting is disabled");
    bool read_only = config.mount_mode != MOUNT_MODE_RW;
    if (lua_isboolean(L, 3) && config.mount_mode != MOUNT_MODE_RO_STRICT) read_only = lua_toboolean(L, 3);
    lua_pushboolean(L, addMount(get_comp(L), wstr(luaL_checkstring(L, 2)), luaL_checkstring(L, 1), read_only));
    return 1;
}

static int mounter_unmount(lua_State *L) {
    lastCFunction = __func__;
    if (config.mount_mode == MOUNT_MODE_NONE) luaL_error(L, "Mounting is disabled");
    Computer * computer = get_comp(L);
    const char * comp_path = luaL_checkstring(L, 1);
    std::vector<std::string> elems = split(comp_path, "/\\");
    std::list<std::string> pathc;
    for (const std::string& s : elems) {
        if (s == "..") { 
            if (pathc.empty()) luaL_error(L, "Not a directory");
            else pathc.pop_back();
        } 
        else if (!s.empty() && !std::all_of(s.begin(), s.end(), [](const char c)->bool{return c == '.';})) pathc.push_back(s);
    }
    if (pathc.front() == "rom" && config.romReadOnly) {
        lua_pushboolean(L, false);
        return 1;
    }
    bool found = false;
    for (auto it = computer->mounts.begin(); it != computer->mounts.end(); ++it) {
        if (pathc.size() == std::get<0>(*it).size() && std::equal(std::get<0>(*it).begin(), std::get<0>(*it).end(), pathc.begin())) {
            it = computer->mounts.erase(it);
            found = true;
            if (it == computer->mounts.end()) break;
        }
    }
    lua_pushboolean(L, found);
    return 1;
}

static int mounter_list(lua_State *L) {
    lastCFunction = __func__;
    Computer * computer = get_comp(L);
    lua_createtable(L, 0, computer->mounts.size()); // table
    for (auto m : computer->mounts) {
        std::stringstream ss;
        for (const std::string& s : std::get<0>(m)) ss << (ss.tellp() == 0 ? "" : "/") << s;
        lua_pushstring(L, ss.str().c_str()); // table, key
        lua_gettable(L, -2); // table, value
        if (lua_isnil(L, -1)) {
            lua_pop(L, 1); // table
            lua_createtable(L, 1, 0); // table, entries
        }
        lua_pushinteger(L, lua_objlen(L, -1) + 1); // table, entries, index
        if (std::regex_match(std::get<1>(m), pathregex(WS("\\d+:")))) lua_pushfstring(L, "(virtual mount:%s)", std::get<1>(m).substr(0, std::get<1>(m).size()-1).c_str());
        else lua_pushstring(L, astr(std::get<1>(m)).c_str()); // table, entries, index, value
        lua_settable(L, -3); // table, entries
        lua_pushstring(L, ss.str().c_str()); // table, entries, key
        lua_pushvalue(L, -2); // table, entries, key, entries
        lua_remove(L, -3); // table, key, entries
        lua_settable(L, -3); // table
    }
    return 1;
}

static int mounter_isReadOnly(lua_State *L) {
    lastCFunction = __func__;
    Computer * computer = get_comp(L);
    const char * comp_path = luaL_checkstring(L, 1);
    std::vector<std::string> elems = split(comp_path, "/\\");
    std::list<std::string> pathc;
    for (const std::string& s : elems) {
        if (s == "..") {
            if (pathc.empty()) luaL_error(L, "Not a directory");
            else pathc.pop_back();
        } else if (!s.empty() && !std::all_of(s.begin(), s.end(), [](const char c)->bool{return c == '.';})) pathc.push_back(s);
    }
    for (const auto& e : computer->mounts) {
        if (std::get<0>(e).size() == pathc.size() && std::equal(std::get<0>(e).begin(), std::get<0>(e).end(), pathc.begin())) {
            lua_pushboolean(L, std::get<2>(e));
            return 1;
        }
    }
    luaL_error(L, "%s: Not mounted", comp_path);
    return 0; // redundant
}

static luaL_Reg mounter_reg[] = {
    {"mount", mounter_mount},
    {"unmount", mounter_unmount},
    {"list", mounter_list},
    {"isReadOnly", mounter_isReadOnly},
    {NULL, NULL}
};

library_t mounter_lib = {"mounter", mounter_reg, nullptr, nullptr};
