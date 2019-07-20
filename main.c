#include <lauxlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "bit.h"
#include "fs.h"
#include "os.h"
#include "term.h"
#include "redstone.h"

pthread_t tid;
lua_State *L;

void sighandler(int sig) {
    if (sig == SIGINT) {
        running = 0;
        pthread_join(tid, NULL);
        termClose();
        closeKeys();
        lua_close(L);   /* Cya, Lua */
        exit(SIGINT);
    }
}

int main() {
    int status, result, i;
    double sum;
    lua_State *coro;
start:
    /*
     * All Lua contexts are held in this structure. We work with it almost
     * all the time.
     */
    L = luaL_newstate();

    coro = lua_newthread(L);

    luaL_openlibs(coro); /* Load Lua libraries */
    load_library(coro, bit_lib);
    load_library(coro, fs_lib);
    load_library(coro, os_lib);
    load_library(coro, rs_lib);
    lua_getglobal(coro, "redstone");
    lua_setglobal(coro, "rs");
    load_library(coro, term_lib);
    termInit();

    lua_pushstring(L, "");
    lua_setglobal(L, "_CC_DEFAULT_SETTINGS");
    lua_pushstring(L, "Linux i386 4.18");
    lua_setglobal(L, "_HOST");

    /* Load the file containing the script we are going to run */
    status = luaL_loadfile(coro, "/etc/craftos/bios.lua");
    if (status) {
        /* If something went wrong, error message is at the top of */
        /* the stack */
        fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(L, -1));
        exit(1);
    }
    pthread_create(&tid, NULL, &termRenderLoop, NULL);
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
            pthread_join(tid, NULL);
            usleep(5000000);
            termClose();
            printf("%s\n", lua_tostring(coro, -1));
            lua_close(L);
            exit(1);
        }
    }

    pthread_join(tid, NULL);
    termClose();
    lua_close(L);   /* Cya, Lua */

    if (running == 2) {
        usleep(1000000);
        goto start;
    }

    return 0;
}
