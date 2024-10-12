/*
 * fs.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the fs API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <codecvt>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <Computer.hpp>
#include <configuration.hpp>
#include <dirent.h>
#include <FileEntry.hpp>
#include <sys/stat.h>
#include "handles/fs_handle.hpp"
#include "../platform.hpp"
#include "../runtime.hpp"
#ifdef WIN32
#include <io.h>
#define W_OK 0x02
#define access(p, m) _waccess(p, m)
#else
#include <libgen.h>
#include <unistd.h>
#endif
#if defined(__INTELLISENSE__) && !defined(S_ISDIR)
#define S_ISDIR(m) 1 // silence errors in IntelliSense (which isn't very intelligent for its name)
#define W_OK 2
#endif

static std::basic_regex<path_t::value_type> pathregex(const std::string& str) {return std::basic_regex<path_t::value_type>(path_t(str).native());};

#define err(L, idx, err) luaL_error(L, "/%s: %s", fixpath(get_comp(L), lua_tostring(L, idx), false, false).string().c_str(), err)

#ifdef STANDALONE_ROM
extern FileEntry standaloneROM;
extern FileEntry standaloneDebug;
extern std::string standaloneBIOS;
#endif

static path_t ignored_files[4] = {
    ".",
    "..",
    ".DS_Store",
    "desktop.ini"
};

static bool _nothrow(std::function<void()> f) { try { f(); return true; } catch (...) { return false; } }
#define nothrow(expr) _nothrow([&](){ expr ;})

inline bool isVFSPath(path_t path) {
    if (!std::isdigit(path.native()[0])) return false;
    for (const path_t::value_type& c : path.native()) {
        if (c == ':') return true;
        else if (!std::isdigit(c)) return false;
    }
    return false;
}

static std::vector<path_t> fixpath_multiple(Computer *comp, std::string path) {
    std::vector<path_t> retval;
    path.erase(std::remove_if(path.begin(), path.end(), [](char c)->bool {return c == '"' || c == '*' || c == ':' || c == '<' || c == '>' || c == '?' || c == '|' || c < 32; }), path.end());
    std::vector<std::string> elems = split(path, "/\\");
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") {
            if (pathc.empty()) return retval;
            else if (pathc.empty()) pathc.push_back("..");
            else pathc.pop_back();
        } else if (!s.empty() && !std::all_of(s.begin(), s.end(), [](const char c)->bool{return c == '.';})) {
            pathc.push_back(s);
        }
    }
    while (!pathc.empty() && pathc.front().empty()) pathc.pop_front();
    if (comp->isDebugger && pathc.size() == 1 && pathc.front() == "bios.lua")
#ifdef STANDALONE_ROM
        return {path_t(":bios.lua", path_t::format::generic_format)};
#else
        return {getROMPath()/"bios.lua"};
#endif
    std::pair<size_t, std::vector<_path_t> > max_path = std::make_pair(0, std::vector<_path_t>(1, comp->dataDir));
    std::list<std::string> * mount_list = NULL;
    for (auto& m : comp->mounts) {
        std::list<std::string> &pathlist = std::get<0>(m);
        if (pathc.size() >= pathlist.size() && std::equal(pathlist.begin(), pathlist.end(), pathc.begin())) {
            if (pathlist.size() > max_path.first) {
                max_path = std::make_pair(pathlist.size(), std::vector<_path_t>(1, std::get<1>(m)));
                mount_list = &pathlist;
            } else if (pathlist.size() == max_path.first) {
                max_path.second.push_back(std::get<1>(m));
            }
        }
    }
    for (size_t i = 0; i < max_path.first; i++) pathc.pop_front();
    for (const _path_t& p : max_path.second) {
        path_t sstmp = p;
        std::error_code e;
        for (const std::string& s : pathc) sstmp /= s;
        if (
            (isVFSPath(p) && nothrow(comp->virtualMounts[(unsigned)std::stoul(p.substr(0, p.size()-1))]->path(sstmp.string()))) ||
            (fs::exists(sstmp, e))) {
            if (path_t::preferred_separator != (path_t::value_type)'/' && isVFSPath(sstmp)) {
                path_t::string_type str = sstmp.native();
                std::replace(str.begin(), str.end(), path_t::preferred_separator, (path_t::value_type)'/');
                sstmp = path_t(str);
            }
            retval.push_back(sstmp);
        }
    }
    return retval;
}

static std::string normalizePath(const path_t& basePath) {
    path_t cleanPath;
    for (const auto& p : basePath) {
        if (std::regex_match(p.native(), pathregex("^\\.\\.\\.+$"))) cleanPath /= ".";
        else cleanPath /= p;
    }
    cleanPath = cleanPath.lexically_normal();
    if (path_t::preferred_separator != (path_t::value_type)'/') {
        path_t::string_type str = cleanPath.native();
        std::replace(str.begin(), str.end(), path_t::preferred_separator, (path_t::value_type)'/');
        cleanPath = path_t(str);
    }
    std::string retval = cleanPath.string();
    if (retval == ".") retval = "";
    if (!retval.empty() && retval[0] == '/') retval = retval.substr(1);
    if (!retval.empty() && retval[retval.size()-1] == '/') retval = retval.substr(0, retval.size()-1);
    return retval;
}

static fs::space_info getSpace(const path_t& path) {
    std::error_code e;
    fs::space_info retval;
    path_t p = path;
    do {
        retval = fs::space(p, e);
        p = p.parent_path();
    } while (((e && e.value() == ENOENT) || retval.capacity == -1) && p.has_parent_path());
    return retval;
}

static int fs_list(lua_State *L) {
    lastCFunction = __func__;
    std::string str = checkstring(L, 1);
    const std::vector<path_t> possible_paths = fixpath_multiple(get_comp(L), str);
    if (possible_paths.empty()) err(L, 1, "No such file");
    bool gotdir = false;
    std::set<std::string> entries;
    for (const path_t& path : possible_paths) {
        if (std::regex_search((*path.begin()).native(), pathregex("^\\d+:"))) {
            try {
                const FileEntry &d = get_comp(L)->virtualMounts[(unsigned)std::stoul((*path.begin()).native())]->path(path.lexically_relative(*path.begin()));
                if (d.isDir) {
                    gotdir = true;
                    for (const auto& p : d.dir) entries.insert(p.first);
                }
            } catch (...) {continue;}
        } else {
            std::error_code e;
            if (fs::is_directory(path, e)) {
                gotdir = true;
                for (const auto& dir : fs::directory_iterator(path, e)) {
                    if (dir.path().filename() == ".DS_Store" || dir.path().filename() == "desktop.ini") continue;
                    entries.insert(dir.path().filename().u8string());
                }
            }
        }
    }
    if (!gotdir) err(L, 1, "Not a directory");
    std::set<std::string> mounts = getMounts(get_comp(L), str);
    std::set<std::string> all;
    std::set_union(entries.begin(), entries.end(), mounts.begin(), mounts.end(), std::inserter(all, all.begin()));
    int i = 1;
    lua_createtable(L, all.size(), 0);
    for (const std::string& p : all) {
        lua_pushinteger(L, i++);
        lua_pushstring(L, p.c_str());
        lua_settable(L, -3);
    }
    return 1;
}

static int fs_exists(lua_State *L) {
    lastCFunction = __func__;
    const path_t path = fixpath(get_comp(L), checkstring(L, 1), true);
    if (std::regex_search((*path.begin()).native(), pathregex("^\\d+:"))) {
        bool found = true;
        try {get_comp(L)->virtualMounts[(unsigned)std::stoul((*path.begin()).native())]->path(path.lexically_relative(*path.begin()));} catch (...) {found = false;}
        lua_pushboolean(L, found);
#ifdef STANDALONE_ROM
    } else if (path == ":bios.lua") {
        lua_pushboolean(L, true);
#endif
    } else {
        lua_pushboolean(L, !path.empty());
    }
    return 1;
}

static int fs_isDir(lua_State *L) {
    lastCFunction = __func__;
    const path_t path = fixpath(get_comp(L), checkstring(L, 1), true);
    if (path.empty()) {
        lua_pushboolean(L, false);
        return 1;
    }
    if (std::regex_search((*path.begin()).native(), pathregex("^\\d+:"))) {
        try {lua_pushboolean(L, get_comp(L)->virtualMounts[(unsigned)std::stoul((*path.begin()).native())]->path(path.lexically_relative(*path.begin())).isDir);} 
        catch (...) {lua_pushboolean(L, false);}
    } else {
        std::error_code e;
        lua_pushboolean(L, fs::is_directory(path, e));
    }
    return 1;
}

static int fs_isReadOnly(lua_State *L) {
    lastCFunction = __func__;
    std::string str = checkstring(L, 1);
    if (fixpath_ro(get_comp(L), str)) {
        lua_pushboolean(L, true);
        return 1;
    }
    const path_t path = fixpath_mkdir(get_comp(L), str, false);
    std::error_code e;
    if (path.empty()) err(L, 1, "Invalid path"); // This should never happen
    if (!fs::exists(path, e)) lua_pushboolean(L, false);
#ifdef WIN32
    else if (e.clear(), fs::is_directory(path, e)) {
        e.clear();
        const path_t file = path / ".reallylongfilenamethatshouldhopefullyneverexist";
        const bool didexist = fs::exists(file, e);
        std::fstream fp(file, didexist ? std::ios::in : std::ios::out);
        lua_pushboolean(L, !fp.is_open());
        if (fp.is_open()) fp.close();
        e.clear();
        if (!didexist && fs::exists(file, e)) fs::remove(file, e);
    }
#endif
    else lua_pushboolean(L, access(path.native().c_str(), W_OK) != 0);
    return 1;
}

static int fs_getName(lua_State *L) {
    lastCFunction = __func__;
    pushstring(L, path_t(normalizePath(checkstring(L, 1))).filename().string());
    return 1;
}

static int fs_getDrive(lua_State *L) {
    lastCFunction = __func__;
    std::string retval;
    std::string str = checkstring(L, 1);
    fixpath_mkdir(get_comp(L), str + "/a", false, &retval);
    lua_pushstring(L, retval.c_str());
    return 1;
}

static int fs_getSize(lua_State *L) {
    lastCFunction = __func__;
    std::string str = checkstring(L, 1);
    const path_t path = fixpath(get_comp(L), str, true);
    std::error_code e;
    if (path.empty()) err(L, 1, "No such file");
    if (std::regex_search((*path.begin()).native(), pathregex("^\\d+:"))) {
        try {
            const FileEntry &d = get_comp(L)->virtualMounts[(unsigned)std::stoul((*path.begin()).native())]->path(path.lexically_relative(*path.begin()));
            if (d.isDir) err(L, 1, "Is a directory");
            lua_pushinteger(L, d.data.size());
        } catch (...) {err(L, 1, "No such file");}
#ifdef STANDALONE_ROM
    } else if (path == ":bios.lua") {
        lua_pushinteger(L, standaloneBIOS.size());
#endif
    } else if (fs::is_directory(path, e)) {
        lua_pushinteger(L, 0);
    } else {
        lua_pushinteger(L, fs::file_size(path, e));
    }
    return 1;
}

static int calculateDirectorySize(const path_t& path) {
    int size = 0;
    std::error_code e;
    for (const auto& dir : fs::directory_iterator(path, e)) {
        if (dir.is_directory()) size += calculateDirectorySize(dir.path());
        else size += dir.file_size(e);
    }
    return size;
}

static int fs_getFreeSpace(lua_State *L) {
    lastCFunction = __func__;
    std::string mountPath;
    std::string str = checkstring(L, 1);
    const path_t path = fixpath(get_comp(L), str, false, true, &mountPath);
    if (path.empty()) err(L, 1, "No such path");
    if (fixpath_ro(get_comp(L), str)) lua_pushinteger(L, 0);
    else if (!config.standardsMode || mountPath != "hdd") lua_pushinteger(L, getSpace(path).free);
    else lua_pushinteger(L, config.computerSpaceLimit - calculateDirectorySize(fixpath(get_comp(L), "", true)));
    return 1;
}

static int fs_makeDir(lua_State *L) {
    lastCFunction = __func__;
    std::string str = checkstring(L, 1);
    if (fixpath_ro(get_comp(L), str)) err(L, 1, "Access denied");
    const path_t path = fixpath_mkdir(get_comp(L), str);
    if (path.empty()) err(L, 1, "Could not create directory");
    if (std::regex_search((*path.begin()).native(), pathregex("^\\d+:"))) err(L, 1, "Permission denied");
    std::error_code e;
    fs::create_directories(path, e);
    if (e) {
        if (e.value() == ENOTDIR) e.assign(EEXIST, std::generic_category());
        err(L, 1, e.message().c_str());
    }
    return 0;
}

static int fs_move(lua_State *L) {
    lastCFunction = __func__;
    std::string str1 = checkstring(L, 1);
    std::string str2 = checkstring(L, 2);
    if (fixpath_ro(get_comp(L), str1)) luaL_error(L, "Access denied");
    if (fixpath_ro(get_comp(L), str2)) luaL_error(L, "Access denied");
    bool isRoot = false;
    const path_t fromPath = fixpath(get_comp(L), str1, true, true, NULL, &isRoot);
    const path_t toPath = fixpath_mkdir(get_comp(L), str2);
    if (fromPath.empty()) luaL_error(L, "No such file");
    if (toPath.empty()) err(L, 2, "Invalid path");
    if (std::regex_search((*fromPath.begin()).native(), pathregex("^\\d+:"))) err(L, 1, "Permission denied");
    if (std::regex_search((*toPath.begin()).native(), pathregex("^\\d+:"))) err(L, 2, "Permission denied");
    if (std::mismatch(toPath.begin(), toPath.end(), fromPath.begin(), fromPath.end()).second == fromPath.end()) 
        luaL_error(L, "Can't move a directory inside itself");
    if (isRoot) luaL_error(L, "Cannot move mount");
    std::error_code e;
    if (fs::exists(toPath, e)) luaL_error(L, "File exists");
    e.clear();
    fs::create_directories(toPath.parent_path(), e);
    if (e) err(L, 2, e.message().c_str());
    fs::rename(fromPath, toPath, e);
    if (e) err(L, 1, e.message().c_str());
    return 0;
}

static int fs_copy(lua_State *L) {
    lastCFunction = __func__;
    std::string str1 = checkstring(L, 1);
    std::string str2 = checkstring(L, 2);
    if (fixpath_ro(get_comp(L), str2)) luaL_error(L, "/%s: Access denied", fixpath(get_comp(L), str2, false, false).c_str());
    const path_t fromPath = fixpath(get_comp(L), str1, true);
    const path_t toPath = fixpath_mkdir(get_comp(L), str2);
    if (fromPath.empty()) err(L, 1, "No such file");
    if (toPath.empty()) err(L, 2, "Invalid path");
    if (std::regex_search((*toPath.begin()).native(), pathregex("^\\d+:"))) err(L, 2, "Permission denied");
    if (std::regex_search((*fromPath.begin()).native(), pathregex("^\\d+:"))) {
        try {
            const FileEntry &d = get_comp(L)->virtualMounts[(unsigned)std::stoul((*fromPath.begin()).c_str())]->path(fromPath.lexically_relative(*fromPath.begin()));
            if (d.isDir) err(L, 1, "Is a directory");
            std::ofstream tofp(toPath);
            if (!tofp.is_open()) return err(L, 2, "Cannot write file");
            tofp.write(d.data.c_str(), d.data.size());
            tofp.close();
        } catch (...) {err(L, 1, "No such file");}
    } else {
        /*if (isFSCaseSensitive == -1) {
            struct_stat st;
            char* name = tmpnam(NULL);
            fclose(platform_fopen(name, "w"));
            std::transform(name, name + strlen(name), name, [](char c)->char{return isupper(c) ? tolower(c) : toupper(c);});
            isFSCaseSensitive = stat(name, &st);
            remove(name);
        }*/
        std::vector<std::string> fromElems = split(str1, "/\\"), toElems = split(str2, "/\\");
        while (!fromElems.empty() && fromElems.front().empty()) fromElems.erase(fromElems.begin());
        while (!toElems.empty() && toElems.front().empty()) toElems.erase(toElems.begin());
        while (!fromElems.empty() && fromElems.back().empty()) fromElems.pop_back();
        while (!toElems.empty() && toElems.back().empty()) toElems.pop_back();
        bool equal = true;
        for (unsigned i = 0; i < toElems.size() && equal; i++) {
            if (i >= fromElems.size()) err(L, 1, "Can't copy a directory inside itself");
            std::string lstrfrom = fromElems[i], lstrto = toElems[i];
            std::transform(lstrfrom.begin(), lstrfrom.end(), lstrfrom.begin(), [](unsigned char c) {return std::tolower(c);});
            std::transform(lstrto.begin(), lstrto.end(), lstrto.begin(), [](unsigned char c) {return std::tolower(c);});
            if (lstrfrom != lstrto) equal = false;
            else if ((i == fromElems.size() - 1 && i == toElems.size() - 1)) err(L, 1, "Can't copy a directory inside itself");
        }
        if (equal) err(L, 1, "Can't copy a directory inside itself");
        std::error_code e;
        fs::create_directories(toPath.parent_path(), e);
        if (e) err(L, 2, e.message().c_str());
        fs::copy(fromPath, toPath, fs::copy_options::recursive, e);
        if (e) err(L, 1, e.message().c_str());
    }
    return 0;
}

