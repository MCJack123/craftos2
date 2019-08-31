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
#include <thread>
#include <fstream>
#include <dirent.h>
#include <sys/stat.h>
extern "C" {
#include <lauxlib.h>
}

extern bool headless;
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
    if (headless) term = NULL;
    else term = new TerminalWindow("CraftOS Terminal: Computer " + std::to_string(id));
    config = getComputerConfig(id);
}

Computer::~Computer() {
    if (!headless) delete term;
    setComputerConfig(id, config);
    freeComputerConfig(config);
    for (auto p : peripherals) delete p.second;
    for (std::list<Computer*>::iterator c = referencers.begin(); c != referencers.end(); c++) {
        (*c)->peripherals_mutex.lock();
        for (auto it = (*c)->peripherals.begin(); it != (*c)->peripherals.end(); it++) {
            if (std::string(it->second->getMethods().name) == "computer" && ((computer*)it->second)->comp == this) {
                delete (computer*)it->second;
                it = (*c)->peripherals.erase(it);
                if (it == (*c)->peripherals.end()) break;
            }
        }
        (*c)->peripherals_mutex.unlock();
        if (c == referencers.end()) break;
    }
}

void Computer::run() {
    running = 1;
    if (L != NULL) lua_close(L);
    while (running) {
        int status;
        lua_State *coro;
        if (!headless) {
            term->screen = std::vector<std::vector<char> >(term->height, std::vector<char>(term->width, ' '));
            term->colors = std::vector<std::vector<unsigned char> >(term->height, std::vector<unsigned char>(term->width, 0xF0));
            term->pixels = std::vector<std::vector<char> >(term->height * term->fontHeight, std::vector<char>(term->width * term->fontWidth, 0x0F));
            term->blinkX = 0;
            term->blinkY = 0;
        }

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
        if (::config.http_enable) load_library(this, coro, http_lib);
        lua_getglobal(coro, "redstone");
        lua_setglobal(coro, "rs");

        // Load overridden IO library
        lua_pushcfunction(L, luaopen_io);
        lua_pushstring(L, "io");
        lua_call(L, 1, 0);

        // Load any plugins available
        lua_getglobal(L, "package");
        lua_pushstring(L, "loadlib");
        lua_gettable(L, -2);
        struct dirent *dir;
        DIR * d = opendir((std::string(getROMPath()) + "/plugins").c_str());
        struct stat st;
        if (d) {
            for (int i = 0; (dir = readdir(d)) != NULL; i++) {
                if (stat((std::string(getROMPath()) + "/plugins/" + std::string(dir->d_name)).c_str(), &st) == 0 && S_ISDIR(st.st_mode)) continue;
                std::string api_name = std::string(dir->d_name).substr(0, std::string(dir->d_name).find_last_of('.'));
                lua_pushvalue(L, -1);
                lua_pushfstring(L, "%s/plugins/%s", getROMPath(), dir->d_name);
                lua_pushfstring(L, "luaopen_%s", api_name.c_str());
                if (lua_pcall(L, 2, 2, 0) != 0) { lua_pop(L, 1); continue; }
                if (lua_isnil(L, -2)) {printf("Error loading plugin %s: %s\n", api_name.c_str(), lua_tostring(L, -1)); lua_pop(L, 2); continue;}
                if (lua_isnoneornil(L, -1)) lua_pop(L, 1);
                lua_pushstring(L, getROMPath());
                lua_pushstring(L, getBasePath());
                lua_call(L, 2, 1);
                lua_setglobal(L, api_name.c_str());
            }
            closedir(d);
        }

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
        lua_setglobal(L, "print");
        lua_pushnil(L);
        lua_setglobal(L, "newproxy");
        if (!::config.debug_enable) {
            lua_pushnil(L);
            lua_setglobal(L, "debug");
        }

        // Set default globals
        lua_pushstring(L, ::config.default_computer_settings);
        lua_setglobal(L, "_CC_DEFAULT_SETTINGS");
        pushHostString(L);
        lua_setglobal(L, "_HOST");
        if (headless) {
            lua_pushboolean(L, true);
            lua_setglobal(L, "_HEADLESS");
        }

        /* Load the file containing the script we are going to run */
        char* bios_path_expanded = getBIOSPath();
        std::ifstream in(bios_path_expanded, std::ios::ate);
        size_t sz = in.tellg();
        char* buf = new char[sz];
        in.seekg(0);
        in.read(buf, sz);
        in.close();
        status = luaL_loadbuffer(coro, buf, sz, "@bios.lua");
        delete[] buf;
        free(bios_path_expanded);
        if (status) {
            /* If something went wrong, error message is at the top of */
            /* the stack */
            fprintf(stderr, "Couldn't load file: %s\n", lua_tostring(L, -1));
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
            return;
        }
        std::thread tid;
        if (!headless) {
            tid = std::thread(&termRenderLoop, this);
            setThreadName(tid, std::string("Computer " + std::to_string(id) + " Render Thread").c_str());
        }

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
                if (!headless) tid.join();
                //usleep(5000000);
                printf("%s\n", lua_tostring(coro, -1));
                for (int i = 0; i < sizeof(libraries) / sizeof(library_t*); i++) 
                    if (libraries[i]->deinit != NULL) libraries[i]->deinit(this);
                lua_close(L);
                L = NULL;
                return;
            }
        }

        if (!headless) tid.join();
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

std::list<std::thread*> computerThreads;

Computer * startComputer(int id) {
    Computer * comp = new Computer(id);
    computers.push_back(comp);
    std::thread * th = new std::thread(computerThread, comp);
    setThreadName(*th, std::string("Computer " + std::to_string(id) + " Main Thread").c_str());
    computerThreads.push_back(th);
    return comp;
}