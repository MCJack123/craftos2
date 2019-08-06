#include <lauxlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/utsname.h>
#ifdef WIN32
#include <SDL_main.h>
#else
#include <SDL2/SDL_main.h>
#endif
#include "bit.h"
#include "config.h"
#include "fs.h"
#include "http.h"
#include "os.h"
#include "term.h"
#include "redstone.h"
#include "peripheral/peripheral.h"
#include "periphemu.h"
#include "platform.h"

void * tid;
lua_State *L;

library_t * libraries[] = {
    &bit_lib,
    &config_lib,
    &fs_lib,
    &os_lib,
    &peripheral_lib,
    &periphemu_lib,
    &rs_lib,
    &term_lib
};

void sighandler(int sig) {
    if (sig == SIGINT) {
        running = 0;
        joinThread(tid);
        for (int i = 0; i < sizeof(libraries) / sizeof(library_t*); i++) 
            if (libraries[i]->deinit != NULL) libraries[i]->deinit();
        lua_close(L);   /* Cya, Lua */
        exit(SIGINT);
    }
}

int main(int argc, char*argv[]) {
    int status;
    lua_State *coro;
start:
    /*
     * All Lua contexts are held in this structure. We work with it almost
     * all the time.
     */
    L = luaL_newstate();

    coro = lua_newthread(L);

    // Load libraries
    luaL_openlibs(coro);
    lua_sethook(coro, termHook, LUA_MASKCOUNT, 100);
    for (int i = 0; i < sizeof(libraries) / sizeof(library_t*); i++) load_library(coro, *libraries[i]);
    if (config.http_enable) load_library(coro, http_lib);
    lua_getglobal(coro, "redstone");
    lua_setglobal(coro, "rs");

    // Delete unwanted globals
    lua_pushnil(L);
    lua_setglobal(L, "collectgarbage");
    lua_pushnil(L);
    lua_setglobal(L, "dofile");
    lua_pushnil(L);
    lua_setglobal(L, "loadfile");
    lua_pushnil(L);
    lua_setglobal(L, "module");
    lua_pushnil(L);
    lua_setglobal(L, "require");
    lua_pushnil(L);
    lua_setglobal(L, "package");
    lua_pushnil(L);
    lua_setglobal(L, "io");
    lua_pushnil(L);
    lua_setglobal(L, "print");
    lua_pushnil(L);
    lua_setglobal(L, "newproxy");

    // Set default globals
    lua_pushstring(L, config.default_computer_settings);
    lua_setglobal(L, "_CC_DEFAULT_SETTINGS");
    struct utsname host;
    uname(&host);
    lua_pushfstring(L, "%s %s %s", host.sysname, "i386", host.release);
    lua_setglobal(L, "_HOST");

    /* Load the file containing the script we are going to run */
	char* bios_path_expanded = expandEnvironment(bios_path);
    status = luaL_loadfile(coro, bios_path_expanded);
	free(bios_path_expanded);
    if (status) {
        /* If something went wrong, error message is at the top of */
        /* the stack */
        fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(L, -1));
		msleep(5000);
        exit(1);
    }
    tid = createThread(&termRenderLoop, coro);
    signal(SIGINT, sighandler);

    /* Ask Lua to run our little script */
    status = LUA_YIELD;
    int narg = 0;
    while (status == LUA_YIELD && running == 1) {
        status = lua_resume(coro, narg);
        if (status == LUA_YIELD) {
            if (lua_isstring(coro, -1)) narg = getNextEvent(coro, lua_tostring(coro, -1));
            else narg = getNextEvent(coro, "");
        } else if (status != 0) {
            running = 0;
            joinThread(tid);
            //usleep(5000000);
            printf("%s\n", lua_tostring(coro, -1));
            for (int i = 0; i < sizeof(libraries) / sizeof(library_t*); i++) 
                if (libraries[i]->deinit != NULL) libraries[i]->deinit();
            lua_close(L);
            exit(1);
        }
    }

    joinThread(tid);
    for (int i = 0; i < sizeof(libraries) / sizeof(library_t*); i++) 
        if (libraries[i]->deinit != NULL) libraries[i]->deinit();
    lua_close(L);   /* Cya, Lua */

    if (running == 2) {
        //usleep(1000000);
        running = 1;
        goto start;
    }

    return 0;
}