static int fs_delete(lua_State *L) {
    lastCFunction = __func__;
    std::string str = checkstring(L, 1);
    if (fixpath_ro(get_comp(L), str)) err(L, 1, "Access denied");
    bool isRoot = false;
    const path_t path = fixpath(get_comp(L), str, true, true, NULL, &isRoot);
    if (isRoot) luaL_error(L, "Cannot delete mount, use mounter.unmount instead");
    if (path.empty()) return 0;
    if (std::regex_search((*path.begin()).native(), pathregex("^\\d+:"))) err(L, 1, "Permission denied");
    std::error_code e;
    fs::remove_all(path, e);
    if (e) err(L, 1, e.message().c_str());
    return 0;
}

static int fs_combine(lua_State *L) {
    lastCFunction = __func__;
    path_t basePath = path_t(checkstring(L, 1), path_t::format::generic_format);
    for (int i = 2; i <= lua_gettop(L); i++) if (!checkstring(L, i).empty()) {
        std::string str = tostring(L, i);
        if (str[0] == '/' || str[0] == '\\') str = str.substr(1);
        basePath /= str;
    }
    pushstring(L, normalizePath(basePath));
    return 1;
}

static int fs_open(lua_State *L) {
    lastCFunction = __func__;
    Computer * computer = get_comp(L);
    const char * mode = luaL_checkstring(L, 2);
    if (
        (mode[0] != 'r' && mode[0] != 'w' && mode[0] != 'a') ||
        (
            !(mode[1] == '+' && mode[2] == 'b' && mode[3] == '\0') &&
            !(mode[1] == '+' && mode[2] == '\0') &&
            !(mode[1] == 'b' && mode[2] == '\0') &&
            mode[1] != '\0'
        )) luaL_error(L, "%s: Unsupported mode", mode);
    std::string str = checkstring(L, 1);
    const path_t path = mode[0] == 'r' ? fixpath(get_comp(L), str, true) : fixpath_mkdir(get_comp(L), str);
    if (path.empty()) {
        if (mode[0] != 'r' && fixpath_ro(computer, str)) {
            lua_pushnil(L);
            lua_pushfstring(L, "/%s: Access denied", fixpath(computer, str, false, false).string().c_str());
            return 2;
        } else {
            lua_pushnil(L);
            lua_pushfstring(L, "/%s: No such file", fixpath(computer, str, false, false).string().c_str());
            return 2;
        }
    }
    int fpid;
    if (std::regex_search((*path.begin()).native(), pathregex("^\\d+:")) || path == ":bios.lua") {
        if (computer->files_open >= config.maximumFilesOpen) err(L, 1, "Too many files already open");
        std::stringstream ** fp = (std::stringstream**)lua_newuserdata(L, sizeof(std::stringstream**));
        fpid = lua_gettop(L);
#ifdef STANDALONE_ROM
        if (path == ":bios.lua") {
            *fp = new std::stringstream(standaloneBIOS);
        } else {
#endif
            try {
                const FileEntry &d = computer->virtualMounts[(unsigned)std::stoul((*path.begin()).native())]->path(path.lexically_relative(*path.begin()));
                if (d.isDir) {
                    lua_remove(L, fpid);
                    lua_pushnil(L);
                    if (strchr(mode, 'r') != NULL) lua_pushfstring(L, "/%s: Not a file", fixpath(computer, str, false, false).string().c_str());
                    else lua_pushfstring(L, "/%s: Cannot write to directory", fixpath(computer, str, false, false).string().c_str());
                    return 2; 
                }
                *fp = new std::stringstream(d.data);
            } catch (...) {
                lua_remove(L, fpid);
                lua_pushnil(L);
                lua_pushfstring(L, "/%s: No such file", fixpath(computer, str, false, false).string().c_str());
                return 2;
            }
#ifdef STANDALONE_ROM
        }
#endif
    } else {
        std::error_code e;
        if (fs::is_directory(path, e)) { 
            lua_pushnil(L);
            if (strchr(mode, 'r') != NULL) lua_pushfstring(L, "/%s: Not a file", fixpath(computer, str, false, false).string().c_str());
            else lua_pushfstring(L, "/%s: Cannot write to directory", fixpath(computer, str, false, false).string().c_str());
            return 2; 
        }
        if (strcmp(mode, "w") == 0 || strcmp(mode, "a") == 0 || strcmp(mode, "wb") == 0 || strcmp(mode, "ab") == 0) {
            if (fixpath_ro(computer, str)) {
                lua_pushnil(L);
                lua_pushfstring(L, "/%s: Access denied", fixpath(computer, str, false, false).string().c_str());
                return 2; 
            }
            e.clear();
            fs::create_directories(path.parent_path(), e);
            if (e) {
                lua_pushnil(L);
                lua_pushfstring(L, "/%s: Cannot create directory", fixpath(computer, str, false, false).string().c_str());
                return 2; 
            }
        }
        std::fstream ** fp = (std::fstream**)lua_newuserdata(L, sizeof(std::fstream*));
        fpid = lua_gettop(L);
        std::ios::openmode flags = std::ios::binary;
        if (strchr(mode, 'r')) {
            flags |= std::ios::in;
            if (strchr(mode, '+')) flags |= std::ios::out;
        } else if (strchr(mode, 'w')) {
            flags |= std::ios::out | std::ios::trunc;
            if (strchr(mode, '+')) flags |= std::ios::in;
        } else if (strchr(mode, 'a')) {
            flags |= std::ios::in | std::ios::out | std::ios::ate;
            if (strchr(mode, '+')) flags |= std::ios::in;
        }
        *fp = new std::fstream(path, flags);
        if (!(*fp)->is_open()) {
            bool ok = false;
            if (strchr(mode, 'a')) {
                (*fp)->open(path, (flags & ~std::ios::ate) | std::ios::trunc);
                ok = (*fp)->is_open();
            }
            if (!ok) {
                delete *fp;
                lua_remove(L, fpid);
                lua_pushnil(L);
                lua_pushfstring(L, "/%s: No such file", fixpath(computer, str, false, false).native().c_str());
                return 2;
            }
        }
        if (computer->files_open >= config.maximumFilesOpen) {
            (*fp)->close();
            delete *fp;
            err(L, 1, "Too many files already open");
        }
    }
    lua_createtable(L, 0, 1);
    lua_pushvalue(L, fpid);
    lua_pushcclosure(L, fs_handle_gc, 1);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);
    
    lua_createtable(L, 0, 4);
    lua_pushstring(L, "close");
    lua_pushvalue(L, fpid);
    lua_pushcclosure(L, fs_handle_close, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "seek");
    lua_pushvalue(L, fpid);
    lua_pushcclosure(L, fs_handle_seek, 1);
    lua_settable(L, -3);

    if (mode[0] == 'r' || strchr(mode, '+')) {
        if (strchr(mode, 'b')) {
            lua_pushstring(L, "read");
            lua_pushvalue(L, fpid);
            lua_pushcclosure(L, fs_handle_readByte, 1);
            lua_settable(L, -3);
        } else {
            lua_pushstring(L, "read");
            lua_pushvalue(L, fpid);
            lua_pushcclosure(L, fs_handle_readChar, 1);
            lua_settable(L, -3);
        }

        lua_pushstring(L, "readAll");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_readAllByte, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "readLine");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_readLine, 1);
        lua_settable(L, -3);
    }

    if (mode[0] == 'w' || mode[0] == 'a' || strchr(mode, '+')) {
        if (strchr(mode, 'b')) {
            lua_pushstring(L, "write");
            lua_pushvalue(L, fpid);
            lua_pushcclosure(L, fs_handle_writeByte, 1);
            lua_settable(L, -3);
        } else {
            lua_pushstring(L, "write");
            lua_pushvalue(L, fpid);
            lua_pushcclosure(L, fs_handle_writeString, 1);
            lua_settable(L, -3);
        }

        lua_pushstring(L, "writeLine");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_writeLine, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "flush");
        lua_pushvalue(L, fpid);
        lua_pushcclosure(L, fs_handle_flush, 1);
        lua_settable(L, -3);
    }

    computer->files_open++;
    return 1;
}

