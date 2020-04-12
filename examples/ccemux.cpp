/*
 * ccemux.cpp
 * CraftOS-PC 2
 * 
 * This file creates a new CCEmuX API for backwards-compatibility with CCEmuX
 * programs when run in CraftOS-PC.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "../src/Computer.hpp"
#include <chrono>
#include <string>
#include <SDL2/SDL.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <stdlib.h>
#endif
#define libFunc(lib, name) getLibraryFunction(getLibrary(lib), name)

#define PLUGIN_VERSION 2

library_t * (*getLibrary)(std::string);

void bad_argument(lua_State *L, const char * type, int pos) {
    lua_pushfstring(L, "bad argument #%d (expected %s, got %s)", pos, type, lua_typename(L, lua_type(L, pos)));
    lua_error(L);
}

lua_CFunction getLibraryFunction(library_t * lib, const char * name) {
    for (int i = 0; i < lib->count; i++) if (std::string(lib->keys[i]) == std::string(name)) return lib->values[i];
    return NULL;
}

int ccemux_getVersion(lua_State *L) {
    libFunc("os", "about")(L);
    lua_getglobal(L, "string");
    lua_pushstring(L, "match");
    lua_gettable(L, -2);
    lua_pushvalue(L, -3);
    lua_pushstring(L, "^CraftOS%-PC v([%d%l%.%-]+)\n");
    lua_call(L, 2, 1);
    return 1;
}

int ccemux_openEmu(lua_State *L) {
    int id = 1;
    if (lua_isnumber(L, 1)) id = lua_tointeger(L, 1);
    else {
        library_t * plib = getLibrary("peripheral");
        for (; id < 256; id++) { // don't search forever
            lua_pushcfunction(L, getLibraryFunction(plib, "isPresent"));
            lua_pushfstring(L, "computer_%d", id);
            lua_call(L, 1, 1);
            if (!lua_toboolean(L, -1)) {lua_pop(L, 1); break;}
            lua_pop(L, 1);
        }
    }
    lua_pushcfunction(L, libFunc("periphemu", "create"));
    lua_pushinteger(L, id);
    lua_pushstring(L, "computer");
    if (lua_pcall(L, 2, 1, 0) != 0) lua_error(L);
    if (lua_toboolean(L, -1)) lua_pushinteger(L, id);
    else lua_pushnil(L);
    return 1;
}

int ccemux_closeEmu(lua_State *L) {
    return libFunc("os", "shutdown")(L);
}

int ccemux_openDataDir(lua_State *L) {
    const char * basePath = lua_tostring(L, lua_upvalueindex(1));
    Computer *comp = get_comp(L);
#ifdef WIN32
    ShellExecuteA(NULL, "explore", (std::string(basePath) + "/computer/" + std::to_string(comp->id)).c_str(), NULL, NULL, SW_SHOW);
#elif defined(__APPLE__)
    system(("open '" + std::string(basePath) + "/computer/" + std::to_string(comp->id) + "'").c_str());
    lua_pushboolean(L, true);
#elif defined(__linux__)
    system(("xdg-open '" + std::string(basePath) + "/computer/" + std::to_string(comp->id) + "'").c_str());
    lua_pushboolean(L, true);
#else
    lua_pushboolean(L, false);
#endif
    return 1;
}

int ccemux_openConfig(lua_State *L) {
    const char * basePath = lua_tostring(L, lua_upvalueindex(1));
#ifdef WIN32
    ShellExecuteA(NULL, "open", (std::string(basePath) + "/config/global.json").c_str(), NULL, NULL, SW_SHOW);
#elif defined(__APPLE__)
    system(("open '" + std::string(basePath) + "/config/global.json'").c_str());
    lua_pushboolean(L, true);
#elif defined(__linux__)
    system(("xdg-open '" + std::string(basePath) + "/config/global.json'").c_str());
    lua_pushboolean(L, true);
#else
    lua_pushboolean(L, false);
#endif
    return 1;
}

int ccemux_milliTime(lua_State *L) {
    lua_pushinteger(L, std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    return 1;
}

int ccemux_nanoTime(lua_State *L) {
    lua_pushinteger(L, std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count());
    return 1;
}

int ccemux_echo(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    printf("%s\n", lua_tostring(L, 1));
    return 0;
}

int ccemux_setClipboard(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    SDL_SetClipboardText(lua_tostring(L, 1));
    return 0;
}

int ccemux_attach(lua_State *L) {
    int args = lua_gettop(L);
    if (lua_isstring(L, 2) && std::string(lua_tostring(L, 2)) == "disk_drive") {
        lua_pushstring(L, "drive");
        lua_replace(L, 2);
    }
    if (lua_isstring(L, 2) && std::string(lua_tostring(L, 2)) == "wireless_modem") {
        lua_pushstring(L, "modem");
        lua_replace(L, 2);
    }
    return libFunc("periphemu", "create")(L);
}

int ccemux_detach(lua_State *L) {
    return libFunc("periphemu", "remove")(L);
}

extern "C" {
#ifdef _WIN32
_declspec(dllexport)
#endif
int luaopen_ccemux(lua_State *L) {
    struct luaL_reg M[] = {
        {"getVersion", ccemux_getVersion},
        {"openEmu", ccemux_openEmu},
        {"closeEmu", ccemux_closeEmu},
        {"openDataDir", ccemux_openDataDir},
        {"openConfig", ccemux_openConfig},
        {"milliTime", ccemux_milliTime},
        {"nanoTime", ccemux_nanoTime},
        {"echo", ccemux_echo},
        {"setClipboard", ccemux_setClipboard},
        {"attach", ccemux_attach},
        {"detach", ccemux_detach},
        {NULL, NULL}
    };
    lua_newtable(L);
    for (int i = 0; M[i].name != NULL && M[i].func != NULL; i++) {
        lua_pushstring(L, M[i].name);
        if (std::string(M[i].name) == "openDataDir" || std::string(M[i].name) == "openConfig") {
            lua_pushvalue(L, 2);
            lua_pushcclosure(L, M[i].func, 1);
        } else lua_pushcfunction(L, M[i].func);
        lua_settable(L, -3);
    }
    return 1;
}

int register_getLibrary(lua_State *L) {getLibrary = (library_t*(*)(std::string))lua_touserdata(L, 1); return 0;}

#ifdef _WIN32
_declspec(dllexport)
#endif
int plugin_info(lua_State *L) {
    lua_newtable(L);
    lua_pushinteger(L, PLUGIN_VERSION);
    lua_setfield(L, -2, "version");
    lua_pushcfunction(L, register_getLibrary);
    lua_setfield(L, -2, "register_getLibrary");
    return 1;
}
}