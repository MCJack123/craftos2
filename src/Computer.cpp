/*
 * Computer.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods of the Computer class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "Computer.hpp"
#include "platform.hpp"
#include "term.hpp"
#include "bit.hpp"
#include "config.hpp"
#include "fs.hpp"
#include "http.hpp"
#include "mounter.hpp"
#include "os.hpp"
#include "redstone.hpp"
#include "peripheral/peripheral.hpp"
#include "peripheral/computer.hpp"
#include "periphemu.hpp"
#include <unordered_set>
extern "C" {
#include <lauxlib.h>
}

std::vector<Computer*> computers;
std::unordered_set<Computer*> freedComputers; 

library_t * libraries[] = {
    &bit_lib,
    &config_lib,
    &fs_lib,
    &mounter_lib,
    &os_lib,
    &peripheral_lib,
    &periphemu_lib,
    &rs_lib,
    &term_lib
};

Computer::Computer(int i) {
    id = i;
    mounter_initializing = true;
    platformInit(this);
    mounter_initializing = false;
#ifdef _WIN32
    createDirectory((std::string(getBasePath()) + "\\computer\\" + std::to_string(id)).c_str());
#else
    createDirectory((std::string(getBasePath()) + "/computer/" + std::to_string(id)).c_str());
#endif
    term = new TerminalWindow("CraftOS Terminal: Computer " + std::to_string(id));
}

Computer::~Computer() {
    delete term;
    for (auto p : peripherals) delete p.second;
    for (Computer * c : referencers) {
        for (auto it = c->peripherals.begin(); it != c->peripherals.end(); it++) {
            if (std::string(it->second->getMethods().name) == "computer" && ((computer*)it->second)->comp == this) {
                delete (computer*)it->second;
                it = c->peripherals.erase(it);
                if (it == c->peripherals.end()) break;
            }
        }
    }
}

void Computer::run() {
    running = 1;
    if (L != NULL) lua_close(L);
    while (running) {
        int status;
        lua_State *coro;
        term->screen = std::vector<std::vector<char> >(term->height, std::vector<char>(term->width, ' '));
        term->colors = std::vector<std::vector<unsigned char> >(term->height, std::vector<unsigned char>(term->width, 0xF0));
        term->pixels = std::vector<std::vector<char> >(term->height*term->fontHeight, std::vector<char>(term->width*term->fontWidth, 0x0F));

        /*
        * All Lua contexts are held in this structure. We work with it almost
        * all the time.
        */
        L = luaL_newstate();

        coro = lua_newthread(L);
        paramQueue = lua_newthread(L);

        // Push reference to this to the registry
        lua_pushstring(L, "computer");
        lua_pushlightuserdata(L, this);
        lua_settable(L, LUA_REGISTRYINDEX);

        // Load libraries
        luaL_openlibs(coro);
        lua_sethook(coro, termHook, LUA_MASKCOUNT, 100);
        for (int i = 0; i < sizeof(libraries) / sizeof(library_t*); i++) load_library(this, coro, *libraries[i]);
        if (config.http_enable) load_library(this, coro, http_lib);
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
        if (!config.debug_enable) {
            lua_pushnil(L);
            lua_setglobal(L, "debug");
        }

        // Set default globals
        lua_pushstring(L, config.default_computer_settings);
        lua_setglobal(L, "_CC_DEFAULT_SETTINGS");
        pushHostString(L);
        lua_setglobal(L, "_HOST");

        // Load patched pcall/xpcall
        luaL_loadstring(L, "return function( _fn, _fnErrorHandler )\n\
        local typeT = type( _fn )\n\
        assert( typeT == \"function\", \"bad argument #1 to xpcall (function expected, got \"..typeT..\")\" )\n\
        local co = coroutine.create( _fn )\n\
        local tResults = { coroutine.resume( co ) }\n\
        while coroutine.status( co ) ~= \"dead\" do\n\
            tResults = { coroutine.resume( co, coroutine.yield( unpack( tResults, 2 ) ) ) }\n\
        end\n\
        if tResults[1] == true then\n\
            return true, unpack( tResults, 2 )\n\
        else\n\
            return false, _fnErrorHandler( tResults[2] )\n\
        end\n\
    end");
        lua_call(L, 0, 1);
        lua_setglobal(L, "xpcall");
        
        luaL_loadstring(L, "return function( _fn, ... )\n\
        local typeT = type( _fn )\n\
        assert( typeT == \"function\", \"bad argument #1 to pcall (function expected, got \"..typeT..\")\" )\n\
        local tArgs = { ... }\n\
        return xpcall(\n\
            function()\n\
                return _fn( unpack( tArgs ) )\n\
            end,\n\
            function( _error )\n\
                return _error\n\
            end\n\
        )\n\
    end");
        lua_call(L, 0, 1);
        lua_setglobal(L, "pcall");

        /* Load the file containing the script we are going to run */
        char* bios_path_expanded = getBIOSPath();
        status = luaL_loadfile(coro, bios_path_expanded);
        free(bios_path_expanded);
        if (status) {
            /* If something went wrong, error message is at the top of */
            /* the stack */
            fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(L, -1));
            msleep(5000);
            exit(1);
        }
        void * tid = createThread(&termRenderLoop, this, std::string("Computer " + std::to_string(id) + " Render Thread").c_str());
        //signal(SIGINT, sighandler);

        /* Ask Lua to run our little script */
        status = LUA_YIELD;
        int narg = 0;
        running = 1;
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
                    if (libraries[i]->deinit != NULL) libraries[i]->deinit(this);
                lua_close(L);
                L = NULL;
                return;
            }
        }

        joinThread(tid);
        for (int i = 0; i < sizeof(libraries) / sizeof(library_t*); i++) 
            if (libraries[i]->deinit != NULL) libraries[i]->deinit(this);
        lua_close(L);   /* Cya, Lua */
        L = NULL;
    }
}

bool Computer::getEvent(SDL_Event* e) {
    if (termEventQueue.size() == 0) return false;
    memcpy(e, &termEventQueue.front(), sizeof(SDL_Event));
    termEventQueue.pop();
    return true;
}

void* computerThread(void* data) {
    Computer * comp = (Computer*)data;
    comp->run();
    freedComputers.insert(comp);
    for (auto it = computers.begin(); it != computers.end(); it++) {
        if (*it == comp) {
            it = computers.erase(it);
            if (it == computers.end()) break;
        }
    }
    queueTask([](void* arg)->void* {delete (Computer*)arg; return NULL; }, comp);
    return NULL;
}

std::list<void*> computerThreads;

Computer * startComputer(int id) {
    Computer * comp = new Computer(id);
    computers.push_back(comp);
    computerThreads.push_back(createThread(computerThread, comp, std::string("Computer " + std::to_string(id) + " Main Thread").c_str()));
    return comp;
}