static std::string replace_str(std::string data, const std::string& toSearch, const std::string& replaceStr) {
    size_t pos = data.find(toSearch);
    while (pos != std::string::npos) {
        data.replace(pos, toSearch.size(), replaceStr);
        pos = data.find(toSearch, pos + replaceStr.size());
    }
    return data;
}

static std::string regex_escape[] = {"\\", ".", "[", "]", "{", "}", "^", "$", "(", ")", "+", "?", "|"};

static std::list<std::string> matchWildcard(Computer * comp, const std::list<std::string>& options, std::list<std::string>::iterator pathc, const std::list<std::string>::iterator end) {
    if (pathc == end) return {};
    std::string pathc_regex = *pathc;
    for (const std::string& r : regex_escape) pathc_regex = replace_str(pathc_regex, r, "\\" + r);
    pathc_regex = replace_str(pathc_regex, "*", ".*");
    std::list<std::string> nextOptions;
    for (const std::string& opt : options) {
        std::vector<path_t> possible_paths = fixpath_multiple(comp, opt.c_str());
        if (possible_paths.empty()) continue;
        for (const path_t& path : possible_paths) {
            if (std::regex_search((*path.begin()).native(), pathregex("^\\d+:"))) {
                try {
                    const FileEntry &d = comp->virtualMounts[(unsigned)std::stoul((*path.begin()).native())]->path(path.lexically_relative(*path.begin()));
                    if (d.isDir) for (auto p : d.dir) if (std::regex_match(p.first, std::regex(pathc_regex))) nextOptions.push_back(opt + (opt == "" ? "" : "/") + p.first);
                } catch (...) {continue;}
            } else {
                std::error_code e;
                if (fs::is_directory(path, e)) {
                    for (const auto& dir : fs::directory_iterator(path, e)) {
                        if (dir.path().filename() == ".DS_Store" || dir.path().filename() == "desktop.ini") continue;
                        if (std::regex_match(dir.path().filename().u8string(), std::regex(pathc_regex))) nextOptions.push_back(opt + (opt.empty() ? "" : "/") + dir.path().filename().u8string());
                    }
                }
            }
        }
        for (const std::string& value : getMounts(comp, opt.c_str())) 
            if (*pathc == "*" || value == *pathc) nextOptions.push_back(opt + (opt.empty() ? "" : "/") + value);
    }
    if (++pathc == end) return nextOptions;
    else return matchWildcard(comp, nextOptions, pathc, end);
}

