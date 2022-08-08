/*
 * apis/handles/fs_handle.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for file handles.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <codecvt>
#include <iostream>
#include <fstream>
#include <sstream>
#include <locale>
#include <string>
#include "fs_handle.hpp"
#include "../../util.hpp"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include "../../runtime.hpp"
#endif

#ifdef __EMSCRIPTEN__
EM_JS(void, syncfs, (), {
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
    queueTask([](void*)->void*{syncfs(); return NULL;}, NULL, true);
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
    queueTask([](void*)->void*{syncfs(); return NULL;}, NULL, true);
#endif
    return 0;
}

int fs_handle_readAll(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    if (fp->eof()) return 0;
    const long pos = (long)fp->tellg();
    fp->seekg(0, std::ios::end);
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
    if (fp->bad() || fp->eof()) return 0;
    std::string retval;
    std::getline(*fp, retval);
    if (retval.empty() && fp->eof()) return 0;
    size_t len = retval.length() - (retval[retval.length()-1] == '\n' && !lua_toboolean(L, 1));
    if (len > 0 && retval[len-1] == '\r') {if (lua_toboolean(L, 1)) {retval[len] = '\0'; retval[--len] = '\n';} else retval[--len] = '\0';}
    const std::string out = lua_toboolean(L, lua_upvalueindex(2)) ? std::string(retval, 0, len) : makeASCIISafe(retval.c_str(), len);
    lua_pushlstring(L, out.c_str(), out.length());
    return 1;
}

int fs_handle_readChar(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    if (fp->eof()) return 0;
    std::string retval;
    for (int i = 0; i < luaL_optinteger(L, 1, 1) && !fp->eof(); i++) {
        uint32_t codepoint;
        const int c = fp->get();
        if (c == EOF) break;
        else if (c > 0x7F) {
            if (c & 64) {
                const int c2 = fp->get();
                if (c2 == EOF) {retval += '?'; break;}
                else if (c2 < 0x80 || c2 & 64) codepoint = 1U<<31;
                else if (c & 32) {
                    const int c3 = fp->get();
                    if (c3 == EOF) {retval += '?'; break;}
                    else if (c3 < 0x80 || c3 & 64) codepoint = 1U<<31;
                    else if (c & 16) {
                        if (c & 8) codepoint = 1U<<31;
                        else {
                            const int c4 = fp->get();
                            if (c4 == EOF) {retval += '?'; break;}
                            else if (c4 < 0x80 || c4 & 64) codepoint = 1U<<31;
                            else codepoint = ((c & 0x7) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
                        }
                    } else codepoint = ((c & 0xF) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                } else codepoint = ((c & 0x1F) << 6) | (c2 & 0x3F);
            } else codepoint = 1U<<31;
        } else codepoint = (unsigned char)c;
        if (codepoint > 255) retval += '?';
        else {
            if (codepoint == '\r') {
                const int nextc = fp->get();
                if (nextc == '\n') codepoint = nextc;
                else fp->putback((char)nextc);
            }
            retval += (char)codepoint;
        }
    }
    lua_pushlstring(L, retval.c_str(), retval.length());
    return 1;
}

int fs_handle_readByte(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    if (fp->eof()) return 0;
    if (lua_isnumber(L, 1)) {
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
    if (fp->eof()) return 0;
    size_t size = 0;
    char * str = (char*)malloc(512);
    if (str == NULL) return luaL_error(L, "failed to allocate memory");
    while (!fp->eof()) {
        fp->read(&str[size], 512);
        size += fp->gcount();
        if (size % 512 != 0) break;
        char * strn = (char*)realloc(str, size + 512);
        if (strn == NULL) {
            free(str);
            return luaL_error(L, "failed to allocate memory");
        }
        str = strn;
    }
    lua_pushlstring(L, str, size);
    free(str);
    return 1;
}

int fs_handle_writeString(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) luaL_error(L, "attempt to use a closed file");
    if (lua_isnoneornil(L, 1)) return 0;
    else if (!lua_isstring(L, 1) && !lua_isnumber(L, 1)) luaL_typerror(L, 1, "string");
    std::string str(lua_tostring(L, 1), lua_strlen(L, 1));
    std::wstring wstr;
    for (unsigned char c : str) wstr += (wchar_t)c;
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t> > converter;
    const std::string newstr = converter.to_bytes(wstr);
    fp->write(newstr.c_str(), newstr.size());
    return 0;
}

int fs_handle_writeLine(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) luaL_error(L, "attempt to use a closed file");
    if (lua_isnoneornil(L, 1)) return 0;
    else if (!lua_isstring(L, 1) && !lua_isnumber(L, 1)) luaL_typerror(L, 1, "string");
    std::string str(lua_tostring(L, 1), lua_strlen(L, 1));
    std::wstring wstr;
    for (unsigned char c : str) wstr += (wchar_t)c;
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t> > converter;
    const std::string newstr = converter.to_bytes(wstr);
    fp->write(newstr.c_str(), newstr.size());
    fp->put('\n');
    return 0;
}

int fs_handle_writeByte(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) luaL_error(L, "attempt to use a closed file");
    if (lua_type(L, 1) == LUA_TNUMBER) {
        const char b = (unsigned char)(lua_tointeger(L, 1) & 0xFF);
        fp->put(b);
    } else if (lua_isstring(L, 1)) {
        size_t sz = 0;
        const char * str = lua_tolstring(L, 1, &sz);
        if (sz == 0) return 0;
        fp->write(str, sz);
    } else luaL_typerror(L, 1, "number or string");
    return 0;
}

int fs_handle_flush(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) luaL_error(L, "attempt to use a closed file");
    fp->flush();
#ifdef __EMSCRIPTEN__
    queueTask([](void*)->void*{syncfs(); return NULL;}, NULL, true);
#endif
    return 0;
}

int fs_handle_seek(lua_State *L) {
    lastCFunction = __func__;
    std::iostream * fp = *(std::iostream**)lua_touserdata(L, lua_upvalueindex(1));
    if (fp == NULL) return luaL_error(L, "attempt to use a closed file");
    const char * whence = luaL_optstring(L, 1, "cur");
    const size_t offset = luaL_optinteger(L, 2, 0);
    std::ios::seekdir origin;
    if (strcmp(whence, "set") == 0) origin = std::ios::beg;
    else if (strcmp(whence, "cur") == 0) origin = std::ios::cur;
    else if (strcmp(whence, "end") == 0) origin = std::ios::end;
    else return luaL_error(L, "bad argument #1 to 'seek' (invalid option '%s')", whence);
    fp->seekg(offset, origin);
    if (fp->bad()) {
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        return 2;
    }
    lua_pushinteger(L, fp->tellg());
    return 1;
}