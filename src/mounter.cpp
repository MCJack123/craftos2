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
#include "fs_standalone.hpp"
#include "os.hpp"
#include "mounter.hpp"
#include "platform.hpp"
#include "terminal/SDLTerminal.hpp"
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

bool nothrow(std::function<void()> f) {try {f(); return true;} catch (std::exception &e) {return false;}}

std::string fixpath(Computer *comp, const char * path, bool exists, bool addExt, std::string * mountPath, bool getAllResults, bool * isRoot) {
    std::vector<std::string> elems = split(path, '/');
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") { if (pathc.size() < 1) return std::string(); else pathc.pop_back(); } 
        else if (s != "." && s != "") pathc.push_back(s);
    }
    while (pathc.size() > 0 && pathc.front().empty()) pathc.pop_front();
    if (comp->isDebugger && addExt && pathc.size() == 1 && pathc.front() == "bios.lua") 
#ifdef STANDALONE_ROM
        return ":bios.lua";
#else
        return getROMPath() + PATH_SEP + "bios.lua";
#endif
    std::stringstream ss;
    if (addExt) {
        std::pair<size_t, std::vector<std::string> > max_path = std::make_pair(0, std::vector<std::string>(1, comp->dataDir));
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
        if (isRoot != NULL) *isRoot = pathc.empty();
        if (exists) {
            bool found = false;
            for (std::string p : max_path.second) {
                std::stringstream sstmp;
                struct stat st;
                sstmp << p;
                for (std::string s : pathc) sstmp << PATH_SEP << s;
                if (
#ifdef STANDALONE_ROM
                (p == "rom:" && nothrow([&sstmp](){standaloneROM.path(sstmp.str());})) || (p == "debug:" && nothrow([&sstmp](){standaloneDebug.path(sstmp.str());})) ||
#endif
                (stat((sstmp.str()).c_str(), &st) == 0)) {
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
                if (
#ifdef STANDALONE_ROM
                (p == "rom:" && (nothrow([&sstmp, back](){standaloneROM.path(sstmp.str() + "/" + back);}) || (nothrow([&sstmp](){standaloneROM.path(sstmp.str());}) && standaloneROM.path(sstmp.str()).isDir))) || 
                (p == "debug:" && (nothrow([&sstmp, back](){standaloneDebug.path(sstmp.str() + "/" + back);}) || (nothrow([&sstmp](){standaloneDebug.path(sstmp.str());}) && standaloneDebug.path(sstmp.str()).isDir))) ||
#endif
                (stat((sstmp.str() + PATH_SEP + back).c_str(), &st) == 0) || (stat(sstmp.str().c_str(), &st) == 0 && S_ISDIR(st.st_mode))
                ) {
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
    if (
#ifdef STANDALONE_ROM
        (std::string(real_path) != "rom:" && std::string(real_path) != "debug:") &&
#endif
        (stat(real_path, &sb) != 0 || !S_ISDIR(sb.st_mode))
        ) return false;
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
    int selected = 1;
    if (!comp->mounter_initializing && config.showMountPrompt && dynamic_cast<SDLTerminal*>(comp->term) != NULL) {
        SDL_MessageBoxData data;
        data.flags = SDL_MESSAGEBOX_WARNING;
        data.window = dynamic_cast<SDLTerminal*>(comp->term)->win;
        data.title = "Mount requested";
        // see config.cpp:234 for why this is a pointer (TL;DR Windows is dumb)
        std::string * message = new std::string("A script is attempting to mount the REAL path " + std::string(real_path) + ". Any script will be able to read" + (read_only ? " " : " AND WRITE ") + "any files in this directory. Do you want to allow mounting this path?");
        data.message = message->c_str();
        data.numbuttons = 2;
        SDL_MessageBoxButtonData buttons[2];
        buttons[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
        buttons[0].buttonid = 0;
        buttons[0].text = "Deny";
        buttons[1].flags = 0;
        buttons[1].buttonid = 1;
        buttons[1].text = "Allow";
        data.buttons = buttons;
        data.colorScheme = NULL;
        queueTask([data](void*selected)->void*{SDL_ShowMessageBox(&data, (int*)selected); return NULL;}, &selected);
        delete message;
    }
    if (!selected) return false;
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
    if (config.mount_mode == MOUNT_MODE_NONE) luaL_error(L, "Mounting is disabled");
    bool read_only = config.mount_mode != MOUNT_MODE_RW;
    if (lua_isboolean(L, 3) && config.mount_mode != MOUNT_MODE_RO_STRICT) read_only = lua_toboolean(L, 3);
    lua_pushboolean(L, addMount(get_comp(L), lua_tostring(L, 2), lua_tostring(L, 1), read_only));
    return 1;
}

int mounter_unmount(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (config.mount_mode == MOUNT_MODE_NONE) luaL_error(L, "Mounting is disabled");
    Computer * computer = get_comp(L);
    const char * comp_path = lua_tostring(L, 1);
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

extern std::string fixpath_mkdir(Computer * comp, std::string path, bool md = true, std::string * mountPath = NULL);

extern "C" FILE* mounter_fopen(lua_State *L, const char * filename, const char * mode) {
    if (!((mode[0] == 'r' || mode[0] == 'w' || mode[0] == 'a') && (mode[1] == '\0' || mode[1] == 'b' || mode[1] == '+') && (mode[1] == '\0' || mode[2] == '\0' || mode[2] == 'b' || mode[2] == '+'))) 
        luaL_error(L, "Unsupported mode");
    if (get_comp(L)->files_open >= config.maximumFilesOpen) { errno = EMFILE; return NULL; }
    struct stat st;
    std::string newpath = mode[0] == 'r' ? fixpath(get_comp(L), lua_tostring(L, 1), true) : fixpath_mkdir(get_comp(L), lua_tostring(L, 1));
    if ((mode[0] == 'w' || mode[0] == 'a' || (mode[0] == 'r' && (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')))) && fixpath_ro(get_comp(L), filename)) 
        { errno = EACCES; return NULL; }
    if (stat(newpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) { errno = EISDIR; return NULL; }
    FILE* retval;
    if (mode[1] == 'b' && mode[2] == '+') retval = fopen(newpath.c_str(), std::string(mode).substr(0, 2).c_str());
    else if (mode[1] == '+') {
        std::string mstr = mode;
        mstr.erase(mstr.begin() + 1);
        retval = fopen(newpath.c_str(), mstr.c_str());
    } else retval = fopen(newpath.c_str(), mode);
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