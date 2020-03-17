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

std::string fixpath(Computer *comp, const char * path, bool exists, bool addExt, std::string * mountPath, bool getAllResults) {
    std::vector<std::string> elems = split(path, '/');
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") { if (pathc.size() < 1) return std::string(); else pathc.pop_back(); } 
        else if (s != "." && s != "") pathc.push_back(s);
    }
    if (comp->isDebugger && addExt && pathc.size() == 1 && pathc.front() == "bios.lua") return getROMPath() + "/bios.lua";
    std::stringstream ss;
    if (addExt) {
        std::pair<size_t, std::vector<std::string> > max_path = std::make_pair(0, std::vector<std::string>(1, getBasePath() + PATH_SEP + "computer" + PATH_SEP + std::to_string(comp->id)));
        std::list<std::string> * mount_list = NULL;
        for (auto it = comp->mounts.begin(); it != comp->mounts.end(); it++) {
            if (pathc.size() >= std::get<0>(*it).size() && std::equal(std::get<0>(*it).begin(), std::get<0>(*it).end(), pathc.begin())) {
                if (std::get<0>(*it).size() > max_path.first) {
                    max_path = std::make_pair(std::get<0>(*it).size(), std::vector<std::string>(1, std::get<1>(*it)));
                    mount_list = &std::get<0>(*it);
                } else if (std::get<0>(*it).size() == max_path.first) {
                    max_path.second.push_back(std::get<1>(*it));
                }
            }
        }
        for (size_t i = 0; i < max_path.first; i++) pathc.pop_front();
        if (exists) {
            bool found = false;
            for (std::string p : max_path.second) {
                std::stringstream sstmp;
                struct stat st;
                sstmp << p;
                for (std::string s : pathc) sstmp << PATH_SEP << s;
                if ((stat((sstmp.str()).c_str(), &st) == 0)) {
                    if (getAllResults && found) ss << "\n";
                    ss << sstmp.str();
                    found = true;
                    if (!getAllResults) break;
                }
            }
            if (!found) return std::string();
        } else if (pathc.size() > 1) {
            bool found = false;
            std::string back = pathc.back();
            pathc.pop_back();
            for (std::string p : max_path.second) {
                std::stringstream sstmp;
                struct stat st;
                sstmp << p;
                for (std::string s : pathc) sstmp << PATH_SEP << s;
                if ((stat((sstmp.str() + PATH_SEP + back).c_str(), &st) == 0) || (!exists && stat(sstmp.str().c_str(), &st) == 0 && S_ISDIR(st.st_mode))) {
                    if (getAllResults && found) ss << "\n";
                    ss << sstmp.str() << PATH_SEP << back;
                    found = true;
                    if (!getAllResults) break;
                }
            }
            if (!found) return std::string();
        } else {
            ss << max_path.second.front();
            for (std::string s : pathc) ss << PATH_SEP << s;
        }
        if (mountPath != NULL) {
            if (mount_list == NULL) *mountPath = "hdd";
            else {
                std::stringstream ss2;
                for (auto it = mount_list->begin(); it != mount_list->end(); it++) {
                    if (it != mount_list->begin()) ss2 << "/";
                    ss2 << *it;
                }
                *mountPath = ss2.str();
            }
        }
    } else for (std::string s : pathc) ss << (ss.tellp() == 0 ? "" : "/") << s;
    return ss.str();
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
    if (pathc.front() == "rom" && !comp->mounter_initializing && config.romReadOnly) return false;
    /*for (auto it = comp->mounts.begin(); it != comp->mounts.end(); it++)
        if (pathc.size() == std::get<0>(*it).size() && std::equal(std::get<0>(*it).begin(), std::get<0>(*it).end(), pathc.begin()))
            return std::get<1>(*it) == std::string(real_path);*/
    comp->mounts.push_back(std::make_tuple(std::list<std::string>(pathc), std::string(real_path), read_only));
    return true;
}

std::set<std::string> getMounts(Computer * computer, const char * comp_path) {
    std::vector<std::string> elems = split(comp_path, '/');
    std::list<std::string> pathc;
    std::set<std::string> retval;
    for (std::string s : elems) {
        if (s == "..") { if (pathc.size() < 1) return retval; else pathc.pop_back(); } 
        else if (s != "." && s != "") pathc.push_back(s);
    }
    for (auto it = computer->mounts.begin(); it != computer->mounts.end(); it++)
        if (pathc.size() + 1 == std::get<0>(*it).size() && std::equal(pathc.begin(), pathc.end(), std::get<0>(*it).begin()))
            retval.insert(std::get<0>(*it).back());
    return retval;
}

int mounter_mount(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    if (config.mount_mode == MOUNT_MODE_NONE) {lua_pushstring(L, "Mounting is disabled"); lua_error(L);}
    bool read_only = config.mount_mode != MOUNT_MODE_RW;
    if (lua_isboolean(L, 3) && config.mount_mode != MOUNT_MODE_RO_STRICT) read_only = lua_toboolean(L, 3);
    lua_pushboolean(L, addMount(get_comp(L), lua_tostring(L, 2), lua_tostring(L, 1), read_only));
    return 1;
}

int mounter_unmount(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (config.mount_mode == MOUNT_MODE_NONE) {lua_pushstring(L, "Mounting is disabled"); lua_error(L);}
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

int mounter_list(lua_State *L) {
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
        lua_pushstring(L, std::get<1>(m).c_str()); // table, entries, index, value
        lua_settable(L, -3); // table, entries
        lua_pushstring(L, ss.str().c_str()); // table, entries, key
        lua_pushvalue(L, -2); // table, entries, key, entries
        lua_remove(L, -3); // table, key, entries
        lua_settable(L, -3); // table
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
    std::string newpath = fixpath(get_comp(L), filename, mode[0] == 'r');
    if (strcmp(mode, "w") == 0 || strcmp(mode, "a") == 0) createDirectory(newpath.substr(0, newpath.find_last_of('/')));
    FILE* retval = fopen(newpath.c_str(), mode);
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

library_t mounter_lib = {"mounter", 4, mounter_keys, mounter_values, nullptr, nullptr};