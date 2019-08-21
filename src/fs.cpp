/*
 * fs.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the fs API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "fs.hpp"
#include "fs_handle.hpp"
#include "platform.hpp"
#include "mounter.hpp"
#include "config.hpp"
extern "C" {
#include <lauxlib.h>
}
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
#ifdef WIN32
#include <io.h>
#define access(p, m) _access(p, m)
#define E_OK 0x00
#define W_OK 0x02
#define R_OK 0x04
#define RW_OK 0x06
#else
#include <unistd.h>
#include <libgen.h>
#endif

extern void injectMounts(lua_State *L, const char * comp_path, int idx);

void err(lua_State *L, char * path, const char * err) {
    char * msg = (char*)malloc(strlen(path) + strlen(err) + 3);
    sprintf(msg, "%s: %s", path, err);
    free(path);
    lua_pushstring(L, msg);
    free(msg);
    lua_error(L);
}

const char * ignored_files[4] = {
    ".",
    "..",
    ".DS_Store",
    "desktop.ini"
};

int fs_list(lua_State *L) {
    struct dirent *dir;
    char * path = fixpath(get_comp(L), lua_tostring(L, 1));
	if (path == NULL) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
    DIR * d = opendir(path);
    if (d) {
        lua_newtable(L);
        int i;
        for (i = 0; (dir = readdir(d)) != NULL; i++) {
            int found = 0;
            for (int j = 0; j < sizeof(ignored_files) / sizeof(const char *); j++) 
                if (strcmp(dir->d_name, ignored_files[j]) == 0) { i--; found = 1; }
            if (found) continue;
            lua_pushinteger(L, i + 1);
            lua_pushstring(L, dir->d_name);
            lua_settable(L, -3);
        }
        closedir(d);
        injectMounts(L, lua_tostring(L, 1), i);
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
    } else err(L, path, "Not a directory");
    free(path);
    return 1;
}

int fs_exists(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    char * path = fixpath(get_comp(L), lua_tostring(L, 1));
	if (path == NULL) {
		lua_pushboolean(L, false);
		return 1;
	}
    struct stat st;
    lua_pushboolean(L, stat(path, &st) == 0);
    free(path);
    return 1;
}

int fs_isDir(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    char * path = fixpath(get_comp(L), lua_tostring(L, 1));
	if (path == NULL) {
		lua_pushboolean(L, false);
		return 1;
	}
    struct stat st;
    lua_pushboolean(L, stat(path, &st) == 0 && S_ISDIR(st.st_mode));
    free(path);
    return 1;
}

int fs_isReadOnly(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (fixpath_ro(get_comp(L), lua_tostring(L, 1))) {
        lua_pushboolean(L, true);
        return 1;
    }
    char * path = fixpath(get_comp(L), lua_tostring(L, 1));
	if (path == NULL) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
	struct stat st;
    if (stat(path, &st) != 0) lua_pushboolean(L, false);
#ifdef WIN32
    else if (S_ISDIR(st.st_mode)) {
        char * file = (char*)malloc(strlen(path) + 3);
        strcpy(file, path);
        strcat(file, "\\a");
        FILE * fp = fopen(file, "a");
        lua_pushboolean(L, fp == NULL);
        if (fp != NULL) fclose(fp);
        if (stat(file, &st) == 0) remove(file);
        free(file);
    }
#endif
	else lua_pushboolean(L, access(path, W_OK) != 0);
    free(path);
    return 1;
}

int fs_getName(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    char * path = (char*)malloc(lua_strlen(L, 1) + 1);
    strcpy(path, lua_tostring(L, 1));
    lua_pushstring(L, basename(path));
    free(path);
    return 1;
}

int fs_getDrive(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * path = lua_tostring(L, 1);
    // basic mountpoint check, will remove when adding mounter
	int s = path[0] == '/';
    if (path[s] == 'r' && path[s+1] == 'o' && path[s+2] == 'm' && path[s+3] == '/')
        lua_pushstring(L, "rom");
    else lua_pushstring(L, "hdd");
    return 1;
}

int fs_getSize(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    char * path = fixpath(get_comp(L), lua_tostring(L, 1));
	if (path == NULL) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
    struct stat st;
    if (stat(path, &st) != 0) err(L, path, "No such file");
    lua_pushinteger(L, st.st_size);
    free(path);
    return 1;
}

int fs_getFreeSpace(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    char * path = fixpath(get_comp(L), lua_tostring(L, 1));
	if (path == NULL) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
	lua_pushinteger(L, getFreeSpace(path));
    free(path);
    return 1;
}

int fs_makeDir(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    char * path = fixpath(get_comp(L), lua_tostring(L, 1));
	if (path == NULL) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
    if (createDirectory(path) != 0 && errno != EEXIST) err(L, path, strerror(errno));
    free(path);
    return 0;
}

int fs_move(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    char * fromPath = fixpath(get_comp(L), lua_tostring(L, 1));
    char * toPath = fixpath(get_comp(L), lua_tostring(L, 2));
	if (fromPath == NULL) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
	if (toPath == NULL) luaL_error(L, "%s: Invalid path", lua_tostring(L, 2));
    if (rename(fromPath, toPath) != 0) {
        free(toPath);
        err(L, fromPath, strerror(errno));
    }
    free(fromPath);
    free(toPath);
    return 0;
}

int fs_copy(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    char * fromPath = fixpath(get_comp(L), lua_tostring(L, 1));
    char * toPath = fixpath(get_comp(L), lua_tostring(L, 2));
	if (fromPath == NULL) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
	if (toPath == NULL) luaL_error(L, "%s: Invalid path", lua_tostring(L, 2));
	FILE * fromfp = fopen(fromPath, "r");
    if (fromfp == NULL) {
        free(toPath);
        err(L, fromPath, "Cannot read file");
    }
    FILE * tofp = fopen(toPath, "w");
    if (tofp == NULL) {
        free(fromPath);
        fclose(fromfp);
        err(L, toPath, "Cannot write file");
    }

    char tmp[1024];
    while (!feof(fromfp)) {
        int read = fread(tmp, 1, 1024, fromfp);
        fwrite(tmp, read, 1, tofp);
        if (read < 1024) break;
    }

    fclose(fromfp);
    fclose(tofp);
    free(fromPath);
    free(toPath);
    return 0;
}

int fs_delete(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    char * path = fixpath(get_comp(L), lua_tostring(L, 1));
	if (path == NULL) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
	int res = removeDirectory(path);
	if (res != 0) err(L, path, "Failed to remove");
    free(path);
    return 0;
}

int fs_combine(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    const char * basePath = lua_tostring(L, 1);
    const char * localPathOrig = lua_tostring(L, 2);
    char * localPath = (char*)malloc(strlen(localPathOrig) + 2);
    if (localPathOrig[0] != '/') {
        localPath[0] = '/';
        strcpy(&localPath[1], localPathOrig);
    } else strcpy(localPath, localPathOrig);
    if (basePath[0] == '/') basePath = basePath + 1;
    if (basePath[strlen(basePath)-1] == '/') localPath = localPath + 1;
    char * retval = (char*)malloc(strlen(basePath) + strlen(localPath) + 1);
    strcpy(retval, basePath);
    strcat(retval, localPath);
    if (basePath[strlen(basePath)-1] == '/') localPath = localPath - 1;
    free(localPath);
    char * realRetval = fixpath(get_comp(L), retval, false);
    free(retval);
    lua_pushstring(L, realRetval);
    free(realRetval);
    return 1;
}

int fs_open(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    Computer * computer = get_comp(L);
    char * path = fixpath(get_comp(L), lua_tostring(L, 1));
	if (path == NULL) luaL_error(L, "%s: Invalid path", lua_tostring(L, 1));
    const char * mode = lua_tostring(L, 2);
    if (computer->files_open >= config.maximumFilesOpen) err(L, path, "Too many files open");
	//printf("fs.open(\"%s\", \"%s\")\n", path, mode);
	FILE * fp = fopen(path, mode);
	if (fp == NULL) { 
		free(path);
		lua_pushnil(L);
		lua_pushfstring(L, "%s: File does not exist", lua_tostring(L, 1));
		return 2; 
	}
    free(path);
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
        lua_pushcclosure(L, fs_handle_readLine, 1);
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
        char * tmp = (char*)malloc(strlen(mode) + 1);
        strcpy(tmp, mode);
        err(L, tmp, "Invalid mode");
    }
    computer->files_open++;
    return 1;
}

extern std::vector<std::string> split(std::string strToSplit, char delimeter);

std::list<std::string> matchWildcard(lua_State *L, std::list<std::string> options, std::list<std::string>::iterator pathc, std::list<std::string>::iterator end) {
    if (pathc == end) return {};
    std::list<std::string> nextOptions;
    for (std::list<std::string>::iterator it = options.begin(); it != options.end(); it++) {
        struct dirent *dir;
        char * path = fixpath(get_comp(L), it->c_str());
        if (path == NULL) continue;
        DIR * d = opendir(path);
        if (d) {
            int i;
            for (i = 0; (dir = readdir(d)) != NULL; i++) {
                int found = 0;
                for (int j = 0; j < sizeof(ignored_files) / sizeof(const char *); j++)
                    if (strcmp(dir->d_name, ignored_files[j]) == 0) { i--; found = 1; }
                if (found) continue;
                if (*pathc == "*" || std::string(dir->d_name) == *pathc) nextOptions.push_back(*it + (*it == "" ? "" : "/") + std::string(dir->d_name));
            }
            closedir(d);
            lua_newtable(L);
            injectMounts(L, it->c_str(), 1);
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                lua_pushvalue(L, -2);
                const char *value = lua_tostring(L, -2);
                if (*pathc == "*" || std::string(value) == *pathc) nextOptions.push_back(*it + (*it == "" ? "" : "/") + std::string(value));
                lua_pop(L, 2);
            }
            lua_pop(L, 1);
        }
        free(path);
    }
    if (++pathc == end) return nextOptions;
    else return matchWildcard(L, nextOptions, pathc, end);
}

int fs_find(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::vector<std::string> elems = split(lua_tostring(L, 1), '/');
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
    lua_State *tmp = lua_newthread(L);
    std::list<std::string> matches = matchWildcard(tmp, {""}, pathc.begin(), pathc.end());
    lua_pop(L, 1);
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
    char * path = (char*)malloc(lua_strlen(L, 1) + 1);
    strcpy(path, lua_tostring(L, 1));
    lua_pushstring(L, dirname(path[0] == '/' ? &path[1] : path));
    free(path);
    return 1;
}

const char * fs_keys[16] = {
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
    "getDir"
};

lua_CFunction fs_values[16] = {
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
    fs_getDir
};

library_t fs_lib = {"fs", 16, fs_keys, fs_values, NULL, NULL};