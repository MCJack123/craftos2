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
#include "CLITerminalWindow.hpp"
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
#include <dirent.h>
#include <sys/stat.h>
extern "C" {
#include <lauxlib.h>
}

extern bool headless;
extern bool cli;
std::vector<Computer*> computers;
std::unordered_set<Computer*> freedComputers; 

// Basic CraftOS libraries
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

// Constructor
Computer::Computer(int i) {
    id = i;
    // Tell the mounter it's initializing to prevent checking rom remounts
    mounter_initializing = true;
    addMount(this, (getROMPath() + "/rom").c_str(), "rom", ::config.romReadOnly);
    mounter_initializing = false;
    // Create the root directory
#ifdef _WIN32
    createDirectory((std::string(getBasePath()) + "\\computer\\" + std::to_string(id)).c_str());
#else
    createDirectory((std::string(getBasePath()) + "/computer/" + std::to_string(id)).c_str());
#endif
    // Create the terminal
    if (headless) term = NULL;
#ifndef NO_CLI
    else if (cli) term = new CLITerminalWindow("CraftOS Terminal: Computer " + std::to_string(id));
#endif
    else term = new TerminalWindow("CraftOS Terminal: Computer " + std::to_string(id));
    // Load config
    config = getComputerConfig(id);
}

// Destructor
Computer::~Computer() {
    // Destroy terminal
    if (!headless) delete term;
    // Save config
    setComputerConfig(id, config);
    // Deinitialize all peripherals
    for (auto p : peripherals) delete p.second;
    for (std::list<Computer*>::iterator c = referencers.begin(); c != referencers.end(); c++) {
        (*c)->peripherals_mutex.lock();
        for (auto it = (*c)->peripherals.begin(); it != (*c)->peripherals.end(); it++) {
            if (std::string(it->second->getMethods().name) == "computer" && ((computer*)it->second)->comp == this) {
                // Detach computer peripherals pointing to this on other computers
                delete (computer*)it->second;
                it = (*c)->peripherals.erase(it);
                if (it == (*c)->peripherals.end()) break;
            }
        }
        (*c)->peripherals_mutex.unlock();
        if (c == referencers.end()) break;
    }
}

int setthreadenv(lua_State *L) {
    lua_newtable(L);
    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, [](lua_State *L)->int {
        lua_getfield(L, LUA_REGISTRYINDEX, "_coroutine_stack");
        lua_pushthread(L);
        lua_pushnil(L);
        lua_settable(L, -3);
        return 0;
    });
    lua_settable(L, -3);
    lua_setmetatable(L, -2);
    lua_getfield(L, LUA_REGISTRYINDEX, "_coroutine_stack");
    lua_pushthread(L);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushthread(L);
        lua_newtable(L);
        lua_settable(L, -3);
        lua_pushthread(L);
        lua_gettable(L, -2);
    }
    printf("Old stack height: %zu\n", lua_objlen(L, -1));
    lua_pushvalue(L, 1);
    lua_pushvalue(L, -2);
    lua_settable(L, -4);
    lua_pop(L, 2);
    return 0;
}

#define CO_RUN	0	/* running */
#define CO_SUS	1	/* suspended */
#define CO_NOR	2	/* 'normal' (it resumed another coroutine) */
#define CO_DEAD	3

static const char *const statnames[] =
    {"running", "suspended", "normal", "dead"};

static int costatus (lua_State *L, lua_State *co) {
  if (L == co) return CO_RUN;
  switch (lua_status(co)) {
    case LUA_YIELD:
      return CO_SUS;
    case 0: {
      lua_Debug ar;
      if (lua_getstack(co, 0, &ar) > 0)  /* does it have frames? */
        return CO_NOR;  /* it is running */
      else if (lua_gettop(co) == 0)
          return CO_DEAD;
      else
        return CO_SUS;  /* initial state */
    }
    default:  /* some error occured */
      return CO_DEAD;
  }
}

