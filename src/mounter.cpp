/*
 * mounter.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the mounter API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "mounter.hpp"
#include "platform.hpp"
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
#define PATH_SEP "\\"
#else
#include <libgen.h>
#define PATH_SEP "/"
#endif

extern std::string script_file;

std::vector<std::string> split(std::string strToSplit, char delimeter) {
    std::stringstream ss(strToSplit);
    std::string item;
    std::vector<std::string> splittedStrings;
    while (std::getline(ss, item, delimeter)) {
        splittedStrings.push_back(item);
    }
    return splittedStrings;
}

char * fixpath(Computer *comp, const char * path, bool addExt) {
    std::vector<std::string> elems = split(path, '/');
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") { if (pathc.size() < 1) return NULL; else pathc.pop_back(); } 
        else if (s != "." && s != "") pathc.push_back(s);
    }
    if (script_file != "" && addExt && pathc.size() == 1 && pathc.front() == "startup.lua") {
        char * retval = (char*)malloc(script_file.size() + 1);
        strcpy(retval, script_file.c_str());
        return retval;
    }
    std::stringstream ss;
    if (addExt) {
        std::pair<size_t, std::string> max_path = std::make_pair(0, std::string(getBasePath()) + PATH_SEP + "computer" + PATH_SEP + std::to_string(comp->id));
        for (auto it = comp->mounts.begin(); it != comp->mounts.end(); it++)
            if (pathc.size() >= std::get<0>(*it).size() && std::get<0>(*it).size() > max_path.first && std::equal(std::get<0>(*it).begin(), std::get<0>(*it).end(), pathc.begin()))
                max_path = std::make_pair(std::get<0>(*it).size(), std::get<1>(*it));
        for (int i = 0; i < max_path.first; i++) pathc.pop_front();
        ss << max_path.second;
        for (std::string s : pathc) ss << PATH_SEP << s;
    } else for (std::string s : pathc) ss << (ss.tellp() == 0 ? "" : "/") << s;
    std::string retstr = ss.str();
    char * retval = (char*)malloc(retstr.size() + 1);
    strcpy(retval, retstr.c_str());
    //if (addExt) printf("%s\n", retval);
    return retval;
}

bool fixpath_ro(Computer *comp, const char * path) {
    std::vector<std::string> elems = split(path, '/');
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") { if (pathc.size() < 1) return false; else pathc.pop_back(); } 
        else if (s != "." && s != "") pathc.push_back(s);
    }
    std::pair<size_t, bool> max_path = std::make_pair(0, false);
    for (auto it = comp->mounts.begin(); it != comp->mounts.end(); it++)
        if (pathc.size() >= std::get<0>(*it).size() && std::get<0>(*it).size() > max_path.first && std::equal(std::get<0>(*it).begin(), std::get<0>(*it).end(), pathc.begin()))
            max_path = std::make_pair(std::get<0>(*it).size(), std::get<2>(*it));
    return max_path.second;
}

bool addMount(Computer *comp, const char * real_path, const char * comp_path, bool read_only) {
    struct stat sb;
    if (stat(real_path, &sb) != 0 || !S_ISDIR(sb.st_mode)) return false;
    std::vector<std::string> elems = split(comp_path, '/');
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") { if (pathc.size() < 1) return false; else pathc.pop_back(); }
        else if (s != "." && s != "") pathc.push_back(s);
    }
    if (pathc.size() == 0 || (pathc.front() == "rom" && !comp->mounter_initializing)) return false;
    for (auto it = comp->mounts.begin(); it != comp->mounts.end(); it++)
        if (pathc.size() == std::get<0>(*it).size() && std::equal(std::get<0>(*it).begin(), std::get<0>(*it).end(), pathc.begin()))
            return std::get<1>(*it) == std::string(real_path);
    comp->mounts.push_back(std::make_tuple(std::list<std::string>(pathc), std::string(real_path), read_only));
    return true;
}

void injectMounts(lua_State *L, const char * comp_path, int idx) {
    Computer * computer = get_comp(L);
    std::vector<std::string> elems = split(comp_path, '/');
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") { if (pathc.size() < 1) return; else pathc.pop_back(); } 
        else if (s != "." && s != "") pathc.push_back(s);
    }
    for (auto it = computer->mounts.begin(); it != computer->mounts.end(); it++) {
        if (pathc.size() + 1 == std::get<0>(*it).size() && std::equal(pathc.begin(), pathc.end(), std::get<0>(*it).begin())) {
            lua_pushinteger(L, ++idx);
            lua_pushstring(L, std::get<0>(*it).back().c_str());
            lua_settable(L, -3);
        }
    }
}

int mounter_mount(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    bool read_only = false;
    if (lua_isboolean(L, 3)) read_only = lua_toboolean(L, 3);
    lua_pushboolean(L, addMount(get_comp(L), lua_tostring(L, 2), lua_tostring(L, 1), read_only));
    return 1;
}

int mounter_unmount(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
    const char * comp_path = lua_tostring(L, 1);
    std::vector<std::string> elems = split(comp_path, '/');
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") { 
            if (pathc.size() < 1) {
                lua_pushstring(L, "Not a directory");
                lua_error(L);
            } else pathc.pop_back();
        } 
        else if (s != "." && s != "") pathc.push_back(s);
    }
    if (pathc.front() == "rom") {
        lua_pushboolean(L, false);
        return 1;
    }
    for (auto it = computer->mounts.begin(); it != computer->mounts.end(); it++) {
        if (pathc.size() == std::get<0>(*it).size() && std::equal(std::get<0>(*it).begin(), std::get<0>(*it).end(), pathc.begin())) {
            computer->mounts.erase(it);
            lua_pushboolean(L, true);
            return 1;
        }
    }
    lua_pushboolean(L, false);
    return 1;
}

int mounter_list(lua_State *L) {
    Computer * computer = get_comp(L);
    lua_newtable(L);
    for (auto m : computer->mounts) {
        std::stringstream ss;
        for (std::string s : std::get<0>(m)) ss << (ss.tellp() == 0 ? "" : "/") << s;
        lua_pushstring(L, ss.str().c_str());
        lua_pushstring(L, std::get<1>(m).c_str());
        lua_settable(L, -3);
    }
    return 1;
}

int mounter_isReadOnly(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
    const char * comp_path = lua_tostring(L, 1);
    std::vector<std::string> elems = split(comp_path, '/');
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") {
            if (pathc.size() < 1) {
                lua_pushstring(L, "Not a directory");
                lua_error(L);
            } else pathc.pop_back(); 
        } else if (s != "." && s != "") pathc.push_back(s);
    }
    for (auto it = computer->mounts.begin(); it != computer->mounts.end(); it++) {
        if (std::get<0>(*it).size() == pathc.size() && std::equal(std::get<0>(*it).begin(), std::get<0>(*it).end(), pathc.begin())) {
            lua_pushboolean(L, std::get<2>(*it));
            return 1;
        }
    }
    lua_pushfstring(L, "%s: Not mounted", comp_path);
    lua_error(L);
    return 0; // redundant
}

extern "C" FILE* mounter_fopen(lua_State *L, const char * filename, const char * mode) {
    if (get_comp(L)->files_open >= config.maximumFilesOpen) { errno = EMFILE; return NULL; }
    char * newpath = fixpath(get_comp(L), filename);
    if (strcmp(mode, "w") == 0 || strcmp(mode, "a") == 0) {
        char * dname = new char[strlen(newpath) + 1];
        strcpy(dname, newpath);
        const char * ddname = dirname(dname);
        createDirectory(ddname);
        delete[] dname;
    }
    FILE* retval = fopen(newpath, mode);
    free(newpath);
    if (retval != NULL) get_comp(L)->files_open++;
    return retval;
}

extern "C" int mounter_fclose(lua_State *L, FILE * stream) {
    int retval = fclose(stream);
    if (retval == 0 && get_comp(L)->files_open > 0) get_comp(L)->files_open--;
    return retval;
}

const char * mounter_keys[4] = {
    "mount",
    "unmount",
    "list",
    "isReadOnly"
};

lua_CFunction mounter_values[4] = {
    mounter_mount,
    mounter_unmount,
    mounter_list,
    mounter_isReadOnly
};

library_t mounter_lib = {"mounter", 4, mounter_keys, mounter_values, NULL, NULL};