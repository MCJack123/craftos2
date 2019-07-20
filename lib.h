#ifndef LIB_H
#define LIB_H
#include <lua.h>
#include <lualib.h>

typedef struct library {
    const char * name;
    int count;
    const char ** keys;
    lua_CFunction * values;
} library_t;

void load_library(lua_State *L, library_t lib);
#endif