// Deprecated as of 1.105.0, but remains here for safety.
static int fs_find(lua_State *L) {
    lastCFunction = __func__;
    std::string str = checkstring(L, 1);
    std::vector<std::string> elems = split(str, "/\\");
    std::list<std::string> pathc;
    for (const std::string& s : elems) {
        if (s == "..") { 
            if (pathc.empty()) luaL_error(L, "Not a directory");
            else pathc.pop_back(); 
        }
        else if (!s.empty() && !std::all_of(s.begin(), s.end(), [](const char c)->bool{return c == '.';})) pathc.push_back(s);
    }
    while (!pathc.empty() && pathc.front().empty()) pathc.pop_front();
    while (!pathc.empty() && pathc.back().empty()) pathc.pop_back();
    if (pathc.empty()) {
        lua_createtable(L, 1, 0);
        lua_pushstring(L, "");
        lua_rawseti(L, -2, 1);
        return 1;
    }
    std::list<std::string> matches = matchWildcard(get_comp(L), {""}, pathc.begin(), pathc.end());
    matches.sort();
    matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    lua_createtable(L, matches.size(), 0);
    lua_Integer i = 0;
    for (const std::string& m : matches) {
        lua_pushinteger(L, ++i);
        lua_pushstring(L, m.c_str());
        lua_settable(L, -3);
    }
    return 1;
}

