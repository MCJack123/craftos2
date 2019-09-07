/*
 * fs_handle.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for file handles.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "fs_handle.hpp"
#include "lib.hpp"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

int fs_handle_close(lua_State *L) {
    fclose((FILE*)lua_touserdata(L, lua_upvalueindex(1)));
    lua_pushnil(L);
    lua_replace(L, lua_upvalueindex(1));
    get_comp(L)->files_open--;
    return 0;
}

char checkChar(char c) {
	if ((c >= 32 && c < 127) || c == '\n' || c == '\t' || c == '\r') return c;
	else if (c == EOF) return '\0';
	else {
		//printf("Unknown char %d\n", c);
		return '?';
	}
}

int fs_handle_readAll(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) return 0;
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    long pos = ftell(fp);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp) - pos;
    char * retval = (char*)malloc(size + 1);
    memset(retval, 0, size + 1);
    fseek(fp, pos, SEEK_SET);
    int i;
    for (i = 0; !feof(fp) && i < size; i++) {
        char c = fgetc(fp);
        if (c == '\n' && (i > 0 && retval[i-1] == '\r')) retval[--i] = '\n';
        else retval[i] = checkChar(c);
    }
    lua_pushstring(L, retval);
    free(retval);
    return 1;
}

int fs_handle_readLine(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) return 0;
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp) || ferror(fp)) {
        lua_pushnil(L);
        return 1;
    }
	char* retval = (char*)malloc(256);
	for (int i = 0; 1; i += 256) {
		if (fgets(&retval[i], 256, fp) == NULL) break;
		if (strlen(retval) < i + 255) break;
		char* tmp = (char*)malloc(i + 512);
		memcpy(tmp, retval, i + 256);
		free(retval);
		retval = tmp;
	}
    int len = strlen(retval) - (retval[strlen(retval)-1] == '\n');
    if (retval[len-1] == '\r') retval[--len] = '\0';
    lua_pushlstring(L, retval, len);
    free(retval);
    return 1;
}

int fs_handle_readChar(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) return 0;
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    char retval[2];
    retval[0] = checkChar(fgetc(fp));
    if (retval[0] == '\r') {
        int nextc = fgetc(fp);
        if (nextc == '\n') retval[0] = nextc;
        else ungetc(nextc, fp);
    }
    lua_pushstring(L, retval);
    return 1;
}

int fs_handle_readByte(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) return 0;
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    if (lua_isnumber(L, 1)) {
        size_t s = lua_tointeger(L, 1);
        char* retval = (char*)malloc(s);
        size_t read = fread(retval, s, 1, fp);
        lua_pushlstring(L, retval, read);
        free(retval);
    } else {
        int retval = fgetc(fp);
        if (retval == EOF || feof(fp)) return 0;
        lua_pushinteger(L, (unsigned char)retval);
    }
    return 1;
}

int fs_handle_writeString(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) return 0;
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * str = lua_tostring(L, 1);
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    fwrite(str, strlen(str), 1, fp);
    return 0;
}

int fs_handle_writeLine(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) return 0;
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * str = lua_tostring(L, 1);
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    fwrite(str, strlen(str), 1, fp);
    fputc('\n', fp);
    return 0;
}

int fs_handle_writeByte(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) return 0;
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    const char b = lua_tointeger(L, 1) & 0xFF;
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    fputc(b, fp);
    return 0;
}

int fs_handle_flush(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) return 0;
    fflush((FILE*)lua_touserdata(L, lua_upvalueindex(1)));
    return 0;
}

int fs_handle_seek(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1))) return 0;
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