/*
 * apis/handles/fs_handle.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for file handles.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "fs_handle.hpp"
#include "../../util.hpp"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include "../../runtime.hpp"
#endif

#ifdef __EMSCRIPTEN__
EM_JS(void, emsyncfs, (), {
    if (window.fsIsSyncing) return;
    window.fsIsSyncing = true;
    FS.syncfs(false, function(err) {
        window.fsIsSyncing = false;
        if (err != null) console.log('Error while syncing filesystem: ', err);
    });
})
#endif

int fs_handle_close(lua_State *L) {
    lastCFunction = __func__;
    std::iostream ** fp = (std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (*fp == NULL)
        return luaL_error(L, "attempt to use a closed file");
    if (dynamic_cast<std::fstream*>(*fp) != NULL) delete (std::fstream*)*fp;
    else if (dynamic_cast<std::stringstream*>(*fp) != NULL) delete (std::stringstream*)*fp;
    else delete *fp;
    *fp = NULL;
    get_comp(L)->files_open--;
#ifdef __EMSCRIPTEN__
    queueTask([](void*)->void*{emsyncfs(); return NULL;}, NULL, true);
#endif
    return 0;
}

int fs_handle_gc(lua_State *L) {
    lastCFunction = __func__;
    std::iostream ** fp = (std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (*fp == NULL)
        return 0;
    if (dynamic_cast<std::fstream*>(*fp) != NULL) delete (std::fstream*)*fp;
    else if (dynamic_cast<std::stringstream*>(*fp) != NULL) delete (std::stringstream*)*fp;
    else delete *fp;
    *fp = NULL;
    get_comp(L)->files_open--;
#ifdef __EMSCRIPTEN__
    queueTask([](void*)->void*{emsyncfs(); return NULL;}, NULL, true);
#endif
    return 0;
}

int fs_handle_readAll(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    if (fp->eof()) {
        if (fp->tellg() < 1) return 0;
        lua_pushliteral(L, "");
        return 1;
    }
    if (fp->bad() || fp->fail()) return 0;
    const long pos = (long)fp->tellg();
    fp->seekg(0, std::ios::end);
    if (fp->bad() || fp->fail()) return 0;
    long size = (long)fp->tellg() - pos;
    char * retval = new char[size + 1];
    memset(retval, 0, size + 1);
    fp->seekg(pos);
    int i;
    for (i = 0; !fp->eof() && i < size; i++) {
        int c = fp->get();
        if (c == EOF && fp->eof()) { size = i; break; }
        if (c == '\n' && (i > 0 && retval[i-1] == '\r')) retval[--i] = '\n';
        else retval[i] = (char)c;
    }
    const std::string out = makeASCIISafe(retval, i - (i == size ? 0 : 1));
    delete[] retval;
    lua_pushlstring(L, out.c_str(), out.length());
    return 1;
}

int fs_handle_readLine(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    if (fp->eof()) return 0;
    if (!fp->good()) return luaL_error(L, "Could not read file");
    std::string retval;
    std::getline(*fp, retval);
    if (retval.empty() && fp->eof()) return 0;
    if (lua_toboolean(L, 1) && fp->good()) retval += '\n';
    else if (!retval.empty() && retval[retval.size()-1] == '\r') retval.resize(retval.size() - 1);
    lua_pushlstring(L, retval.c_str(), retval.length());
    return 1;
}

int fs_handle_readChar(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    if (fp->eof()) return 0;
    if (!fp->good()) luaL_error(L, "Could not read file");
    if (lua_isnumber(L, 1)) {
        if (lua_tointeger(L, 1) < 0) luaL_error(L, "Cannot read a negative number of characters");
        const size_t s = lua_tointeger(L, 1);
        if (s == 0) {
            if (fp->peek() == EOF || fp->eof()) return 0;
            lua_pushstring(L, "");
            return 1;
        }
        char* retval = new char[s];
        fp->read(retval, s);
        const size_t actual = fp->gcount();
        if (actual == 0) {delete[] retval; return 0;}
        lua_pushlstring(L, retval, actual);
        delete[] retval;
    } else {
        const int retval = fp->get();
        if (retval == EOF || fp->eof()) return 0;
        lua_pushlstring(L, (const char *)&retval, 1);
    }
    return 1;
}

int fs_handle_readByte(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    if (fp->eof()) return 0;
    if (!fp->good()) return luaL_error(L, "Could not read file");
    if (lua_isnumber(L, 1)) {
        if (lua_tointeger(L, 1) < 0) luaL_error(L, "Cannot read a negative number of bytes");
        const size_t s = lua_tointeger(L, 1);
        if (s == 0) {
            if (fp->peek() == EOF || fp->eof()) return 0;
            lua_pushstring(L, "");
            return 1;
        }
        char* retval = new char[s];
        fp->read(retval, s);
        const size_t actual = fp->gcount();
        if (actual == 0) {delete[] retval; return 0;}
        lua_pushlstring(L, retval, actual);
        delete[] retval;
    } else {
        const int retval = fp->get();
        if (retval == EOF || fp->eof()) return 0;
        lua_pushinteger(L, (unsigned char)retval);
    }
    return 1;
}

int fs_handle_readAllByte(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    if (fp->eof()) {
        lua_pushliteral(L, "");
        return 1;
    }
    if (fp->bad() || fp->fail()) return luaL_error(L, "Could not read file");
    std::streampos pos = fp->tellg();
    fp->seekg(0, std::ios_base::end);
    size_t size = fp->tellg() - pos;
    fp->seekg(pos, std::ios_base::beg);
    char * str = (char*)malloc(size);
    if (str == NULL) return luaL_error(L, "failed to allocate memory");
    fp->read(str, size);
    size = fp->gcount();
    fp->setstate(std::ios::eofbit); // set EOF flag
    lua_pushlstring(L, str, size);
    free(str);
    return 1;
}

int fs_handle_writeString(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    if (lua_isnoneornil(L, 1)) return 0;
    else if (!lua_isstring(L, 1) && !lua_isnumber(L, 1)) return luaL_error(L, "bad argument #1 (string expected, got %s)", lua_typename(L, lua_type(L, 1)));
    if (fp->eof()) {
        fp->seekp(0, std::ios::end);
        fp->clear(fp->rdstate() & ~std::ios::eofbit);
    }
    if (!fp->good()) return luaL_error(L, "Could not write file");
    size_t sz = 0;
    const char * str = lua_tolstring(L, 1, &sz);
    fp->write(str, sz);
    return 0;
}

int fs_handle_writeLine(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    if (lua_isnoneornil(L, 1)) return 0;
    else if (!lua_isstring(L, 1) && !lua_isnumber(L, 1)) return luaL_error(L, "bad argument #1 (string expected, got %s)", lua_typename(L, lua_type(L, 1)));
    if (fp->eof()) {
        fp->seekp(0, std::ios::end);
        fp->clear(fp->rdstate() & ~std::ios::eofbit);
    }
    if (!fp->good()) return luaL_error(L, "Could not write file");
    size_t sz = 0;
    const char * str = lua_tolstring(L, 1, &sz);
    fp->write(str, sz);
    fp->put('\n');
    return 0;
}

int fs_handle_writeByte(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    if (fp->eof()) {
        fp->seekp(0, std::ios::end);
        fp->clear(fp->rdstate() & ~std::ios::eofbit);
    }
    if (!fp->good()) return luaL_error(L, "Could not write file");
    if (lua_type(L, 1) == LUA_TNUMBER) {
        const char b = (unsigned char)(lua_tointeger(L, 1) & 0xFF);
        fp->put(b);
    } else if (lua_isstring(L, 1)) {
        size_t sz = 0;
        const char * str = lua_tolstring(L, 1, &sz);
        if (sz == 0) return 0;
        fp->write(str, sz);
    } else return luaL_error(L, "bad argument #1 (number or string expected, got %s)", lua_typename(L, lua_type(L, 1)));
    return 0;
}

int fs_handle_flush(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    fp->flush();
#ifdef __EMSCRIPTEN__
    queueTask([](void*)->void*{emsyncfs(); return NULL;}, NULL, true);
#endif
    return 0;
}

int fs_handle_seek(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    const char * whence = luaL_optstring(L, 1, "cur");
    const size_t offset = luaL_optinteger(L, 2, 0);
    if (strcmp(whence, "set") == 0 && luaL_optinteger(L, 2, 0) < 0) {
        lua_pushnil(L);
        lua_pushliteral(L, "Position is negative");
        return 2;
    }
    std::ios::seekdir origin;
    if (strcmp(whence, "set") == 0) origin = std::ios::beg;
    else if (strcmp(whence, "cur") == 0) origin = std::ios::cur;
    else if (strcmp(whence, "end") == 0) origin = std::ios::end;
    else return luaL_error(L, "bad argument #1 to 'seek' (invalid option '%s')", whence);
    fp->clear();
    fp->seekg(offset, origin);
    if (fp->fail()) {
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        return 2;
    }
    lua_pushinteger(L, fp->tellg());
    return 1;
}