static int auxresume (lua_State *L, lua_State *co, int narg) {
  int status = costatus(L, co);
  if (!lua_checkstack(co, narg))
    luaL_error(L, "too many arguments to resume");
  if (status != CO_SUS) {
    lua_pushfstring(L, "cannot resume %s coroutine", statnames[status]);
    return -1;  /* error flag */
  }
  lua_xmove(L, co, narg);
  lua_setlevel(L, co);
  status = lua_resume(co, narg);
  if (status == 0 || status == LUA_YIELD) {
    int nres = lua_gettop(co);
    if (!lua_checkstack(L, nres + 1))
      luaL_error(L, "too many results to resume");
    lua_xmove(co, L, nres);  /* move yielded values */
    return nres;
  }
  else {
    lua_xmove(co, L, 1);  /* move error message */
    return -1;  /* error flag */
  }
}

int coroutine_resume (lua_State *L) {
    lua_State *co = lua_tothread(L, 1);
    int r;
    luaL_argcheck(L, co, 1, "coroutine expected");
    lua_getfield(L, LUA_REGISTRYINDEX, "_coroutine_stack");
    lua_pushinteger(L, lua_objlen(L, -1) + 1);
    lua_pushvalue(L, 1);
    lua_settable(L, -3);
    lua_pop(L, 1);
    r = auxresume(L, co, lua_gettop(L) - 1);
    lua_getfield(L, LUA_REGISTRYINDEX, "_coroutine_stack");
    lua_pushinteger(L, lua_objlen(L, -1));
    lua_pushnil(L);
    lua_settable(L, -3);
    lua_pop(L, 1);
    if (r < 0) {
        lua_pushboolean(L, 0);
        lua_insert(L, -2);
        return 2;  /* return false + error message */
    }
    else {
        lua_pushboolean(L, 1);
        lua_insert(L, -(r + 1));
        return r + 1;  /* return true + `resume' returns */
    }
}

extern int fs_getName(lua_State *L);

extern "C" {
    extern int db_errorfb (lua_State *L);
    LUA_API int lua_getstack_patch (lua_State *L, int level, lua_Debug *ar, lua_State **L_ret) {
        if (level == 0) return lua_getstack(L, 0, ar);
        lua_getfield(L, LUA_REGISTRYINDEX, "_coroutine_stack");
        for (int i = lua_objlen(L, -1); i > 0; i--) {
            lua_pushinteger(L, i);
            lua_gettable(L, -2);
            *L_ret = lua_tothread(L, -1);
            lua_pop(L, 1);
            int r;
            for (int j = 0; level > 0; level--, j++) 
                if ((r = lua_getstack(*L_ret, j, ar)) == 0) break;
            if (level == 0 && r == 1) {
                lua_pop(L, 1);
                return 1;
            }
        }
        lua_pop(L, 1);
        return 0;
    }

    int db_breakpoint(lua_State *L) {
        if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
        if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
        lua_pushcfunction(L, fs_getName);
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        Computer * computer = get_comp(L);
        computer->breakpoints.push_back(std::make_pair(std::string("@") + lua_tostring(L, 3), lua_tointeger(L, 2)));
        return 0;
    }

    int db_unsetbreakpoint(lua_State *L) {
        if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
        if (!lua_isnumber(L, 2)) bad_argument(L, "number", 2);
        lua_pushcfunction(L, fs_getName);
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        Computer * computer = get_comp(L);
        for (auto it = computer->breakpoints.begin(); it != computer->breakpoints.end(); it++) {
            if (it->first == std::string(lua_tostring(L, 3)) && it->second == lua_tointeger(L, 2)) {
                computer->breakpoints.erase(it);
                lua_pushboolean(L, true);
                break;
            }
        }
        if (!lua_isboolean(L, -1)) lua_pushboolean(L, false);
        return 1;
    }
}