static int fs_getDir(lua_State *L) {
    lastCFunction = __func__;
    path_t path = path_t(normalizePath(checkstring(L, 1)));
    if (path.empty() || path.string() == "/") {
        lua_pushliteral(L, "..");
        return 1;
    }
    if (!path.has_filename()) path = path.parent_path();
    pushstring(L, path.parent_path().string());
    return 1;
}

#if defined(__APPLE__) // macOS has ns-precise times in st_[x]timespec.tv_nsec
#define st_time_ms(st) ((st##timespec.tv_nsec / 1000000) + (st##timespec.tv_sec * 1000))
#elif defined(__linux__) // Linux has ns-precise times in st_[x]tim.tv_nsec
#define st_time_ms(st) ((st##tim.tv_nsec / 1000000) + (st##time * 1000))
#else // Other systems have only the standard s-precise times
#define st_time_ms(st) (st##time * 1000)
#endif

static int fs_attributes(lua_State *L) {
    lastCFunction = __func__;
    std::string str = checkstring(L, 1);
    const path_t path = fixpath(get_comp(L), str, true);
    if (path.empty()) err(L, 1, "No such file");
    if (std::regex_search((*path.begin()).native(), pathregex("^\\d+:"))) {
        try {
            const FileEntry &d = get_comp(L)->virtualMounts[(unsigned)std::stoul((*path.begin()).native())]->path(path.lexically_relative(*path.begin()));
            lua_createtable(L, 0, 6);
            lua_pushinteger(L, 0);
            lua_setfield(L, -2, "modification");
            lua_pushinteger(L, 0);
            lua_setfield(L, -2, "modified");
            lua_pushinteger(L, 0);
            lua_setfield(L, -2, "created");
            lua_pushinteger(L, d.isDir ? 0 : d.data.length());
            lua_setfield(L, -2, "size");
            lua_pushboolean(L, d.isDir);
            lua_setfield(L, -2, "isDir");
            lua_pushboolean(L, true);
            lua_setfield(L, -2, "isReadOnly");
        } catch (...) {
            lua_pushnil(L);
            return 1;
        }
    } else {
#ifdef _WIN32
        struct _stat st;
        if (_wstat(path.c_str(), &st) != 0) {
            lua_pushnil(L);
            return 1;
        }
#else
        struct stat st;
        if (stat(path.c_str(), &st) != 0) {
            lua_pushnil(L);
            return 1;
        }
#endif
        lua_createtable(L, 0, 6);
        lua_pushinteger(L, st_time_ms(st.st_m));
        lua_setfield(L, -2, "modification");
        lua_pushinteger(L, st_time_ms(st.st_m));
        lua_setfield(L, -2, "modified");
        lua_pushinteger(L, st_time_ms(st.st_c));
        lua_setfield(L, -2, "created");
        lua_pushinteger(L, S_ISDIR(st.st_mode) ? 0 : st.st_size);
        lua_setfield(L, -2, "size");
        lua_pushboolean(L, S_ISDIR(st.st_mode));
        lua_setfield(L, -2, "isDir");
        if (fixpath_ro(get_comp(L), str)) lua_pushboolean(L, true);
        else {
            std::error_code e;
            if (!fs::exists(path, e)) lua_pushboolean(L, false);
#ifdef WIN32
            else if (e.clear(), fs::is_directory(path, e)) {
                const path_t file = path / "a";
                const bool didexist = fs::exists(file, e);
                std::fstream fp(file, didexist ? std::ios::in : std::ios::out);
                lua_pushboolean(L, !fp.is_open());
                fp.close();
                e.clear();
                if (!didexist && fs::exists(file, e)) fs::remove(file, e);
            }
#endif
            else lua_pushboolean(L, access(path.c_str(), W_OK) != 0);
        }
        lua_setfield(L, -2, "isReadOnly");
    }
    return 1;
}

