/*
 * fs_handle.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for file handles.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include "fs_handle.hpp"
#include "lib.hpp"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
extern "C" {
#include <lauxlib.h>
}
#include <codecvt>
#include <string>
#include <locale>

int fs_handle_close(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) {
        lua_pushstring(L, "attempt to use a closed file");
        lua_error(L);
    }
    fclose((FILE*)lua_touserdata(L, lua_upvalueindex(1)));
    lua_pushnil(L);
    lua_replace(L, lua_upvalueindex(1));
    get_comp(L)->files_open--;
    return 0;
}

#define checkChar(c) c
/*
char checkChar(char c) {
	if ((c >= 32 && c < 127) || c == '\n' || c == '\t' || c == '\r') return c;
	else if (c == EOF) return '\0';
	else {
		//printf("Unknown char %d\n", c);
		return '?';
	}
}
*/

int fs_handle_readAll(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) {
        lua_pushstring(L, "attempt to use a closed file");
        lua_error(L);
    }
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    long pos = ftell(fp);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp) - pos;
    char * retval = new char[size + 1];
    memset(retval, 0, size + 1);
    fseek(fp, pos, SEEK_SET);
    int i;
    for (i = 0; !feof(fp) && i < size; i++) {
        char c = fgetc(fp);
        if (c == '\n' && (i > 0 && retval[i-1] == '\r')) retval[--i] = '\n';
        else retval[i] = c;
    }
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring wstr = converter.from_bytes(retval, retval + i - 1);
    delete[] retval;
    std::string out;
    for (wchar_t c : wstr) if (c < 256) out += (char)c;
    lua_pushlstring(L, out.c_str(), out.length());
    return 1;
}

int fs_handle_readLine(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) {
        lua_pushstring(L, "attempt to use a closed file");
        lua_error(L);
    }
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp) || ferror(fp)) {
        lua_pushnil(L);
        return 1;
    }
	char* retval = new char[256];
	for (unsigned i = 0; 1; i += 256) {
		if (fgets(&retval[i], 256, fp) == NULL) break;
		if (strlen(retval) < i + 255) break;
		retval = (char*)realloc(retval, i + 512);
	}
    int len = strlen(retval) - (retval[strlen(retval)-1] == '\n');
    if (retval[len-1] == '\r') retval[--len] = '\0';
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring wstr = converter.from_bytes(retval, retval + len);
    delete[] retval;
    std::string out;
    for (wchar_t c : wstr) if (c < 256) out += (char)c;
    lua_pushlstring(L, out.c_str(), out.length());
    return 1;
}

int fs_handle_readChar(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) {
        lua_pushstring(L, "attempt to use a closed file");
        lua_error(L);
    }
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    uint32_t codepoint;
    char c = fgetc(fp);
    if (c < 0) {
        if (c & 64) {
            char c2 = fgetc(fp);
            if (c2 >= 0 || c2 & 64) luaL_error(L, "malformed UTF-8 sequence");
            if (c & 32) {
                char c3 = fgetc(fp);
                if (c3 >= 0 || c3 & 64) luaL_error(L, "malformed UTF-8 sequence");
                if (c & 16) {
                    if (c & 8) luaL_error(L, "malformed UTF-8 sequence");
                    char c4 = fgetc(fp);
                    if (c4 >= 0 || c4 & 64) luaL_error(L, "malformed UTF-8 sequence");
                    codepoint = ((c & 0x7) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
                } else {
                    codepoint = ((c & 0xF) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                }
            } else {
                codepoint = ((c & 0x1F) << 6) | (c2 & 0x3F);
            }
        } else luaL_error(L, "malformed UTF-8 sequence");
    } else codepoint = c;
    if (codepoint > 255) return fs_handle_readChar(L);
    char retval[2];
    retval[0] = (char)codepoint;
    if (retval[0] == '\r') {
        int nextc = fgetc(fp);
        if (nextc == '\n') retval[0] = nextc;
        else ungetc(nextc, fp);
    }
    lua_pushstring(L, retval);
    return 1;
}

int fs_handle_readByte(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) {
        lua_pushstring(L, "attempt to use a closed file");
        lua_error(L);
    }
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    if (lua_isnumber(L, 1)) {
        size_t s = lua_tointeger(L, 1);
        char* retval = new char[s];
        fread(retval, s, 1, fp);
        lua_pushlstring(L, retval, s);
        delete[] retval;
    } else {
        int retval = fgetc(fp);
        if (retval == EOF || feof(fp)) return 0;
        lua_pushinteger(L, (unsigned char)retval);
    }
    return 1;
}

int fs_handle_writeString(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) {
        lua_pushstring(L, "attempt to use a closed file");
        lua_error(L);
    }
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * str = lua_tostring(L, 1);
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    fwrite(str, strlen(str), 1, fp);
    return 0;
}

int fs_handle_writeLine(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) {
        lua_pushstring(L, "attempt to use a closed file");
        lua_error(L);
    }
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * str = lua_tostring(L, 1);
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    fwrite(str, strlen(str), 1, fp);
    fputc('\n', fp);
    return 0;
}

int fs_handle_writeByte(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) {
        lua_pushstring(L, "attempt to use a closed file");
        lua_error(L);
    }
    if (lua_isnumber(L, 1)) {
        const char b = lua_tointeger(L, 1) & 0xFF;
        FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
        fputc(b, fp);
    } else if (lua_isstring(L, 1)) {
        FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
        fwrite(lua_tostring(L, 1), lua_strlen(L, 1), 1, fp);
    } else bad_argument(L, "number or string", 1);
    return 0;
}

int fs_handle_flush(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) {
        lua_pushstring(L, "attempt to use a closed file");
        lua_error(L);
    }
    fflush((FILE*)lua_touserdata(L, lua_upvalueindex(1)));
    return 0;
}

int fs_handle_seek(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) {
        lua_pushstring(L, "attempt to use a closed file");
        lua_error(L);
    }
    if (!lua_isstring(L, 1) && !lua_isnoneornil(L, 1)) bad_argument(L, "string or nil", 1);
    if (!lua_isnumber(L, 2) && !lua_isnoneornil(L, 2)) bad_argument(L, "number or nil", 2);
    const char * whence = lua_isstring(L, 1) ? lua_tostring(L, 1) : "cur";
    size_t offset = lua_isnumber(L, 2) ? lua_tointeger(L, 2) : 0;
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    int origin = 0;
    if (strcmp(whence, "set") == 0) origin = SEEK_SET;
    else if (strcmp(whence, "cur") == 0) origin = SEEK_CUR;
    else if (strcmp(whence, "end") == 0) origin = SEEK_END;
    else {
        lua_pushfstring(L, "bad argument #1 to 'seek' (invalid option '%s')", whence);
        lua_error(L);
    }
    if (fseek(fp, offset, origin) != 0) {
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        return 2;
    }
    lua_pushinteger(L, ftell(fp));
    return 1;
}