static void getfunc (lua_State *L, int opt) {
    if (lua_isfunction(L, 1)) lua_pushvalue(L, 1);
    else {
        lua_Debug ar;
        int level = opt ? luaL_optint(L, 1, 1) : luaL_checkint(L, 1);
        luaL_argcheck(L, level >= 0, 1, "level must be non-negative");
        lua_State *L_ret;
        if (lua_getstack_patch(L, level, &ar, &L_ret) == 0)
            luaL_argerror(L, 1, "invalid level");
        lua_getinfo(L_ret, "f", &ar);
        if (lua_isnil(L, -1))
            luaL_error(L, "no function environment for tail call at level %d",
                            level);
    }
}

static int luaB_getfenv (lua_State *L) {
    getfunc(L, 1);
    if (lua_iscfunction(L, -1))  /* is a C function? */
        lua_pushvalue(L, LUA_GLOBALSINDEX);  /* return the thread's global env. */
    else
        lua_getfenv(L, -1);
    return 1;
}

static int luaB_setfenv (lua_State *L) {
    luaL_checktype(L, 2, LUA_TTABLE);
    getfunc(L, 0);
    lua_pushvalue(L, 2);
    if (lua_isnumber(L, 1) && lua_tonumber(L, 1) == 0) {
        /* change environment of current thread */
        lua_pushthread(L);
        lua_insert(L, -2);
        lua_setfenv(L, -2);
        return 0;
    }
    else if (lua_iscfunction(L, -2) || lua_setfenv(L, -2) == 0)
        luaL_error(L,
            LUA_QL("setfenv") " cannot change environment of given object");
    return 1;
}

void luaL_where (lua_State *L, int level) {
  lua_Debug ar;
  lua_State * L_ret;
  if (lua_getstack_patch(L, level, &ar, &L_ret)) {  /* check function at level */
    lua_getinfo(L_ret, "Sl", &ar);  /* get info about it */
    if (ar.currentline > 0) {  /* is there info? */
      lua_pushfstring(L, "%s:%d: ", ar.short_src, ar.currentline);
      return;
    }
  }
  lua_pushliteral(L, "");  /* else, no information available... */
}