static int fs_getCapacity(lua_State *L) {
    lastCFunction = __func__;
    std::string mountPath;
    std::string str = checkstring(L, 1);
    const path_t path = fixpath(get_comp(L), str, false, true, &mountPath);
    if (mountPath == "rom") {
        lua_pushnil(L);
        return 1;
    } else if (mountPath == "hdd" && config.standardsMode) {
        lua_pushinteger(L, config.computerSpaceLimit);
        return 1;
    }
    if (path.empty()) luaL_error(L, "%s: Invalid path", str.c_str());
    lua_pushinteger(L, getSpace(path).capacity);
    return 1;
}

static int fs_isDriveRoot(lua_State *L) {
    lastCFunction = __func__;
    bool res = false;
    std::string str = checkstring(L, 1);
    fixpath(get_comp(L), str, false, true, NULL, &res);
    lua_pushboolean(L, res);
    return 1;
}

static luaL_Reg fs_reg[] = {
    {"list", fs_list},
    {"exists", fs_exists},
    {"isDir", fs_isDir},
    {"isReadOnly", fs_isReadOnly},
    {"getName", fs_getName},
    {"getDrive", fs_getDrive},
    {"getSize", fs_getSize},
    {"getFreeSpace", fs_getFreeSpace},
    {"makeDir", fs_makeDir},
    {"move", fs_move},
    {"copy", fs_copy},
    {"delete", fs_delete},
    {"combine", fs_combine},
    {"open", fs_open},
    {"find", fs_find},
    {"getDir", fs_getDir},
    {"attributes", fs_attributes},
    {"getCapacity", fs_getCapacity},
    {"isDriveRoot", fs_isDriveRoot},
    {NULL, NULL}
};

library_t fs_lib = {"fs", fs_reg, nullptr, nullptr};
