#include "fs_handle.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int handle_close(lua_State *L) {
    fclose((FILE*)lua_touserdata(L, lua_upvalueindex(1)));
    return 0;
}

char checkChar(char c) {
    if (c == 127) return '?';
    if ((c >= 32 && c < 127) || c == '\n' || c == '\t' || c == '\r') return c;
    else return '?';
}

int handle_readAll(lua_State *L) {
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    long pos = ftell(fp);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp) - pos;
    char * retval = (char*)malloc(size + 1);
    memset(retval, 0, size + 1);
    fseek(fp, pos, SEEK_SET);
    int i;
    for (i = 0; !feof(fp) && i < size; i++)
        retval[i] = checkChar(fgetc(fp));
    lua_pushstring(L, retval);
    return 1;
}

int handle_readLine(lua_State *L) {
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    long size = 0;
    while (fgetc(fp) != '\n' && !feof(fp)) size++;
    fseek(fp, 0 - size - 1, SEEK_CUR);
    char * retval = (char*)malloc(size + 1);
    for (int i = 0; i < size; i++) retval[i] = checkChar(fgetc(fp));
    fgetc(fp);
    lua_pushstring(L, retval);
    return 1;
}

int handle_readChar(lua_State *L) {
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    char retval[2];
    retval[0] = checkChar(fgetc(fp));
    lua_pushstring(L, retval);
    return 1;
}

int handle_readByte(lua_State *L) {
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    if (feof(fp)) return 0;
    char retval = fgetc(fp);
    lua_pushinteger(L, retval);
    return 1;
}

int handle_writeString(lua_State *L) {
    const char * str = lua_tostring(L, 1);
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    fwrite(str, strlen(str), 1, fp);
    return 0;
}

int handle_writeLine(lua_State *L) {
    const char * str = lua_tostring(L, 1);
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    fwrite(str, strlen(str), 1, fp);
    fputc('\n', fp);
    return 0;
}

int handle_writeByte(lua_State *L) {
    const char b = lua_tointeger(L, 1) & 0xFF;
    FILE * fp = (FILE*)lua_touserdata(L, lua_upvalueindex(1));
    fputc(b, fp);
    return 0;
}

int handle_flush(lua_State *L) {
    fflush((FILE*)lua_touserdata(L, lua_upvalueindex(1)));
    return 0;
}