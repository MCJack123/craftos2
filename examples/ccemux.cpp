extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include "../src/Computer.hpp"
#include <chrono>
#include <string>
#ifdef _WIN32
#include <SDL.h>
#include <windows.h>
#else
#include <SDL2/SDL.h>
#include <stdlib.h>
#endif

const char * basePath;

void bad_argument(lua_State *L, const char * type, int pos) {
    lua_pushfstring(L, "bad argument #%d (expected %s, got %s)", pos, type, lua_typename(L, lua_type(L, pos)));
    lua_error(L);
}

Computer * get_comp(lua_State *L) {
    lua_pushstring(L, "computer");
    lua_gettable(L, LUA_REGISTRYINDEX);
    void * retval = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return (Computer*)retval;
}

int ccemux_getVersion(lua_State *L) {
    lua_getglobal(L, "os");
    lua_pushstring(L, "version");
    lua_gettable(L, -2);
    lua_call(L, 0, 1);
    lua_getglobal(L, "string");
    lua_pushstring(L, "match");
    lua_gettable(L, -2);
    lua_pushvalue(L, -3);
    lua_pushstring(L, "^CraftOS%-PC v([%d%l%.%-]+)\n");
    lua_call(L, 2, 1);
    return 1;
}

int ccemux_openEmu(lua_State *L) {
    int id = 0;
    if (lua_isnumber(L, 1)) id = lua_tointeger(L, 1);
    else {
        lua_getglobal(L, "peripheral");
        for (; id < 256; id++) { // don't search forever
            lua_pushstring(L, "isPresent");
            lua_gettable(L, -2);
            lua_pushfstring(L, "computer_%d", id);
            lua_call(L, 1, 1);
            if (!lua_toboolean(L, -1)) break;
            lua_pop(L, 1);
        }
        lua_pop(L, 2);
    }
    lua_getglobal(L, "periphemu");
    lua_pushstring(L, "create");
    lua_gettable(L, -2);
    lua_pushinteger(L, id);
    lua_pushstring(L, "computer");
    if (lua_pcall(L, 2, 1, 0) != 0) lua_error(L);
    if (lua_toboolean(L, -1)) lua_pushinteger(L, id);
    else lua_pushnil(L);
    return 1;
}

int ccemux_closeEmu(lua_State *L) {
    lua_getglobal(L, "os");
    lua_pushstring(L, "shutdown");
    lua_gettable(L, -2);
    lua_call(L, 0, 0);
    return 0;
}

int ccemux_openDataDir(lua_State *L) {
    Computer *comp = get_comp(L);
#ifdef WIN32
    ShellExecute(NULL, "explore", (std::string(basePath) + "/computer/" + std::to_string(comp->id)).c_str(), NULL, NULL, SW_SHOW);
#elif defined(__APPLE__)
    system(("open " + std::string(basePath) + "/computer/" + std::to_string(comp->id)).c_str());
    lua_pushboolean(L, true);
#elif defined(__linux__)
    system(("xdg-open " + std::string(basePath) + "/computer/" + std::to_string(comp->id)).c_str());
    lua_pushboolean(L, true);
#else
    lua_pushboolean(L, false);
#endif
    return 1;
}

int ccemux_openConfig(lua_State *L) {
#ifdef WIN32
    ShellExecute(NULL, "open", (std::string(basePath) + "/config/global.json").c_str(), NULL, NULL, SW_SHOW);
#elif defined(__APPLE__)
    system(("open " + std::string(basePath) + "/config/global.json").c_str());
    lua_pushboolean(L, true);
#elif defined(__linux__)
    system(("xdg-open " + std::string(basePath) + "/config/global.json").c_str());
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
    lua_getglobal(L, "periphemu");
    lua_pushstring(L, "create");
    lua_gettable(L, -2);
    for (int i = 1; i < args; i++) lua_pushvalue(L, i);
    if (!lua_pcall(L, args, 1, 0)) lua_error(L);
    return 1;
}

int ccemux_detach(lua_State *L) {
    int args = lua_gettop(L);
    lua_getglobal(L, "periphemu");
    lua_pushstring(L, "remove");
    lua_gettable(L, -2);
    for (int i = 1; i < args; i++) lua_pushvalue(L, i);
    if (!lua_pcall(L, args, 1, 0)) lua_error(L);
    return 1;
}

#ifdef _WIN32
_declspec(dllexport)
#endif
int luaopen_ccemux(lua_State *L) {
    basePath = lua_tostring(L, 2);
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
    luaL_register(L, "ccemux", M);
    return 1;
}