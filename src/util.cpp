/*
 * util.cpp
 * CraftOS-PC 2
 * 
 * This file implements some commonly-used functions.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#include <sstream>
#include <Computer.hpp>
#include <dirent.h>
#include <Poco/Base64Decoder.h>
#include <Poco/Base64Encoder.h>
#include <sys/stat.h>
#include <FileEntry.hpp>
#include "platform.hpp"
#include "runtime.hpp"
#include "terminal/SDLTerminal.hpp"
#include "util.hpp"
#ifdef WIN32
#define PATH_SEP L"\\"
#define PATH_SEPC '\\'
#else
#include <libgen.h>
#define PATH_SEP "/"
#define PATH_SEPC '/'
#endif

#ifdef STANDALONE_ROM
extern FileEntry standaloneROM;
extern FileEntry standaloneDebug;
extern std::string standaloneBIOS;
#endif

const char * lastCFunction = "(none!)";
char computer_key = 'C';
void* getCompCache_glob = NULL;
Computer * getCompCache_comp = NULL;

Computer * _get_comp(lua_State *L) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, 1);
    void * retval = lua_touserdata(L, -1);
    lua_pop(L, 1);
    getCompCache_glob = *(void**)(((ptrdiff_t)L) + sizeof(void*)*3 + 3 + alignof(void*) - ((sizeof(void*)*3 + 3) % alignof(void*)));
    getCompCache_comp = (Computer*)retval;
    return (Computer*)retval;
}

void load_library(Computer *comp, lua_State *L, const library_t& lib) {
    luaL_register(L, lib.name, lib.functions);
    if (lib.init != NULL) lib.init(comp);
}

std::string b64encode(const std::string& orig) {
    std::stringstream ss;
    Poco::Base64Encoder enc(ss);
    enc.write(orig.c_str(), orig.size());
    enc.close();
    return ss.str();
}

std::string b64decode(const std::string& orig) {
    std::stringstream ss;
    std::stringstream out(orig);
    Poco::Base64Decoder dec(out);
    std::copy(std::istreambuf_iterator<char>(dec), std::istreambuf_iterator<char>(), std::ostreambuf_iterator<char>(ss));
    return ss.str();
}

std::vector<std::string> split(const std::string& strToSplit, char delimeter) {
    std::stringstream ss(strToSplit);
    std::string item;
    std::vector<std::string> splittedStrings;
    while (std::getline(ss, item, delimeter)) {
        splittedStrings.push_back(item);
    }
    return splittedStrings;
}

std::vector<std::wstring> split(const std::wstring& strToSplit, wchar_t delimeter) {
    std::wstringstream ss(strToSplit);
    std::wstring item;
    std::vector<std::wstring> splittedStrings;
    while (std::getline(ss, item, delimeter)) {
        splittedStrings.push_back(item);
    }
    return splittedStrings;
}

static std::string concat(const std::list<std::string> &c, char sep) {
    std::stringstream ss;
    bool started = false;
    for (const std::string& s : c) {
        if (started) ss << sep;
        ss << s;
        started = true;
    }
    return ss.str();
}

static std::list<std::string> split_list(const std::string& strToSplit, char delimeter) {
    std::stringstream ss(strToSplit);
    std::string item;
    std::list<std::string> splittedStrings;
    while (std::getline(ss, item, delimeter)) {
        splittedStrings.push_back(item);
    }
    return splittedStrings;
}

path_t fixpath_mkdir(Computer * comp, const std::string& path, bool md, std::string * mountPath) {
    if (md && fixpath_ro(comp, path.c_str())) return path_t();
    std::list<std::string> components = path.find('/') != path_t::npos ? split_list(path, '/') : split_list(path, '\\');
    while (!components.empty() && components.front().empty()) components.pop_front();
    if (components.empty()) return fixpath(comp, "", true);
    components.pop_back();
    std::list<std::string> append;
    path_t maxPath = fixpath(comp, concat(components, '/').c_str(), false, true, mountPath);
    while (maxPath.empty()) {
        append.push_front(components.back());
        components.pop_back();
        if (components.empty()) return path_t();
        maxPath = fixpath(comp, concat(components, '/').c_str(), false, true, mountPath);
    }
    if (!md) return maxPath;
    if (createDirectory(maxPath + PATH_SEP + wstr(concat(append, PATH_SEPC))) != 0) return path_t();
    return fixpath(comp, path.c_str(), false, true, mountPath);
}

static bool _nothrow(std::function<void()> f) { try { f(); return true; } catch (...) { return false; } }
#define nothrow(expr) _nothrow([&](){ expr ;})

inline bool isVFSPath(path_t path) {
    if (!std::isdigit(path[0])) return false;
    for (const auto& c : path) {
        if (c == ':') return true;
        else if (!std::isdigit(c)) return false;
    }
    return false;
}

path_t fixpath(Computer *comp, const char * path, bool exists, bool addExt, std::string * mountPath, bool getAllResults, bool * isRoot) {
    std::vector<std::string> elems = split(path, '/');
    std::list<std::string> pathc;
    for (const std::string& s : elems) {
        if (s == "..") {
            if (pathc.empty() && addExt) return path_t();
            else if (pathc.empty()) pathc.push_back("..");
            else pathc.pop_back();
        } else if (s != "." && !s.empty()) pathc.push_back(s);
    }
    while (!pathc.empty() && pathc.front().empty()) pathc.pop_front();
    if (comp->isDebugger && addExt && pathc.size() == 1 && pathc.front() == "bios.lua")
#ifdef STANDALONE_ROM
        return WS(":bios.lua");
#else
        return getROMPath() + PATH_SEP + WS("bios.lua");
#endif
    pathstream_t ss;
    if (addExt) {
        std::pair<size_t, std::vector<path_t> > max_path = std::make_pair(0, std::vector<path_t>(1, comp->dataDir));
        std::list<std::string> * mount_list = NULL;
        for (auto& m : comp->mounts) {
            std::list<std::string> &pathlist = std::get<0>(m);
            if (pathc.size() >= pathlist.size() && std::equal(pathlist.begin(), pathlist.end(), pathc.begin())) {
                if (pathlist.size() > max_path.first) {
                    max_path = std::make_pair(pathlist.size(), std::vector<path_t>(1, std::get<1>(m)));
                    mount_list = &pathlist;
                } else if (pathlist.size() == max_path.first) {
                    max_path.second.push_back(std::get<1>(m));
                }
            }
        }
        for (size_t i = 0; i < max_path.first; i++) pathc.pop_front();
        if (isRoot != NULL) *isRoot = pathc.empty();
        if (exists) {
            bool found = false;
            for (const path_t& p : max_path.second) {
                pathstream_t sstmp;
                struct_stat st;
                sstmp << p;
                for (const std::string& s : pathc) sstmp << PATH_SEP << wstr(s);
                if (
                    (isVFSPath(p) && nothrow(comp->virtualMounts[(unsigned)std::stoul(p.substr(0, p.size()-1))]->path(sstmp.str()))) ||
                    (platform_stat(sstmp.str().c_str(), &st) == 0)) {
                    if (getAllResults && found) ss << "\n";
                    ss << sstmp.str();
                    found = true;
                    if (!getAllResults) break;
                }
            }
            if (!found) return path_t();
        } else if (pathc.size() > 1) {
            bool found = false;
            std::string back = pathc.back();
            pathc.pop_back();
            for (const path_t& p : max_path.second) {
                pathstream_t sstmp;
                struct_stat st;
                sstmp << p;
                for (const std::string& s : pathc) sstmp << PATH_SEP << wstr(s);
                if (
                    (isVFSPath(p) && (nothrow(comp->virtualMounts[(unsigned)std::stoul(p.substr(0, p.size()-1))]->path(sstmp.str() + WS("/") + wstr(back))) || (nothrow(comp->virtualMounts[(unsigned)std::stoul(p.substr(0, p.size()-1))]->path(sstmp.str())) && comp->virtualMounts[(unsigned)std::stoul(p.substr(0, p.size()-1))]->path(sstmp.str()).isDir))) ||
                    (platform_stat((sstmp.str() + PATH_SEP + wstr(back)).c_str(), &st) == 0) || (platform_stat(sstmp.str().c_str(), &st) == 0 && S_ISDIR(st.st_mode))
                    ) {
                    if (getAllResults && found) ss << "\n";
                    ss << sstmp.str() << PATH_SEP << wstr(back);
                    found = true;
                    if (!getAllResults) break;
                }
            }
            if (!found) return path_t();
        } else {
            ss << max_path.second.front();
            for (const std::string& s : pathc) ss << PATH_SEP << wstr(s);
        }
        if (mountPath != NULL) {
            if (mount_list == NULL) *mountPath = "hdd";
            else {
                std::stringstream ss2;
                for (auto it = mount_list->begin(); it != mount_list->end(); ++it) {
                    if (it != mount_list->begin()) ss2 << "/";
                    ss2 << *it;
                }
                *mountPath = ss2.str();
            }
        }
    } else for (const std::string& s : pathc) ss << (ss.tellp() == 0 ? WS("") : WS("/")) << wstr(s);
    return ss.str();
}

bool fixpath_ro(Computer *comp, const char * path) {
    std::vector<std::string> elems = split(path, '/');
    std::list<std::string> pathc;
    for (const std::string& s : elems) {
        if (s == "..") { if (pathc.empty()) return false; else pathc.pop_back(); } else if (s != "." && !s.empty()) pathc.push_back(s);
    }
    std::pair<size_t, bool> max_path = std::make_pair(0, false);
    for (const auto& m : comp->mounts)
        if (pathc.size() >= std::get<0>(m).size() && std::get<0>(m).size() > max_path.first && std::equal(std::get<0>(m).begin(), std::get<0>(m).end(), pathc.begin()))
            max_path = std::make_pair(std::get<0>(m).size(), std::get<2>(m));
    return max_path.second;
}

std::set<std::string> getMounts(Computer * computer, const char * comp_path) {
    std::vector<std::string> elems = split(comp_path, '/');
    std::list<std::string> pathc;
    std::set<std::string> retval;
    for (const std::string& s : elems) {
        if (s == "..") { if (pathc.empty()) return retval; else pathc.pop_back(); } else if (s != "." && !s.empty()) pathc.push_back(s);
    }
    for (const auto& m : computer->mounts)
        if (pathc.size() + 1 == std::get<0>(m).size() && std::equal(pathc.begin(), pathc.end(), std::get<0>(m).begin()))
            retval.insert(std::get<0>(m).back());
    return retval;
}

static void xcopy_internal(lua_State *from, lua_State *to, int n, std::unordered_set<const void*>& copies) {
    for (int i = n - 1; i >= 0; i--) {
        if (lua_type(from, -1-i) == LUA_TNUMBER) lua_pushnumber(to, lua_tonumber(from, -1-i));
        else if (lua_type(from, -1-i) == LUA_TSTRING) lua_pushlstring(to, lua_tostring(from, -1-i), lua_strlen(from, -1-i));
        else if (lua_type(from, -1-i) == LUA_TBOOLEAN) lua_pushboolean(to, lua_toboolean(from, -1-i));
        else if (lua_type(from, -1-i) == LUA_TTABLE) {
            const void* ptr = lua_topointer(from, -1-i);
            if (copies.count(ptr)) {
                lua_pushstring(to, "<recursive table>");
                continue;
            } else copies.insert(ptr);
            lua_newtable(to);
            lua_pushnil(from);
            while (lua_next(from, -2-i) != 0) {
                xcopy_internal(from, to, 2, copies);
                lua_settable(to, -3);
                lua_pop(from, 1);
            }
        } else if (lua_isnil(from, -1-i)) lua_pushnil(to);
        else {
            if (luaL_callmeta(from, -1-i, "__tostring")) {
                lua_pushlstring(to, lua_tostring(from, -1), lua_strlen(from, -1));
                lua_pop(from, 1);
            } else lua_pushfstring(to, "<%s: %p>", lua_typename(from, lua_type(from, -1-i)), lua_topointer(from, -1-i));
        }
    }
}

void xcopy(lua_State *from, lua_State *to, int n) {
    std::unordered_set<const void*> copies;
    xcopy_internal(from, to, n, copies);
}