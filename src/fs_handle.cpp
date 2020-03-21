/*
 * fs_handle.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for file handles.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#define CRAFTOSPC_INTERNAL
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
#include <iostream>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include "os.hpp"
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
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        return 0;
    fclose((FILE*)lua_touserdata(L, lua_upvalueindex(1)));
    lua_pushnil(L);
    lua_replace(L, lua_upvalueindex(1));
    get_comp(L)->files_open--;
    #ifdef __EMSCRIPTEN__
    queueTask([](void*)->void*{syncfs(); return NULL;}, NULL, true);
    #endif
    return 0;
}

int fs_handle_istream_free(lua_State *L) {
    delete (std::istream*)lua_touserdata(L, 1);
    return 0;
}

int fs_handle_istream_close(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        return 0;
    //delete (std::istream*)lua_touserdata(L, lua_upvalueindex(1));
    lua_pushnil(L);
    lua_replace(L, lua_upvalueindex(1));
    get_comp(L)->files_open--;
    #ifdef __EMSCRIPTEN__
    queueTask([](void*)->void*{syncfs(); return NULL;}, NULL, true);
    #endif
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
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
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
        int c = fgetc(fp);
		if (c == EOF && feof(fp)) c = '\n';
        if (c == '\n' && (i > 0 && retval[i-1] == '\r')) retval[--i] = '\n';
        else retval[i] = (char)c;
    }
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring wstr;
	try {wstr = converter.from_bytes(retval, retval + i - (i == size ? 0 : 1));} 
	catch (std::exception &e) {
		fprintf(stderr, "fs_handle_readAll: Error decoding UTF-8: %s\n", e.what());
		lua_pushlstring(L, retval, i - (i == size ? 0 : 1));
		delete[] retval;
		return 1;
	}
    delete[] retval;
    std::string out;
    for (wchar_t c : wstr) {if (c < 256) out += (char)c; else out += '?';}
    lua_pushlstring(L, out.c_str(), out.length());
    return 1;
}

int fs_handle_istream_readAll(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
    std::istream * fp = (std::istream*)lua_touserdata(L, lua_upvalueindex(1));
    if (fp->eof()) return 0;
    long pos = fp->tellg();
    fp->seekg(0, std::ios::end);
    long size = (long)fp->tellg() - pos;
    char * retval = new char[size + 1];
    memset(retval, 0, size + 1);
    fp->seekg(pos);
    int i;
    for (i = 0; !fp->eof() && i < size; i++) {
        int c = fp->get();
		if (c == EOF && fp->eof()) c = '\n';
        if (c == '\n' && (i > 0 && retval[i-1] == '\r')) retval[--i] = '\n';
        else retval[i] = (char)c;
    }
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring wstr;
	try {wstr = converter.from_bytes(retval, retval + i - (i == size ? 0 : 1));} 
	catch (std::exception &e) {
		fprintf(stderr, "fs_handle_readAll: Error decoding UTF-8: %s\n", e.what());
		lua_pushlstring(L, retval, i - (i == size ? 0 : 1));
		delete[] retval;
		return 1;
	}
    delete[] retval;
    std::string out;
    for (wchar_t c : wstr) {if (c < 256) out += (char)c; else out += '?';}
    lua_pushlstring(L, out.c_str(), out.length());
    return 1;
}

int fs_handle_readLine(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp) || ferror(fp)) {
        lua_pushnil(L);
        return 1;
    }
	char* retval = (char*)malloc(256);
	for (unsigned i = 0; 1; i += 256) {
		if (fgets(&retval[i], 256, fp) == NULL) break;
		if (strlen(retval) < i + 255) break;
		retval = (char*)realloc(retval, i + 512);
	}
    int len = strlen(retval) - (retval[strlen(retval)-1] == '\n');
    if (retval[len-1] == '\r') retval[--len] = '\0';
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring wstr;
	try {wstr = converter.from_bytes(retval, retval + len);}
	catch (std::exception & e) {
		fprintf(stderr, "fs_handle_readLine: Error decoding UTF-8: %s\n", e.what());
		lua_pushlstring(L, retval, len);
		free(retval);
		return 1;
	}
    free(retval);
    std::string out;
    for (wchar_t c : wstr) {if (c < 256) out += (char)c; else out += '?';}
    lua_pushlstring(L, out.c_str(), out.length());
    return 1;
}

int fs_handle_istream_readLine(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
    std::istream * fp = (std::istream*)lua_touserdata(L, lua_upvalueindex(1));
    if (fp->bad() || fp->eof()) {
        lua_pushnil(L);
        return 1;
    }
    std::string retval;
    std::getline(*fp, retval);
    if (retval.size() == 0) {
        lua_pushnil(L);
        return 1;
    }
    int len = retval.length() - (retval[retval.length()-1] == '\n');
    if (retval[len-1] == '\r') retval[--len] = '\0';
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring wstr;
	try {wstr = converter.from_bytes(retval.c_str(), retval.c_str() + len);}
	catch (std::exception & e) {
		fprintf(stderr, "fs_handle_readLine: Error decoding UTF-8: %s\n", e.what());
		lua_pushlstring(L, retval.c_str(), len);
		return 1;
	}
    std::string out;
    for (wchar_t c : wstr) {if (c < 256) out += (char)c; else out += '?';}
    lua_pushlstring(L, out.c_str(), out.length());
    return 1;
}

int fs_handle_readChar(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    std::string retval;
    for (int i = 0; i < (lua_isnumber(L, 1) ? lua_tointeger(L, 1) : 1); i++) {
        uint32_t codepoint = 0;
        char c = fgetc(fp);
        if (c < 0) {
            if (c & 64) {
                char c2 = fgetc(fp);
                if (c2 >= 0 || c2 & 64) {codepoint = 2^31; goto fs_handle_readCharDone;}
                if (c & 32) {
                    char c3 = fgetc(fp);
                    if (c3 >= 0 || c3 & 64) {codepoint = 2^31; goto fs_handle_readCharDone;}
                    if (c & 16) {
                        if (c & 8) {codepoint = 2^31; goto fs_handle_readCharDone;}
                        char c4 = fgetc(fp);
                        if (c4 >= 0 || c4 & 64) {codepoint = 2^31; goto fs_handle_readCharDone;}
                        codepoint = ((c & 0x7) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
                    } else {
                        codepoint = ((c & 0xF) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                    }
                } else {
                    codepoint = ((c & 0x1F) << 6) | (c2 & 0x3F);
                }
            } else {codepoint = 2^31; goto fs_handle_readCharDone;}
        } else codepoint = c;
    fs_handle_readCharDone:
        if (codepoint > 255) retval += '?';
        else {
            if (codepoint == '\r') {
                int nextc = fgetc(fp);
                if (nextc == '\n') codepoint = nextc;
                else ungetc(nextc, fp);
            }
            retval += codepoint;
        }
    }
    lua_pushlstring(L, retval.c_str(), retval.length());
    return 1;
}

int fs_handle_istream_readChar(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
    std::istream * fp = (std::istream*)lua_touserdata(L, lua_upvalueindex(1));
    if (fp->eof()) return 0;
    std::string retval;
    for (int i = 0; i < (lua_isnumber(L, 1) ? lua_tointeger(L, 1) : 1); i++) {
        uint32_t codepoint = 0;
        char c = fp->get();
        if (c < 0) {
            if (c & 64) {
                char c2 = fp->get();
                if (c2 >= 0 || c2 & 64) {codepoint = 2^31; goto fs_handle_readCharDone;}
                if (c & 32) {
                    char c3 = fp->get();
                    if (c3 >= 0 || c3 & 64) {codepoint = 2^31; goto fs_handle_readCharDone;}
                    if (c & 16) {
                        if (c & 8) {codepoint = 2^31; goto fs_handle_readCharDone;}
                        char c4 = fp->get();
                        if (c4 >= 0 || c4 & 64) {codepoint = 2^31; goto fs_handle_readCharDone;}
                        codepoint = ((c & 0x7) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
                    } else {
                        codepoint = ((c & 0xF) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                    }
                } else {
                    codepoint = ((c & 0x1F) << 6) | (c2 & 0x3F);
                }
            } else {codepoint = 2^31; goto fs_handle_readCharDone;}
        } else codepoint = c;
    fs_handle_readCharDone:
        if (codepoint > 255) retval += '?';
        else {
            if (codepoint == '\r') {
                int nextc = fp->get();
                if (nextc == '\n') codepoint = nextc;
                else fp->putback(nextc);
            }
            retval += codepoint;
        }
    }
    lua_pushlstring(L, retval.c_str(), retval.length());
    return 1;
}

int fs_handle_readByte(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
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

int fs_handle_istream_readByte(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
    std::istream * fp = (std::istream*)lua_touserdata(L, lua_upvalueindex(1));
    if (fp->eof()) return 0;
    if (lua_isnumber(L, 1)) {
        size_t s = lua_tointeger(L, 1);
        char* retval = new char[s];
        fp->read(retval, s);
        lua_pushlstring(L, retval, s);
        delete[] retval;
    } else {
        int retval = fp->get();
        if (retval == EOF || fp->eof()) return 0;
        lua_pushinteger(L, (unsigned char)retval);
    }
    return 1;
}

int fs_handle_writeString(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * str = lua_tostring(L, 1);
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    fwrite(str, strlen(str), 1, fp);
    return 0;
}

int fs_handle_writeLine(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * str = lua_tostring(L, 1);
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    fwrite(str, strlen(str), 1, fp);
    fputc('\n', fp);
    return 0;
}

int fs_handle_writeByte(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
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
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
    fflush((FILE*)lua_touserdata(L, lua_upvalueindex(1)));
    #ifdef __EMSCRIPTEN__
    queueTask([](void*)->void*{syncfs(); return NULL;}, NULL, true);
    #endif
    return 0;
}

int fs_handle_seek(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
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

int fs_handle_istream_seek(lua_State *L) {
    if (!lua_isuserdata(L, lua_upvalueindex(1)))
        luaL_error(L, "attempt to use a closed file");
    if (!lua_isstring(L, 1) && !lua_isnoneornil(L, 1)) bad_argument(L, "string or nil", 1);
    if (!lua_isnumber(L, 2) && !lua_isnoneornil(L, 2)) bad_argument(L, "number or nil", 2);
    const char * whence = lua_isstring(L, 1) ? lua_tostring(L, 1) : "cur";
    size_t offset = lua_isnumber(L, 2) ? lua_tointeger(L, 2) : 0;
    std::istream * fp = (std::iostream*)lua_touserdata(L, lua_upvalueindex(1));
    std::ios::seekdir origin;
    if (strcmp(whence, "set") == 0) origin = std::ios::seekdir::beg;
    else if (strcmp(whence, "cur") == 0) origin = std::ios::seekdir::cur;
    else if (strcmp(whence, "end") == 0) origin = std::ios::seekdir::end;
    else {
        lua_pushfstring(L, "bad argument #1 to 'seek' (invalid option '%s')", whence);
        lua_error(L);
        return 0;
    }
    fp->seekg(offset, origin);
    if (fp->bad()) {
        lua_pushnil(L);
        lua_pushstring(L, strerror(errno));
        return 2;
    }
    lua_pushinteger(L, fp->tellg());
    return 1;
}