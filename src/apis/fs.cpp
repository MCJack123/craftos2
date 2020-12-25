/*
 * fs.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the fs API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <codecvt>
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
#define PATH_SEP L"\\"
#else
#include <libgen.h>
#include <unistd.h>
#define PATH_SEP "/"
#define PATH_SEPC '/'
#endif
#if defined(__INTELLISENSE__) && !defined(S_ISDIR)
#define S_ISDIR(m) 1 // silence errors in IntelliSense (which isn't very intelligent for its name)
#define W_OK 2
#endif

#define err(L, idx, err) luaL_error(L, "/%s: %s", fixpath(get_comp(L), lua_tostring(L, idx), false, false).c_str(), err)

#ifdef STANDALONE_ROM
extern FileEntry standaloneROM;
extern FileEntry standaloneDebug;
extern std::string standaloneBIOS;
#endif

static path_t ignored_files[4] = {
    WS("."),
    WS(".."),
    WS(".DS_Store"),
    WS("desktop.ini")
};

static int fs_list(lua_State *L) {
    lastCFunction = __func__;
    struct_dirent *dir;
    const path_t paths = fixpath(get_comp(L), luaL_checkstring(L, 1), true, true, NULL, true);
    if (paths.empty()) err(L, 1, "Not a directory");
    std::vector<path_t> possible_paths = split(paths, WS('\n'));
    bool gotdir = false;
    std::set<std::string> entries;
    for (const path_t& path : possible_paths) {
        if (std::regex_search(path, pathregex(WS("^\\d+:")))) {
            try {
                const FileEntry &d = get_comp(L)->virtualMounts[(unsigned)std::stoul(path.substr(0, path.find_first_of(':')))]->path(path.substr(path.find_first_of(':') + 1));
                gotdir = true;
                if (d.isDir) for (const auto& p : d.dir) entries.insert(p.first);
                else gotdir = false;
            } catch (...) {continue;}
        } else {
            platform_DIR * d = platform_opendir(path.c_str());
            if (d) {
                gotdir = true;
                while ((dir = platform_readdir(d)) != NULL) {
                    bool found = false;
                    for (const path_t& ign : ignored_files) 
                        if (pathcmp(dir->d_name, ign.c_str()) == 0) found = true;
                    if (!found) entries.insert(astr(dir->d_name));
                }
                platform_closedir(d);
            }
        }
    }
    if (!gotdir) err(L, 1, "Not a directory");
    std::set<std::string> mounts = getMounts(get_comp(L), lua_tostring(L, 1));
    std::set<std::string> all;
    std::set_union(entries.begin(), entries.end(), mounts.begin(), mounts.end(), std::inserter(all, all.begin()));
    int i = 1;
    lua_newtable(L);
    for (const std::string& p : all) {
        lua_pushinteger(L, i++);
        lua_pushstring(L, p.c_str());
        lua_settable(L, -3);
    }
    return 1;
}

static int fs_exists(lua_State *L) {
    lastCFunction = __func__;
    const path_t path = fixpath(get_comp(L), luaL_checkstring(L, 1), true);
    if (std::regex_search(path, pathregex(WS("^\\d+:")))) {
        bool found = true;
        try {get_comp(L)->virtualMounts[(unsigned)std::stoul(path.substr(0, path.find_first_of(':')))]->path(path.substr(path.find_first_of(':') + 1));} catch (...) {found = false;}
        lua_pushboolean(L, found);
#ifdef STANDALONE_ROM
    } else if (path == WS(":bios.lua")) {
        lua_pushboolean(L, true);
#endif
    } else {
        lua_pushboolean(L, !path.empty());
    }
    return 1;
}

static int fs_isDir(lua_State *L) {
    lastCFunction = __func__;
    const path_t path = fixpath(get_comp(L), luaL_checkstring(L, 1), true);
    if (path.empty()) {
        lua_pushboolean(L, false);
        return 1;
    }
    if (std::regex_search(path, pathregex(WS("^\\d+:")))) {
        try {lua_pushboolean(L, get_comp(L)->virtualMounts[(unsigned)std::stoul(path.substr(0, path.find_first_of(':')))]->path(path.substr(path.find_first_of(':') + 1)).isDir);} 
        catch (...) {lua_pushboolean(L, false);}
    } else {
        struct_stat st;
        lua_pushboolean(L, platform_stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
    }
    return 1;
}

static int fs_isReadOnly(lua_State *L) {
    lastCFunction = __func__;
    if (fixpath_ro(get_comp(L), luaL_checkstring(L, 1))) {
        lua_pushboolean(L, true);
        return 1;
    }
    const path_t path = fixpath_mkdir(get_comp(L), lua_tostring(L, 1), false);
    if (path.empty()) err(L, 1, "Invalid path"); // This should never happen
    struct_stat st;
    if (platform_stat(path.c_str(), &st) != 0) lua_pushboolean(L, false);
#ifdef WIN32
    else if (S_ISDIR(st.st_mode)) {
        const path_t file = path + WS("\\a");
        FILE * fp = platform_fopen(file.c_str(), "a");
        lua_pushboolean(L, fp == NULL);
        if (fp != NULL) fclose(fp);
        if (platform_stat(file.c_str(), &st) == 0) platform_remove(file.c_str());
    }
#endif
    else lua_pushboolean(L, platform_access(path.c_str(), W_OK) != 0);
    return 1;
}

static int fs_getName(lua_State *L) {
    lastCFunction = __func__;
    luaL_checkstring(L, 1);
    char * path = new char[lua_strlen(L, 1) + 1];
    strcpy(path, lua_tostring(L, 1));
    lua_pushstring(L, basename(path));
    delete[] path;
    return 1;
}

static int fs_getDrive(lua_State *L) {
    lastCFunction = __func__;
    std::string retval;
    fixpath_mkdir(get_comp(L), std::string(luaL_checkstring(L, 1)) + "/a", false, &retval);
    lua_pushstring(L, retval.c_str());
    return 1;
}

static int fs_getSize(lua_State *L) {
    lastCFunction = __func__;
    const path_t path = fixpath(get_comp(L), luaL_checkstring(L, 1), true);
    if (path.empty()) err(L, 1, "No such file");
    if (std::regex_search(path, pathregex(WS("^\\d+:")))) {
        try {
            const FileEntry &d = get_comp(L)->virtualMounts[(unsigned)std::stoul(path.substr(0, path.find_first_of(':')))]->path(path.substr(path.find_first_of(':') + 1));
            if (d.isDir) err(L, 1, "Is a directory");
            lua_pushinteger(L, d.data.size());
        } catch (...) {err(L, 1, "No such file");}
#ifdef STANDALONE_ROM
    } else if (path == WS(":bios.lua")) {
        lua_pushinteger(L, standaloneBIOS.size());
#endif
    } else {
        struct_stat st;
        if (platform_stat(path.c_str(), &st) != 0) err(L, 1, "No such file"); // redundant since v2.3?
        lua_pushinteger(L, S_ISDIR(st.st_mode) ? 0 : st.st_size);
    }
    return 1;
}

static int calculateDirectorySize(const path_t& path) {
    platform_DIR * d = platform_opendir(path.c_str());
    int size = 0;
    if (d) {
        struct_dirent * dir;
        struct_stat st;
        while ((dir = platform_readdir(d)) != NULL) {
            if (path_t(dir->d_name) != WS(".") && path_t(dir->d_name) != WS("..")) {
                path_t dname = path + PATH_SEP + dir->d_name;
                if (platform_stat(dname.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) size += calculateDirectorySize(dname);
                else size += st.st_size;
            }
        }
        platform_closedir(d);
    }
    return size;
}

static int fs_getFreeSpace(lua_State *L) {
    lastCFunction = __func__;
    std::string mountPath;
    const path_t path = fixpath(get_comp(L), luaL_checkstring(L, 1), false, true, &mountPath);
    if (path.empty()) err(L, 1, "No such path");
    if (fixpath_ro(get_comp(L), lua_tostring(L, 1))) lua_pushinteger(L, 0);
    else if (!config.standardsMode || mountPath != "hdd") lua_pushinteger(L, getFreeSpace(path));
    else lua_pushinteger(L, config.computerSpaceLimit - calculateDirectorySize(fixpath(get_comp(L), "", true)));
    return 1;
}

static int fs_makeDir(lua_State *L) {
    lastCFunction = __func__;
    if (fixpath_ro(get_comp(L), luaL_checkstring(L, 1))) err(L, 1, "Access denied");
    const path_t path = fixpath_mkdir(get_comp(L), lua_tostring(L, 1));
    if (path.empty()) err(L, 1, "Could not create directory");
    struct_stat st;
    if (platform_stat(path.c_str(), &st) == 0 && !S_ISDIR(st.st_mode)) err(L, 1, "File exists");
    if (createDirectory(path) != 0 && errno != EEXIST) err(L, 1, strerror(errno));
    return 0;
}

static int fs_move(lua_State *L) {
    lastCFunction = __func__;
    if (fixpath_ro(get_comp(L), luaL_checkstring(L, 1))) luaL_error(L, "Access denied");
    if (fixpath_ro(get_comp(L), luaL_checkstring(L, 2))) luaL_error(L, "Access denied");
    const path_t fromPath = fixpath(get_comp(L), lua_tostring(L, 1), true);
    const path_t toPath = fixpath_mkdir(get_comp(L), lua_tostring(L, 2));
    if (fromPath.empty()) err(L, 1, "No such file");
    if (toPath.empty()) err(L, 2, "Invalid path");
    if (platform_rename(fromPath.c_str(), toPath.c_str()) != 0) err(L, 1, strerror(errno));
    return 0;
}

static std::pair<int, std::string> recursiveCopy(const path_t& fromPath, const path_t& toPath) {
    struct_stat st;
    if (platform_stat(toPath.c_str(), &st) == 0) return std::make_pair(2, "File exists");
    else if (platform_stat(fromPath.c_str(), &st) != 0) return std::make_pair(1, "No such file"); // likely redundant
    else if (S_ISDIR(st.st_mode)) {
        struct_dirent *dir;
        platform_DIR * d = platform_opendir(fromPath.c_str());
        if (d) {
            createDirectory(toPath);
            while ((dir = platform_readdir(d)) != NULL) {
                bool found = false;
                for (const path_t& ign : ignored_files)
                    if (pathcmp(dir->d_name, ign.c_str()) == 0) found = true;
                if (!found) {
                    auto retval = recursiveCopy(fromPath + PATH_SEP + dir->d_name, toPath + PATH_SEP + dir->d_name);
                    if (retval.first > 0) return retval;
                }
            }
            platform_closedir(d);
        } else return std::make_pair(1, "Cannot open directory");
        return std::make_pair(0, "");
    } else {
        FILE * fromfp = platform_fopen(fromPath.c_str(), "r");
        if (fromfp == NULL) return std::make_pair(1, "Cannot read file");
        FILE * tofp = platform_fopen(toPath.c_str(), "w");
        if (tofp == NULL) {
            fclose(fromfp);
            return std::make_pair(2, "Cannot write file");
        }

        char tmp[4096];
        while (!feof(fromfp)) {
            const size_t read = fread(tmp, 1, 4096, fromfp);
            fwrite(tmp, read, 1, tofp);
            if (read < 4096) break;
        }

        fclose(fromfp);
        fclose(tofp);
    }
    return std::make_pair(0, "");
}

static int fs_copy(lua_State *L) {
    lastCFunction = __func__;
    if (fixpath_ro(get_comp(L), luaL_checkstring(L, 2))) luaL_error(L, "/%s: Access denied", fixpath(get_comp(L), lua_tostring(L, 2), false, false).c_str());
    const path_t fromPath = fixpath(get_comp(L), luaL_checkstring(L, 1), true);
    const path_t toPath = fixpath_mkdir(get_comp(L), lua_tostring(L, 2));
    if (fromPath.empty()) err(L, 1, "No such file");
    if (toPath.empty()) err(L, 2, "Invalid path");
    if (std::regex_search(fromPath, pathregex(WS("^\\d+:")))) {
        try {
            const FileEntry &d = get_comp(L)->virtualMounts[(unsigned)std::stoul(fromPath.substr(0, fromPath.find_first_of(':')))]->path(fromPath.substr(fromPath.find_first_of(':') + 1));
            if (d.isDir) err(L, 1, "Is a directory");
            FILE * tofp = platform_fopen(toPath.c_str(), "w");
            if (tofp == NULL) err(L, 2, "Cannot write file");
            fwrite(d.data.c_str(), d.data.size(), 1, tofp);
            fclose(tofp);
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
        std::vector<std::string> fromElems = split(lua_tostring(L, 1), '/'), toElems = split(lua_tostring(L, 2), '/');
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
        const auto retval = recursiveCopy(fromPath, toPath);
        if (retval.first != 0) err(L, retval.first, retval.second.c_str());
    }
    return 0;
}

static int fs_delete(lua_State *L) {
    lastCFunction = __func__;
    if (fixpath_ro(get_comp(L), luaL_checkstring(L, 1))) err(L, 1, "Access denied");
    const path_t path = fixpath(get_comp(L), lua_tostring(L, 1), true);
    if (path.empty()) return 0;
    const int res = removeDirectory(path);
    if (res != 0 && res != ENOENT) err(L, 1, "Failed to remove");
    return 0;
}

static int fs_combine(lua_State *L) {
    lastCFunction = __func__;
    const std::string basePath = luaL_checkstring(L, 1);
    for (int i = 3; i <= lua_gettop(L); i++) basePath += "/" + std::string(luaL_checkstring(L, i));
    lua_pushstring(L, astr(fixpath(get_comp(L), (basePath).c_str(), false, false)).c_str());
    return 1;
}

static int fs_open(lua_State *L) {
    lastCFunction = __func__;
    Computer * computer = get_comp(L);
    if (computer->files_open >= config.maximumFilesOpen) err(L, 1, "Too many files open");
    const char * mode = luaL_checkstring(L, 2);
    if ((mode[0] != 'r' && mode[0] != 'w' && mode[0] != 'a') || (mode[1] != 'b' && mode[1] != '\0')) luaL_error(L, "%s: Unsupported mode", mode);
    const path_t path = mode[0] == 'r' ? fixpath(get_comp(L), luaL_checkstring(L, 1), true) : fixpath_mkdir(get_comp(L), luaL_checkstring(L, 1));
    if (path.empty()) {
        if (mode[0] != 'r' && fixpath_ro(computer, lua_tostring(L, 1))) {
            lua_pushnil(L);
            lua_pushfstring(L, "/%s: Access denied", astr(fixpath(computer, lua_tostring(L, 1), false, false)).c_str());
            return 2;
        } else {
            lua_pushnil(L);
            lua_pushfstring(L, "/%s: No such file", astr(fixpath(computer, lua_tostring(L, 1), false, false)).c_str());
            return 2;
        }
    }
    if (std::regex_search(path, pathregex(WS("^\\d+:"))) || path == WS(":bios.lua")) {
        std::stringstream * fp;
#ifdef STANDALONE_ROM
        if (path == WS(":bios.lua")) {
            fp = new std::stringstream(standaloneBIOS);
        } else {
#endif
            try {
                const FileEntry &d = computer->virtualMounts[(unsigned)std::stoul(path.substr(0, path.find_first_of(':')))]->path(path.substr(path.find_first_of(':') + 1));
                if (d.isDir) {
                    lua_pushnil(L);
                    if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) lua_pushfstring(L, "/%s: No such file", astr(fixpath(computer, lua_tostring(L, 1), false, false)).c_str());
                    else lua_pushfstring(L, "/%s: Cannot write to directory", astr(fixpath(computer, lua_tostring(L, 1), false, false)).c_str());
                    return 2; 
                }
                fp = new std::stringstream(d.data);
            } catch (...) {
                lua_pushnil(L);
                lua_pushfstring(L, "/%s: No such file", astr(fixpath(computer, lua_tostring(L, 1), false, false)).c_str());
                return 2;
            }
#ifdef STANDALONE_ROM
        }
#endif
        if (strcmp(mode, "r") == 0) {
            lua_newtable(L);
            lua_pushstring(L, "close");
            lua_pushlightuserdata(L, fp);
            lua_newtable(L);
            lua_pushstring(L, "__gc");
            lua_pushcclosure(L, fs_handle_istream_free, 0);
            lua_settable(L, -3);
            lua_setmetatable(L, -2);
            lua_pushcclosure(L, fs_handle_istream_close, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "readAll");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_istream_readAll, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "readLine");
            lua_pushlightuserdata(L, fp);
            lua_pushboolean(L, false);
            lua_pushcclosure(L, fs_handle_istream_readLine, 2);
            lua_settable(L, -3);

            lua_pushstring(L, "read");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_istream_readChar, 1);
            lua_settable(L, -3);
        } else if (strcmp(mode, "rb") == 0) {
            lua_newtable(L);
            lua_pushstring(L, "close");
            lua_pushlightuserdata(L, fp);
            lua_newtable(L);
            lua_pushstring(L, "__gc");
            lua_pushcclosure(L, fs_handle_istream_free, 0);
            lua_settable(L, -3);
            lua_setmetatable(L, -2);
            lua_pushcclosure(L, fs_handle_istream_close, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "read");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_istream_readByte, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "readAll");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_istream_readAllByte, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "readLine");
            lua_pushlightuserdata(L, fp);
            lua_pushboolean(L, true);
            lua_pushcclosure(L, fs_handle_istream_readLine, 2);
            lua_settable(L, -3);

            lua_pushstring(L, "seek");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_istream_seek, 1);
            lua_settable(L, -3);
        } else {
            delete fp;
            lua_pushnil(L);
            lua_pushfstring(L, "/%s: Access denied", astr(fixpath(computer, lua_tostring(L, 1), false, false)).c_str());
            return 2; 
        }
    } else {
        struct_stat st;
        if (platform_stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) { 
            lua_pushnil(L);
            if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) lua_pushfstring(L, "/%s: No such file", astr(fixpath(computer, lua_tostring(L, 1), false, false)).c_str());
            else lua_pushfstring(L, "/%s: Cannot write to directory", astr(fixpath(computer, lua_tostring(L, 1), false, false)).c_str());
            return 2; 
        }
        if (strcmp(mode, "w") == 0 || strcmp(mode, "a") == 0 || strcmp(mode, "wb") == 0 || strcmp(mode, "ab") == 0) {
            if (fixpath_ro(computer, lua_tostring(L, 1))) {
                lua_pushnil(L);
                lua_pushfstring(L, "/%s: Access denied", astr(fixpath(computer, lua_tostring(L, 1), false, false)).c_str());
                return 2; 
            }
    #ifdef WIN32
            createDirectory(path.substr(0, path.find_last_of('\\')));
    #else
            createDirectory(path.substr(0, path.find_last_of('/')));
    #endif
        }
        FILE * fp = platform_fopen(path.c_str(), mode);
        if (fp == NULL) { 
            lua_pushnil(L);
            lua_pushfstring(L, "/%s: No such file", astr(fixpath(computer, lua_tostring(L, 1), false, false)).c_str());
            return 2; 
        }
        lua_newtable(L);
        lua_pushstring(L, "close");
        lua_pushlightuserdata(L, fp);
        lua_pushcclosure(L, fs_handle_close, 1);
        lua_settable(L, -3);
        if (strcmp(mode, "r") == 0) {
            lua_pushstring(L, "readAll");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_readAll, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "readLine");
            lua_pushlightuserdata(L, fp);
            lua_pushboolean(L, false);
            lua_pushcclosure(L, fs_handle_readLine, 2);
            lua_settable(L, -3);

            lua_pushstring(L, "read");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_readChar, 1);
            lua_settable(L, -3);
        } else if (strcmp(mode, "w") == 0 || strcmp(mode, "a") == 0) {
            lua_pushstring(L, "write");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_writeString, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "writeLine");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_writeLine, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "flush");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_flush, 1);
            lua_settable(L, -3);
        } else if (strcmp(mode, "rb") == 0) {
            lua_pushstring(L, "read");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_readByte, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "readAll");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_readAllByte, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "readLine");
            lua_pushlightuserdata(L, fp);
            lua_pushboolean(L, true);
            lua_pushcclosure(L, fs_handle_readLine, 2);
            lua_settable(L, -3);

            lua_pushstring(L, "seek");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_seek, 1);
            lua_settable(L, -3);
        } else if (strcmp(mode, "wb") == 0 || strcmp(mode, "ab") == 0) {
            lua_pushstring(L, "write");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_writeByte, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "flush");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_flush, 1);
            lua_settable(L, -3);

            lua_pushstring(L, "seek");
            lua_pushlightuserdata(L, fp);
            lua_pushcclosure(L, fs_handle_seek, 1);
            lua_settable(L, -3);
        } else {
            // This should now be unreachable, but we'll keep it here for safety
            fclose(fp);
            luaL_error(L, "%s: Unsupported mode", mode);
        }
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
    std::list<std::string> nextOptions;
    for (const std::string& opt : options) {
        struct_dirent *dir;
        path_t paths = fixpath(comp, opt.c_str(), true, true, NULL, true);
        if (paths.empty()) continue;
        std::vector<path_t> possible_paths = split(paths, '\n');
        for (const path_t& path : possible_paths) {
            if (std::regex_search(path, pathregex(WS("^\\d+:")))) {
                try {
                    const FileEntry &d = comp->virtualMounts[(unsigned)std::stoul(path.substr(0, path.find_first_of(':')))]->path(path.substr(path.find_first_of(':') + 1));
                    if (d.isDir) for (auto p : d.dir) if (std::regex_match(p.first, std::regex(replace_str(pathc_regex, "*", ".*")))) nextOptions.push_back(opt + (opt == "" ? "" : "/") + p.first);
                } catch (...) {continue;}
            } else {
                platform_DIR * d = platform_opendir(path.c_str());
                if (d) {
                    for (int i = 0; (dir = platform_readdir(d)) != NULL; i++) {
                        int found = 0;
                        for (const path_t& ign : ignored_files)
                            if (pathcmp(dir->d_name, ign.c_str()) == 0) { i--; found = 1; }
                        if (found) continue;
                        if (std::regex_match(std::string(astr(dir->d_name)), std::regex(replace_str(pathc_regex, "*", ".*")))) nextOptions.push_back(opt + (opt.empty() ? "" : "/") + std::string(astr(dir->d_name)));
                    }
                    platform_closedir(d);
                }
            }
        }
        for (const std::string& value : getMounts(comp, opt.c_str())) 
            if (*pathc == "*" || value == *pathc) nextOptions.push_back(opt + (opt.empty() ? "" : "/") + value);
    }
    if (++pathc == end) return nextOptions;
    else return matchWildcard(comp, nextOptions, pathc, end);
}

static int fs_find(lua_State *L) {
    lastCFunction = __func__;
    std::vector<std::string> elems = split(luaL_checkstring(L, 1), '/');
    std::list<std::string> pathc;
    for (const std::string& s : elems) {
        if (s == "..") { 
            if (pathc.empty()) luaL_error(L, "Not a directory");
            else pathc.pop_back(); 
        }
        else if (s != "." && !s.empty()) pathc.push_back(s);
    }
    while (!pathc.empty() && pathc.front().empty()) pathc.pop_front();
    while (!pathc.empty() && pathc.back().empty()) pathc.pop_back();
    if (pathc.empty()) {
        lua_newtable(L);
        lua_pushstring(L, "");
        lua_rawseti(L, -2, 1);
        return 1;
    }
    std::list<std::string> matches = matchWildcard(get_comp(L), {""}, pathc.begin(), pathc.end());
    lua_newtable(L);
    lua_Integer i = 0;
    for (const std::string& m : matches) {
        lua_pushinteger(L, ++i);
        lua_pushstring(L, m.c_str());
        lua_settable(L, -3);
    }
    lua_getglobal(L, "table");
    lua_pushstring(L, "sort");
    lua_gettable(L, -2);
    lua_pushvalue(L, -3);
    // L: [path, retval, table api, table.sort, retval]
    lua_pcall(L, 1, 0, 0);
    // L: [path, retval, table api]
    lua_pop(L, 1);
    return 1;
}

static int fs_getDir(lua_State *L) {
    lastCFunction = __func__;
    if (strcmp(luaL_checkstring(L, 1), "/") == 0 || strcmp(lua_tostring(L, 1), "") == 0) {
        lua_pushstring(L, "..");
        return 1;
    }
    const std::unique_ptr<char[]> path(new char[lua_strlen(L, 1) + 1]);
    strcpy(path.get(), lua_tostring(L, 1));
    if (strrchr(path.get(), '/') <= path.get()) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, dirname(path[0] == '/' ? &path[1] : path.get()));
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
    const path_t path = fixpath(get_comp(L), luaL_checkstring(L, 1), true);
    if (path.empty()) err(L, 1, "No such file");
    if (std::regex_search(path, pathregex(WS("^\\d+:")))) {
        try {
            const FileEntry &d = get_comp(L)->virtualMounts[(unsigned)std::stoul(path.substr(0, path.find_first_of(':')))]->path(path.substr(path.find_first_of(':') + 1));
            lua_newtable(L);
            lua_pushinteger(L, 0);
            lua_setfield(L, -2, "access");
            lua_pushinteger(L, 0);
            lua_setfield(L, -2, "modification");
            lua_pushinteger(L, 0);
            lua_setfield(L, -2, "created");
            lua_pushinteger(L, d.isDir ? 0 : d.data.length());
            lua_setfield(L, -2, "size");
            lua_pushboolean(L, d.isDir);
            lua_setfield(L, -2, "isDir");
        } catch (...) {err(L, 1, "No such file");}
    } else {
        struct_stat st;
        if (platform_stat(path.c_str(), &st) != 0) {
            lua_pushnil(L);
            return 1;
        }
        lua_newtable(L);
        lua_pushinteger(L, st_time_ms(st.st_a));
        lua_setfield(L, -2, "access");
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
    }
    return 1;
}

static int fs_getCapacity(lua_State *L) {
    lastCFunction = __func__;
    std::string mountPath;
    const path_t path = fixpath(get_comp(L), luaL_checkstring(L, 1), false, true, &mountPath);
    if (mountPath == "rom") {
        lua_pushnil(L);
        return 1;
    }
    if (path.empty()) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
    lua_pushinteger(L, getCapacity(path));
    return 1;
}

static int fs_isDriveRoot(lua_State *L) {
    lastCFunction = __func__;
    bool res = false;
    fixpath(get_comp(L), luaL_checkstring(L, 1), false, true, NULL, false, &res);
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