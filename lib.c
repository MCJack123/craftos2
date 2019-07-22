#include "lib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

void load_library(lua_State *L, library_t lib) {
    lua_newtable(L); // create table
    for (int i = 0; i < lib.count; i++) {
        lua_pushstring(L, lib.keys[i]); // push index
        lua_pushcfunction(L, lib.values[i]); // push value
        lua_settable(L, -3); // add index/value to table
    }
    lua_setglobal(L, lib.name); // add table as global
}

void bad_argument(lua_State *L, const char * type, int pos) {
    const char * gtype = lua_typename(L, lua_type(L, pos));
    char * retval = (char*)malloc(strlen("bad argument # (expected , got )") + strlen(type) + strlen(gtype) + (int)log10(pos) + 2);
    sprintf(retval, "bad argument #%i (expected %s, got %s)", pos, type, gtype);
    lua_pushstring(L, retval);
    free(retval);
    lua_error(L);
}