#include "lib.h"

void load_library(lua_State *L, library_t lib) {
    lua_newtable(L); // create table
    for (int i = 0; i < lib.count; i++) {
        lua_pushstring(L, lib.keys[i]); // push index
        lua_pushcfunction(L, lib.values[i]); // push value
        lua_settable(L, -3); // add index/value to table
    }
    lua_setglobal(L, lib.name); // add table as global
}