/*
 * fs.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the fs API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#define CRAFTOSPC_INTERNAL
#include "fs.hpp"
#include "fs_handle.hpp"
#include "fs_standalone.hpp"
#include "platform.hpp"
#include "mounter.hpp"
#include "config.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <stdbool.h>
#include <assert.h>
#include <sstream>
#include <iterator>
#include <regex>
#ifdef WIN32
#include <io.h>
#define access(p, m) _access(p, m)
#define E_OK 0x00
#define W_OK 0x02
#define R_OK 0x04
#define RW_OK 0x06
#define PATH_SEP "\\"
#define PATH_SEPC '\\'
#else
#include <unistd.h>
#include <libgen.h>
#define PATH_SEP "/"
#define PATH_SEPC '/'
#endif
#if defined(__INTELLISENSE__) && !defined(S_ISDIR)
#define S_ISDIR(m) 1 // silence errors in IntelliSense (which isn't very intelligent for its name)
#define W_OK 2
#endif

extern std::set<std::string> getMounts(Computer * computer, const char * comp_path);

void err(lua_State *L, int idx, const char * err) {
    luaL_error(L, "/%s: %s", fixpath(get_comp(L), lua_tostring(L, idx), false, false).c_str(), err);
}

std::string concat(std::list<std::string> &c, char sep) {
    std::stringstream ss;
    bool started = false;
    for (std::string s : c) {
        if (started) ss << sep;
        ss << s;
        started = true;
    }
    return ss.str();
}

std::list<std::string> split_list(std::string strToSplit, char delimeter) {
    std::stringstream ss(strToSplit);
    std::string item;
    std::list<std::string> splittedStrings;
    while (std::getline(ss, item, delimeter)) {
        splittedStrings.push_back(item);
    }
    return splittedStrings;
}

std::string fixpath_mkdir(Computer * comp, std::string path, bool md = true, std::string * mountPath = NULL) {
    if (md && fixpath_ro(comp, path.c_str())) return std::string();
    std::list<std::string> components = path.find("/") != std::string::npos ? split_list(path, '/') : split_list(path, '\\');
    while (components.size() > 0 && components.front().empty()) components.pop_front();
    if (components.empty()) return fixpath(comp, "", true);
    components.pop_back();
    std::list<std::string> append;
    std::string maxPath = fixpath(comp, concat(components, '/').c_str(), false, true, mountPath);
    while (maxPath.empty()) {
        append.push_back(components.back());
        components.pop_back();
        if (components.empty()) return std::string();
        maxPath = fixpath(comp, concat(components, '/').c_str(), false, true, mountPath);
    }
    if (!md) return maxPath;
    if (createDirectory(maxPath + PATH_SEP + concat(append, PATH_SEPC)) != 0) return std::string();
    return fixpath(comp, path.c_str(), false, true, mountPath);
}

const char * ignored_files[4] = {
    ".",
    "..",
    ".DS_Store",
    "desktop.ini"
};

int fs_list(lua_State *L) {
    struct dirent *dir;
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::string paths = fixpath(get_comp(L), lua_tostring(L, 1), true, true, NULL, true);
	if (paths.empty()) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
    std::vector<std::string> possible_paths = split(paths, '\n');
    bool gotdir = false;
    std::set<std::string> entries;
    for (std::string path : possible_paths) {
#ifdef STANDALONE_ROM
        if (path.substr(0, 4) == "rom:" || path.substr(0, 6) == "debug:") {
            try {
                FileEntry &d = (path.substr(0, 4) == "rom:" ? standaloneROM : standaloneDebug).path(path);
                gotdir = true;
                if (d.isDir) for (auto p : d.dir) entries.insert(p.first);
                else gotdir = false;
            } catch (std::exception &e) {continue;}
        } else {
#endif
        DIR * d = opendir(path.c_str());
        if (d) {
            gotdir = true;
            while ((dir = readdir(d)) != NULL) {
                bool found = false;
                for (unsigned j = 0; j < sizeof(ignored_files) / sizeof(const char *); j++) 
                    if (strcmp(dir->d_name, ignored_files[j]) == 0) found = true;
                if (!found) entries.insert(std::string(dir->d_name));
            }
            closedir(d);
        }
#ifdef STANDALONE_ROM
        }
#endif
    }
    if (!gotdir) err(L, 1, "Not a directory");
    std::set<std::string> mounts = getMounts(get_comp(L), lua_tostring(L, 1));
    std::set<std::string> all;
    std::set_union(entries.begin(), entries.end(), mounts.begin(), mounts.end(), std::inserter(all, all.begin()));
    int i = 1;
    lua_newtable(L);
    for (auto it = all.begin(); it != all.end(); it++) {
        lua_pushinteger(L, i++);
        lua_pushstring(L, it->c_str());
        lua_settable(L, -3);
    }
    return 1;
}

int fs_exists(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::string path = fixpath(get_comp(L), lua_tostring(L, 1), true);
#ifdef STANDALONE_ROM
    if (path.substr(0, 4) == "rom:" || path.substr(0, 6) == "debug:") {
        bool found = true;
        try {(path.substr(0, 4) == "rom:" ? standaloneROM : standaloneDebug).path(path);} catch (std::exception &e) {found = false;}
        lua_pushboolean(L, found);
    } else if (path == ":bios.lua") {
        lua_pushboolean(L, true);
    } else {
#endif
	lua_pushboolean(L, !path.empty());
#ifdef STANDALONE_ROM
    }
#endif
    return 1;
}

int fs_isDir(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::string path = fixpath(get_comp(L), lua_tostring(L, 1), true);
	if (path.empty()) {
		lua_pushboolean(L, false);
		return 1;
	}
#ifdef STANDALONE_ROM
    if (path.substr(0, 4) == "rom:" || path.substr(0, 6) == "debug:") {
        try {lua_pushboolean(L, (path.substr(0, 4) == "rom:" ? standaloneROM : standaloneDebug).path(path).isDir);} 
        catch (std::exception &e) {lua_pushboolean(L, false);}
    } else {
#endif
    struct stat st;
    lua_pushboolean(L, stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
#ifdef STANDALONE_ROM
    }
#endif
    return 1;
}

int fs_isReadOnly(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (fixpath_ro(get_comp(L), lua_tostring(L, 1))) {
        lua_pushboolean(L, true);
        return 1;
    }
    std::string path = fixpath_mkdir(get_comp(L), lua_tostring(L, 1), false);
    if (path.empty()) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
	struct stat st;
    if (stat(path.c_str(), &st) != 0) lua_pushboolean(L, false);
#ifdef WIN32
    else if (S_ISDIR(st.st_mode)) {
        std::string file = path + "\\a";
        FILE * fp = fopen(file.c_str(), "a");
        lua_pushboolean(L, fp == NULL);
        if (fp != NULL) fclose(fp);
        if (stat(file.c_str(), &st) == 0) remove(file.c_str());
    }
#endif
	else lua_pushboolean(L, access(path.c_str(), W_OK) != 0);
    return 1;
}

int fs_getName(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    char * path = new char[lua_strlen(L, 1) + 1];
    strcpy(path, lua_tostring(L, 1));
    lua_pushstring(L, basename(path));
    delete[] path;
    return 1;
}

int fs_getDrive(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::string retval;
    fixpath_mkdir(get_comp(L), std::string(lua_tostring(L, 1)) + "/a", false, &retval);
    lua_pushstring(L, retval.c_str());
    return 1;
}

int fs_getSize(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::string path = fixpath(get_comp(L), lua_tostring(L, 1), true);
	if (path.empty()) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
#ifdef STANDALONE_ROM
    if (path.substr(0, 4) == "rom:" || path.substr(0, 6) == "debug:") {
        try {
            FileEntry &d = (path.substr(0, 4) == "rom:" ? standaloneROM : standaloneDebug).path(path);
            if (d.isDir) err(L, 1, "Is a directory");
            lua_pushinteger(L, d.data.size());
        } catch (std::exception &e) {err(L, 1, "No such file");}
    } else if (path == ":bios.lua") {
        lua_pushinteger(L, standaloneBIOS.size());
    } else {
#endif
    struct stat st;
    if (stat(path.c_str(), &st) != 0) err(L, 1, "No such file");
    else if (S_ISDIR(st.st_mode)) err(L, 1, "Is a directory");
    lua_pushinteger(L, st.st_size);
#ifdef STANDALONE_ROM
    }
#endif
    return 1;
}

int fs_getFreeSpace(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::string path = fixpath(get_comp(L), lua_tostring(L, 1), false);
	if (path.empty()) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
    if (fixpath_ro(get_comp(L), lua_tostring(L, 1))) lua_pushinteger(L, 0);
	else lua_pushinteger(L, getFreeSpace(path));
    return 1;
}

int fs_makeDir(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (fixpath_ro(get_comp(L), lua_tostring(L, 1))) luaL_error(L, "/%s: Access denied", fixpath(get_comp(L), lua_tostring(L, 1), false, false).c_str());
    std::string path = fixpath_mkdir(get_comp(L), lua_tostring(L, 1));
	if (path.empty()) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && !S_ISDIR(st.st_mode)) luaL_error(L, "/%s: File exists", fixpath(get_comp(L), lua_tostring(L, 1), false, false).c_str());
    if (createDirectory(path) != 0 && errno != EEXIST) err(L,1, strerror(errno));
    return 0;
}

int fs_move(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    if (fixpath_ro(get_comp(L), lua_tostring(L, 1))) luaL_error(L, "Access denied");
    if (fixpath_ro(get_comp(L), lua_tostring(L, 2))) luaL_error(L, "Access denied");
    std::string fromPath = fixpath(get_comp(L), lua_tostring(L, 1), true);
    std::string toPath = fixpath_mkdir(get_comp(L), lua_tostring(L, 2));
	if (fromPath.empty()) err(L, 1, "Invalid path");
	if (toPath.empty()) err(L, 2, "Invalid path");
    if (rename(fromPath.c_str(), toPath.c_str()) != 0) err(L, 1, strerror(errno));
    return 0;
}

int fs_copy(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    if (fixpath_ro(get_comp(L), lua_tostring(L, 2))) luaL_error(L, "/%s: Access denied", fixpath(get_comp(L), lua_tostring(L, 2), false, false).c_str());
    std::string fromPath = fixpath(get_comp(L), lua_tostring(L, 1), true);
    std::string toPath = fixpath_mkdir(get_comp(L), lua_tostring(L, 2));
	if (fromPath.empty()) err(L, 1, "Invalid path");
	if (toPath.empty()) err(L, 2, "Invalid path");
#ifdef STANDALONE_ROM
    if (fromPath.substr(0, 4) == "rom:" || fromPath.substr(0, 6) == "debug:") {
        try {
            FileEntry &d = (fromPath.substr(0, 4) == "rom:" ? standaloneROM : standaloneDebug).path(fromPath);
            if (d.isDir) err(L, 1, "Is a directory");
            FILE * tofp = fopen(toPath.c_str(), "w");
            if (tofp == NULL) err(L, 2, "Cannot write file");
            fwrite(d.data.c_str(), d.data.size(), 1, tofp);
            fclose(tofp);
        } catch (std::exception &e) {err(L, 1, "No such file");}
    } else {
#endif
	FILE * fromfp = fopen(fromPath.c_str(), "r");
    if (fromfp == NULL) err(L, 1, "Cannot read file");
    FILE * tofp = fopen(toPath.c_str(), "w");
    if (tofp == NULL) {
        fclose(fromfp);
        err(L, 2, "Cannot write file");
    }

    char tmp[1024];
    while (!feof(fromfp)) {
        int read = fread(tmp, 1, 1024, fromfp);
        fwrite(tmp, read, 1, tofp);
        if (read < 1024) break;
    }

    fclose(fromfp);
    fclose(tofp);
#ifdef STANDALONE_ROM
    }
#endif
    return 0;
}

int fs_delete(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (fixpath_ro(get_comp(L), lua_tostring(L, 1))) luaL_error(L, "/%s: Access denied", fixpath(get_comp(L), lua_tostring(L, 1), false, false).c_str());
    std::string path = fixpath(get_comp(L), lua_tostring(L, 1), true);
	if (path.empty()) return 0;
	int res = removeDirectory(path);
	if (res != 0 && res != ENOENT) err(L, 1, "Failed to remove");
    return 0;
}

int fs_combine(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    std::string basePath = lua_tostring(L, 1);
    std::string localPath = lua_tostring(L, 2);
    lua_pushstring(L, fixpath(get_comp(L), (basePath + "/" + localPath).c_str(), false, false).c_str());
    return 1;
}

int fs_open(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    Computer * computer = get_comp(L);
    if (computer->files_open >= config.maximumFilesOpen) err(L, 1, "Too many files open");
    const char * mode = lua_tostring(L, 2);
    std::string path = mode[0] == 'r' ? fixpath(get_comp(L), lua_tostring(L, 1), true) : fixpath_mkdir(get_comp(L), lua_tostring(L, 1));
	if (path.empty()) {
        lua_pushnil(L);
        lua_pushfstring(L, "/%s: Invalid path", fixpath(computer, lua_tostring(L, 1), false, false).c_str());
        return 2;
    }
#ifdef STANDALONE_ROM
    if (path.substr(0, 4) == "rom:" || path.substr(0, 6) == "debug:" || path == ":bios.lua") {
        std::stringstream * fp;
        if (path == ":bios.lua") {
            fp = new std::stringstream(standaloneBIOS);
        } else {
            try {
                FileEntry &d = (path.substr(0, 4) == "rom:" ? standaloneROM : standaloneDebug).path(path);
                if (d.isDir) {
                    lua_pushnil(L);
                    if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) lua_pushfstring(L, "/%s: No such file", fixpath(computer, lua_tostring(L, 1), false, false).c_str());
                    else lua_pushfstring(L, "/%s: Cannot write to directory", fixpath(computer, lua_tostring(L, 1), false, false).c_str());
                    return 2; 
                }
                fp = new std::stringstream(d.data);
            } catch (std::exception &e) {
                lua_pushnil(L);
                lua_pushfstring(L, "/%s: No such file", fixpath(computer, lua_tostring(L, 1), false, false).c_str());
                return 2;
            }
        }
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
            lua_pushfstring(L, "/%s: Access denied", fixpath(computer, lua_tostring(L, 1), false, false).c_str());
            return 2; 
        }
    } else {
#endif
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) { 
		lua_pushnil(L);
        if (strcmp(mode, "r") == 0 || strcmp(mode, "rb") == 0) lua_pushfstring(L, "/%s: No such file", fixpath(computer, lua_tostring(L, 1), false, false).c_str());
        else lua_pushfstring(L, "/%s: Cannot write to directory", fixpath(computer, lua_tostring(L, 1), false, false).c_str());
		return 2; 
    }
    if (strcmp(mode, "w") == 0 || strcmp(mode, "a") == 0 || strcmp(mode, "wb") == 0 || strcmp(mode, "ab") == 0) {
        if (fixpath_ro(computer, lua_tostring(L, 1))) {
            lua_pushnil(L);
            lua_pushfstring(L, "/%s: Access denied", fixpath(computer, lua_tostring(L, 1), false, false).c_str());
            return 2; 
        }
#ifdef WIN32
        createDirectory(path.substr(0, path.find_last_of('\\')));
#else
        createDirectory(path.substr(0, path.find_last_of('/')));
#endif
    }
	FILE * fp = fopen(path.c_str(), mode);
	if (fp == NULL) { 
		lua_pushnil(L);
		lua_pushfstring(L, "/%s: No such file", fixpath(computer, lua_tostring(L, 1), false, false).c_str());
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
        lua_remove(L, -1);
		fclose(fp);
        err(L, 2, "Invalid mode");
    }
#ifdef STANDALONE_ROM
    }
#endif
    computer->files_open++;
    return 1;
}

std::string replace_str(std::string data, std::string toSearch, std::string replaceStr) {
	size_t pos = data.find(toSearch);
	while (pos != std::string::npos) {
		data.replace(pos, toSearch.size(), replaceStr);
		pos = data.find(toSearch, pos + replaceStr.size());
	}
    return data;
}

std::string regex_escape[] = {"\\", ".", "[", "]", "{", "}", "^", "$", "(", ")", "+", "?", "|"};

std::list<std::string> matchWildcard(Computer * comp, std::list<std::string> options, std::list<std::string>::iterator pathc, std::list<std::string>::iterator end) {
    if (pathc == end) return {};
    std::string pathc_regex = *pathc;
    for (std::string r : regex_escape) pathc_regex = replace_str(pathc_regex, r, "\\" + r);
    std::list<std::string> nextOptions;
    for (std::list<std::string>::iterator it = options.begin(); it != options.end(); it++) {
        struct dirent *dir;
        std::string paths = fixpath(comp, it->c_str(), true, true, NULL, true);
        if (paths.empty()) continue;
        std::vector<std::string> possible_paths = split(paths, '\n');
        for (std::string path : possible_paths) {
#ifdef STANDALONE_ROM
            if (path.substr(0, 4) == "rom:" || path.substr(0, 6) == "debug:") {
                try {
                    FileEntry &d = (path.substr(0, 4) == "rom:" ? standaloneROM : standaloneDebug).path(path);
                    if (d.isDir) for (auto p : d.dir) if (std::regex_match(p.first, std::regex(replace_str(pathc_regex, "*", ".*")))) nextOptions.push_back(*it + (*it == "" ? "" : "/") + p.first);
                } catch (std::exception &e) {continue;}
            } else {
#endif
            DIR * d = opendir(path.c_str());
            if (d) {
                int i;
                for (i = 0; (dir = readdir(d)) != NULL; i++) {
                    int found = 0;
                    for (unsigned j = 0; j < sizeof(ignored_files) / sizeof(const char *); j++)
                        if (strcmp(dir->d_name, ignored_files[j]) == 0) { i--; found = 1; }
                    if (found) continue;
                    if (std::regex_match(std::string(dir->d_name), std::regex(replace_str(pathc_regex, "*", ".*")))) nextOptions.push_back(*it + (*it == "" ? "" : "/") + std::string(dir->d_name));
                }
                closedir(d);
            }
#ifdef STANDALONE_ROM
            }
#endif
        }
        for (std::string value : getMounts(comp, it->c_str())) 
            if (*pathc == "*" || value == *pathc) nextOptions.push_back(*it + (*it == "" ? "" : "/") + value);
    }
    if (++pathc == end) return nextOptions;
    else return matchWildcard(comp, nextOptions, pathc, end);
}

int fs_find(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::vector<std::string> elems = split(lua_tostring(L, 1), '/');
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") { 
            if (pathc.size() < 1) luaL_error(L, "Not a directory");
            else pathc.pop_back(); 
        }
        else if (s != "." && s != "") pathc.push_back(s);
    }
    std::list<std::string> matches = matchWildcard(get_comp(L), {""}, pathc.begin(), pathc.end());
    lua_newtable(L);
    int i = 0;
    for (std::list<std::string>::iterator it = matches.begin(); it != matches.end(); it++, i++) {
        lua_pushinteger(L, i + 1);
        lua_pushstring(L, it->c_str());
        lua_settable(L, -3);
    }
    lua_getglobal(L, "table");
    assert(lua_istable(L, -1));
    lua_pushstring(L, "sort");
    lua_gettable(L, -2);
    assert(lua_isfunction(L, -1));
    lua_pushvalue(L, -3);
    // L: [path, retval, table api, table.sort, retval]
    assert(lua_pcall(L, 1, 0, 0) == 0);
    // L: [path, retval, table api]
    lua_pop(L, 1);
    return 1;
}

int fs_getDir(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (strcmp(lua_tostring(L, 1), "/") == 0 || strcmp(lua_tostring(L, 1), "") == 0) {
        lua_pushstring(L, "..");
        return 1;
    }
    std::unique_ptr<char[]> path(new char[lua_strlen(L, 1) + 1]);
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

int fs_attributes(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::string path = fixpath(get_comp(L), lua_tostring(L, 1), true);
    if (path.empty()) {
        lua_pushnil(L);
        return 1;
    }
#ifdef STANDALONE_ROM
    if (path.substr(0, 4) == "rom:" || path.substr(0, 6) == "debug:") {
        try {
            FileEntry &d = (path.substr(0, 4) == "rom:" ? standaloneROM : standaloneDebug).path(path);
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
        } catch (std::exception &e) {err(L, 1, "No such file");}
    } else {
#endif
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        lua_pushnil(L);
        return 1;
    }
    lua_newtable(L);
    lua_pushinteger(L, st_time_ms(st.st_a));
    lua_setfield(L, -2, "access");
    lua_pushinteger(L, st_time_ms(st.st_m));
    lua_setfield(L, -2, "modification");
    lua_pushinteger(L, st_time_ms(st.st_c));
    lua_setfield(L, -2, "created");
    lua_pushinteger(L, S_ISDIR(st.st_mode) ? 0 : st.st_size);
    lua_setfield(L, -2, "size");
    lua_pushboolean(L, S_ISDIR(st.st_mode));
    lua_setfield(L, -2, "isDir");
#ifdef STANDALONE_ROM
    }
#endif
    return 1;
}

int fs_getCapacity(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::string mountPath;
    std::string path = fixpath(get_comp(L), lua_tostring(L, 1), false, true, &mountPath);
    if (mountPath == "rom") {
        lua_pushnil(L);
        return 1;
    }
    if (path.empty()) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
    lua_pushinteger(L, getCapacity(path));
    return 1;
}

int fs_isDriveRoot(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    bool res = false;
    fixpath(get_comp(L), lua_tostring(L, 1), false, true, NULL, false, &res);
    lua_pushboolean(L, res);
    return 1;
}

const char * fs_keys[19] = {
    "list",
    "exists",
    "isDir",
    "isReadOnly",
    "getName",
    "getDrive",
    "getSize",
    "getFreeSpace",
    "makeDir",
    "move",
    "copy",
    "delete",
    "combine",
    "open",
    "find",
    "getDir",
    "getAttributes",
    "getCapacity",
    "isDriveRoot"
};

lua_CFunction fs_values[19] = {
    fs_list,
    fs_exists,
    fs_isDir,
    fs_isReadOnly,
    fs_getName,
    fs_getDrive,
    fs_getSize,
    fs_getFreeSpace,
    fs_makeDir,
    fs_move,
    fs_copy,
    fs_delete,
    fs_combine,
    fs_open,
    fs_find,
    fs_getDir,
    fs_attributes,
    fs_getCapacity,
    fs_isDriveRoot
};

library_t fs_lib = {"fs", 19, fs_keys, fs_values, nullptr, nullptr};