static int luaB_error (lua_State *L) {
  int level = luaL_optint(L, 2, 1);
  lua_settop(L, 1);
  if (lua_isstring(L, 1) && level > 0) {  /* add extra information? */
    if (config.logErrors) {
        lua_pushcfunction(L, db_errorfb);
        lua_pushvalue(L, 1);
        lua_call(L, 1, 1);
        printf("%s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
    luaL_where(L, level);
    lua_pushvalue(L, 1);
    lua_concat(L, 2);
  }
  return lua_error(L);
}

// Main computer loop
void Computer::run() {
    running = 1;
    if (L != NULL) lua_close(L);
    setjmp(on_panic);
    while (running) {
        int status;
        lua_State *coro;
        if (!headless) {
            // Initialize terminal contents
            term->locked.lock();
            term->blinkX = 0;
            term->blinkY = 0;
            term->screen = std::vector<std::vector<char> >(term->height, std::vector<char>(term->width, ' '));
            term->colors = std::vector<std::vector<unsigned char> >(term->height, std::vector<unsigned char>(term->width, 0xF0));
            term->pixels = std::vector<std::vector<unsigned char> >(term->height * term->fontHeight, std::vector<unsigned char>(term->width * term->fontWidth, 0x0F));
            memcpy(term->palette, defaultPalette, sizeof(defaultPalette));
            term->locked.unlock();
        }
        colors = 0xF0;

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
        lua_newtable(L);
        lua_setfield(L, LUA_REGISTRYINDEX, "_coroutine_stack");

        // Load libraries
        luaL_openlibs(coro);
        lua_sethook(coro, termHook, LUA_MASKCOUNT | LUA_MASKLINE, 100);
        lua_atpanic(L, termPanic);
        for (unsigned i = 0; i < sizeof(libraries) / sizeof(library_t*); i++) load_library(this, coro, *libraries[i]);
        if (::config.http_enable) load_library(this, coro, http_lib);
        lua_getglobal(coro, "redstone");
        lua_setglobal(coro, "rs");

        // Load overridden IO & debug library
        lua_pushcfunction(L, luaopen_io);
        lua_pushstring(L, "io");
        lua_call(L, 1, 0);
        lua_pushcfunction(L, luaopen_debug);
        lua_pushstring(L, "debug");
        lua_call(L, 1, 0);

        // Load overridden [sg]etfenv, error functions
        lua_pushcfunction(L, luaB_getfenv);
        lua_setglobal(L, "getfenv");
        lua_pushcfunction(L, luaB_setfenv);
        lua_setglobal(L, "setfenv");
        lua_pushcfunction(L, luaB_error);
        lua_setglobal(L, "error");

        // Load any plugins available
        lua_getglobal(L, "package");
        lua_pushstring(L, "loadlib");
        lua_gettable(L, -2);
        struct dirent *dir;
        std::string plugin_path = getPlugInPath();
        DIR * d = opendir(plugin_path.c_str());
        struct stat st;
        if (d) {
            for (int i = 0; (dir = readdir(d)) != NULL; i++) {
                if (stat((plugin_path + "/" + std::string(dir->d_name)).c_str(), &st) == 0 && S_ISDIR(st.st_mode)) continue;
                std::string api_name = std::string(dir->d_name).substr(0, std::string(dir->d_name).find_last_of('.'));
                lua_pushvalue(L, -1);
                lua_pushfstring(L, "%s/%s", plugin_path.c_str(), dir->d_name);
                lua_pushfstring(L, "luaopen_%s", api_name.c_str());
                if (lua_pcall(L, 2, 2, 0) != 0) { lua_pop(L, 1); continue; }
                if (lua_isnil(L, -2)) {printf("Error loading plugin %s: %s\n", api_name.c_str(), lua_tostring(L, -1)); lua_pop(L, 2); continue;}
                if (lua_isnoneornil(L, -1)) lua_pop(L, 1);
                lua_pushstring(L, getROMPath().c_str());
                lua_pushstring(L, getBasePath().c_str());
                lua_call(L, 2, 1);
                lua_setglobal(L, api_name.c_str());
            }
            closedir(d);
        } //else printf("Could not open plugins from %s\n", plugin_path.c_str());

        // Delete unwanted globals
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
        if (!::config.debug_enable) {
            lua_pushnil(L);
            lua_setglobal(L, "collectgarbage");
            lua_pushnil(L);
            lua_setglobal(L, "debug");
            lua_pushnil(L);
            lua_setglobal(L, "newproxy");
        }

        // Set default globals
        lua_pushstring(L, ::config.default_computer_settings.c_str());
        lua_setglobal(L, "_CC_DEFAULT_SETTINGS");
        lua_pushboolean(L, ::config.disable_lua51_features);
        lua_setglobal(L, "_CC_DISABLE_LUA51_FEATURES");
        pushHostString(L);
        lua_setglobal(L, "_HOST");
        if (headless) {
            lua_pushboolean(L, true);
            lua_setglobal(L, "_HEADLESS");
        }

        lua_getglobal(L, "coroutine");
        lua_pushstring(L, "resume");
        lua_pushcfunction(L, coroutine_resume);
        lua_settable(L, -3);
        lua_pop(L, 1);

        // Load patched pcall/xpcall
        luaL_loadstring(L, "local nativeResume = coroutine.resume\n\
local setthreadenv, printf = ...\n\
return function( _fn, _fnErrorHandler )\n\
    local typeT = type( _fn )\n\
    assert( typeT == \"function\", \"bad argument #1 to xpcall (function expected, got \"..typeT..\")\" )\n\
    local co = coroutine.create( _fn )\n\
    --setthreadenv( co )\n\
    --printf(#getregistry('_coroutine_stack')[co], #getregistry('_coroutine_stack')[coroutine.running()])\n\
    --assert(getregistry('_coroutine_stack')[co] == getregistry('_coroutine_stack')[coroutine.running()])\n\
    local tResults = { nativeResume( co ) }\n\
    while coroutine.status( co ) ~= \"dead\" do\n\
        tResults = { nativeResume( co, coroutine.yield( unpack( tResults, 2 ) ) ) }\n\
    end\n\
    if tResults[1] == true then\n\
        return true, unpack( tResults, 2 )\n\
    else\n\
        return false, _fnErrorHandler( tResults[2] )\n\
    end\n\
end");
        lua_pushcfunction(L, setthreadenv);
        lua_pushcfunction(L, [](lua_State *L)->int {
            for (int i = 1; i <= lua_gettop(L); i++) printf("%s\t", lua_tostring(L, i));
            printf("\n");
            return 0;
        });
        lua_call(L, 2, 1);
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
        std::string bios_path_expanded = getROMPath() + "/bios.lua";
        status = luaL_loadfile(coro, bios_path_expanded.c_str());
        if (status) {
            /* If something went wrong, error message is at the top of */
            /* the stack */
            fprintf(stderr, "Couldn't load BIOS: %s (%s). Please make sure the CraftOS ROM is installed properly. (See https://github.com/MCJack123/craftos2-rom for more information.)\n", bios_path_expanded.c_str(), lua_tostring(L, -1));
            queueTask([bios_path_expanded](void* term)->void*{
                ((TerminalWindow*)term)->showMessage(
                    SDL_MESSAGEBOX_ERROR, "Couldn't load BIOS", 
                    std::string(
                        "Couldn't load BIOS from " + bios_path_expanded + ". Please make sure the CraftOS ROM is installed properly. (See https://github.com/MCJack123/craftos2-rom for more information.)"
                    ).c_str()
                ); 
                return NULL;
            }, term);
            return;
        }

        /* Ask Lua to run our little script */
        status = LUA_YIELD;
        int narg = 0;
        running = 1;
        while (status == LUA_YIELD && running == 1) {
            status = lua_resume(coro, narg);
            if (status == LUA_YIELD) {
                if (lua_isstring(coro, -1)) narg = getNextEvent(coro, std::string(lua_tostring(coro, -1), lua_strlen(coro, -1)));
                else narg = getNextEvent(coro, "");
            } else if (status != 0) {
                // Catch runtime error
                running = 0;
                lua_pushcfunction(L, termPanic);
                lua_call(L, 1, 0);
                return;
            }
        }
        
        // Shutdown threads
        event_lock.notify_all();
        for (unsigned i = 0; i < sizeof(libraries) / sizeof(library_t*); i++) 
            if (libraries[i]->deinit != NULL) libraries[i]->deinit(this);
        lua_close(L);   /* Cya, Lua */
        L = NULL;
    }
}

// Gets the next event for the given computer
bool Computer::getEvent(SDL_Event* e) {
    if (termEventQueue.size() == 0) return false;
    memcpy(e, &termEventQueue.front(), sizeof(SDL_Event));
    termEventQueue.pop();
    return true;
}

// Thread wrapper for running a computer
void* computerThread(void* data) {
    Computer * comp = (Computer*)data;
#ifdef __APPLE__
    pthread_setname_np(std::string("Computer " + std::to_string(comp->id) + " Thread").c_str());
#endif
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

// Spin up a new computer
Computer * startComputer(int id) {
    Computer * comp = new Computer(id);
    computers.push_back(comp);
    std::thread * th = new std::thread(computerThread, comp);
    setThreadName(*th, std::string("Computer " + std::to_string(id) + " Main Thread").c_str());
    computerThreads.push_back(th);
    return comp;
}