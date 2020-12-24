/*
 * Computer.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods of the Computer class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

extern "C" {
#include <lualib.h>
}
#include <dirent.h>
#include <thread>
#include <unordered_set>
#include <configuration.hpp>
#include <peripheral.hpp>
#include <sys/stat.h>
#include "apis.hpp"
#include "main.hpp"
#include "peripheral/computer.hpp"
#include "platform.hpp"
#include "runtime.hpp"
#include "terminal/SDLTerminal.hpp"
#include "terminal/CLITerminal.hpp"
#include "terminal/RawTerminal.hpp"
#include "terminal/TRoRTerminal.hpp"
#include "terminal/HardwareSDLTerminal.hpp"
#include "termsupport.hpp"

#ifdef WIN32
#define PATH_SEPC '\\'
#else
#define PATH_SEPC '/'
#endif

#ifdef STANDALONE_ROM
extern FileEntry standaloneROM;
extern FileEntry standaloneDebug;
extern std::string standaloneBIOS;
#endif

extern Uint32 eventTimeoutEvent(Uint32 interval, void* param);
extern void stopWebsocket(void*);
extern int term_benchmark(lua_State *L);
extern int onboardingMode;
ProtectedObject<std::vector<Computer*> > computers;
std::unordered_set<Computer*> freedComputers; 
ProtectedObject<std::unordered_set<SDL_TimerID>> freedTimers;
path_t computerDir;
std::unordered_map<int, path_t> customDataDirs;
std::list<path_t> customPlugins;
std::list<std::tuple<std::string, std::string, int> > customMounts;
std::unordered_set<Terminal*> orphanedTerminals;

// Basic CraftOS libraries
library_t * libraries[] = {
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
Computer::Computer(int i, bool debug): isDebugger(debug) {
    id = i;
    // Load config
    const computer_configuration _config = getComputerConfig(id);
    // Create the terminal
    const std::string term_title = _config.label.empty() ? "CraftOS Terminal: " + std::string(debug ? "Debugger" : "Computer") + " " + std::to_string(id) : "CraftOS Terminal: " + asciify(_config.label);
    if (selectedRenderer == 1) term = NULL;
#ifndef NO_CLI
    else if (selectedRenderer == 2) term = new CLITerminal(term_title);
#endif
    else if (selectedRenderer == 3) term = new RawTerminal(term_title);
    else if (selectedRenderer == 4) term = new TRoRTerminal(term_title);
    else if (selectedRenderer == 5) term = new HardwareSDLTerminal(term_title);
    else term = new SDLTerminal(term_title);
    if (term) term->grayscale = !_config.isColor;
    // Tell the mounter it's initializing to prevent checking rom remounts
    mounter_initializing = true;
#ifdef STANDALONE_ROM
    addVirtualMount(this, standaloneROM, "rom");
    if (debug) addVirtualMount(this, standaloneDebug, "debug");
#else
#ifdef _WIN32
    if (!addMount(this, getROMPath() + WS("\\rom"), "rom", ::config.romReadOnly)) { if (::config.standardsMode && term) { displayFailure(term, "Cannot mount ROM"); orphanedTerminals.insert(term); } else delete term; throw std::runtime_error("Could not mount ROM"); }
    if (debug) if (!addMount(this, getROMPath() + WS("\\debug"), "debug", true)) { if (::config.standardsMode && term) { displayFailure(term, "Cannot mount ROM"); orphanedTerminals.insert(term); } else delete term; throw std::runtime_error("Could not mount debugger ROM"); }
#else
    if (!addMount(this, getROMPath() + WS("/rom"), "rom", ::config.romReadOnly)) { if (::config.standardsMode && term) { displayFailure(term, "Cannot mount ROM"); orphanedTerminals.insert(term); } else if (term) delete term; throw std::runtime_error("Could not mount ROM"); }
    if (debug) if (!addMount(this, getROMPath() + WS("/debug"), "debug", true)) { if (::config.standardsMode && term) { displayFailure(term, "Cannot mount ROM"); orphanedTerminals.insert(term); } else if (term) delete term; throw std::runtime_error("Could not mount debugger ROM"); }
#endif // _WIN32
#endif // STANDALONE_ROM
    // Mount custom directories from the command line
    for (auto m : customMounts) {
        switch (std::get<2>(m)) {
            case -1: if (::config.mount_mode != MOUNT_MODE_NONE) addMount(this, wstr(std::get<1>(m)), std::get<0>(m).c_str(), ::config.mount_mode != MOUNT_MODE_RW); break; // use default mode
            case 0: addMount(this, wstr(std::get<1>(m)), std::get<0>(m).c_str(), true); break; // force RO
            default: addMount(this, wstr(std::get<1>(m)), std::get<0>(m).c_str(), false); break; // force RW
        }
    }
    mounter_initializing = false;
    // Get the computer's data directory
    if (customDataDirs.find(id) != customDataDirs.end()) dataDir = customDataDirs[id];
#ifdef _WIN32
    else dataDir = computerDir + WS("\\") + to_path_t(id);
#else
    else dataDir = computerDir + WS("/") + to_path_t(id);
#endif
    // Create the root directory
    createDirectory(dataDir);
    config = new computer_configuration(_config);
}

// Destructor
Computer::~Computer() {
    // Deinitialize any plugins that registered a destructor
    for (const auto& d : userdata_destructors) d.second(this, d.first, userdata[d.first]);
    // Destroy terminal
    if (term != NULL) {
        if (term->errorMode) orphanedTerminals.insert(term);
        else delete term;
    }
    // Save config
    setComputerConfig(id, *config);
    delete config;
    // Deinitialize all peripherals
    for (const auto& p : peripherals) p.second->getDestructor()(p.second);
    for (auto c = referencers.begin(); c != referencers.end(); ++c) {
        std::lock_guard<std::mutex> lock((*c)->peripherals_mutex);
        for (auto it = (*c)->peripherals.begin(); it != (*c)->peripherals.end(); ++it) {
            if (std::string(it->second->getMethods().name) == "computer" && ((computer*)it->second)->comp == this) {
                // Detach computer peripherals pointing to this on other computers
                delete (computer*)it->second;
                it = (*c)->peripherals.erase(it);
                if (it == (*c)->peripherals.end()) break;
            }
        }
        if (c == referencers.end()) break;
    }
    // Mark all currently running timers as invalid
    {
        LockGuard lock(freedTimers);
        for (SDL_TimerID t : timerIDs) freedTimers->insert(t);
    }
    // Cancel the mouse_move debounce timer if active
    if (mouseMoveDebounceTimer != 0) SDL_RemoveTimer(mouseMoveDebounceTimer);
    if (eventTimeout != 0) SDL_RemoveTimer(eventTimeout);
    // Stop all open websockets
    while (!openWebsockets.empty()) stopWebsocket(*openWebsockets.begin());
}

extern "C" {
    /* export */ int db_breakpoint(lua_State *L) {
        Computer * computer = get_comp(L);
        const int id = !computer->breakpoints.empty() ? computer->breakpoints.rbegin()->first + 1 : 1;
        computer->breakpoints[id] = std::make_pair("@/" + astr(fixpath(computer, luaL_checkstring(L, 1), false, false)), luaL_checkinteger(L, 2));
        if (!computer->hasBreakpoints) computer->forceCheckTimeout = true;
        computer->hasBreakpoints = true;
        lua_sethook(computer->L, termHook, LUA_MASKCOUNT | LUA_MASKLINE | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
        lua_sethook(L, termHook, LUA_MASKCOUNT | LUA_MASKLINE | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
        lua_pushinteger(L, id);
        return 1;
    }

    /* export */ int db_unsetbreakpoint(lua_State *L) {
        Computer * computer = get_comp(L);
        if (computer->breakpoints.find((int)luaL_checkinteger(L, 1)) != computer->breakpoints.end()) {
            computer->breakpoints.erase((int)lua_tointeger(L, 1));
            if (computer->breakpoints.empty()) {
                computer->hasBreakpoints = false;
                lua_sethook(computer->L, termHook, LUA_MASKCOUNT | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
                lua_sethook(L, termHook, LUA_MASKCOUNT | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
            }
            lua_pushboolean(L, true);
        } else lua_pushboolean(L, false);
        return 1;
    }

    /* export */ void setcompmask_(lua_State *L, int mask) {
        Computer * comp = get_comp(L);
        comp->hookMask = mask;
    }
}


static const char * file_reader(lua_State *L, void * ud, size_t *size) {
    static char file_read_tmp[4096];
    if (feof((FILE*)ud)) return NULL;
    *size = fread(file_read_tmp, 1, 4096, (FILE*)ud);
    return file_read_tmp;
}

// Main computer loop
void runComputer(Computer * self, const path_t& bios_name) {
    if (self->config->startFullscreen && dynamic_cast<SDLTerminal*>(self->term) != NULL) ((SDLTerminal*)self->term)->toggleFullscreen();
    self->running = 1;
    if (self->L != NULL) lua_close(self->L);
    setjmp(self->on_panic);
    while (self->running) {
        int status;
        if (self->term != NULL) {
            // Initialize terminal contents
            std::lock_guard<std::mutex> lock(self->term->locked);
            self->term->blinkX = 0;
            self->term->blinkY = 0;
            self->term->screen = vector2d<unsigned char>(self->term->width, self->term->height, ' ');
            self->term->colors = vector2d<unsigned char>(self->term->width, self->term->height, 0xF0);
            self->term->pixels = vector2d<unsigned char>(self->term->width * Terminal::fontWidth, self->term->height * Terminal::fontHeight, 0x0F);
            self->term->pixelBuffer = vector2d<unsigned char>(self->term->width * Terminal::fontWidth, self->term->height * Terminal::fontHeight, 0x0F);
            memcpy(self->term->palette, defaultPalette, sizeof(defaultPalette));
            self->term->mode = 0;
        }
        self->colors = 0xF0;

        /*
        * All Lua contexts are held in this structure. We work with it almost
        * all the time.
        */
        lua_State *L = self->L = luaL_newstate();

        self->coro = lua_newthread(L);
        self->paramQueue = lua_newthread(L);
        while (!self->eventQueue.empty()) self->eventQueue.pop();

        // Reinitialize any peripherals that were connected before rebooting
        for (auto p : self->peripherals) p.second->reinitialize(L);

        // Push reference to this to the registry
        lua_pushinteger(L, 1);
        lua_pushlightuserdata(L, self);
        lua_settable(L, LUA_REGISTRYINDEX);
        if (::config.debug_enable) {
            lua_newtable(L);
            lua_newtable(L);
            lua_pushstring(L, "v");
            lua_setfield(L, -2, "__mode");
            lua_setmetatable(L, -2);
            lua_setfield(L, LUA_REGISTRYINDEX, "_coroutine_stack");
        }

        // Load libraries
        luaL_openlibs(self->coro);
        lua_getglobal(L, "os");
        lua_getfield(L, -1, "date");
        lua_setglobal(L, "os_date");
        lua_pop(L, 1);
        if (self->debugger != NULL && !self->isDebugger) lua_sethook(self->coro, termHook, LUA_MASKCOUNT | LUA_MASKLINE | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
        else if (config.debug_enable && !self->isDebugger) lua_sethook(self->coro, termHook, LUA_MASKCOUNT | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
        else lua_sethook(self->coro, termHook, LUA_MASKCOUNT | LUA_MASKERROR, 1000000);
        lua_atpanic(L, termPanic);
        for (library_t * lib : libraries) load_library(self, self->coro, *lib);
        if (config.http_enable) load_library(self, self->coro, http_lib);
        if (self->isDebugger && self->debugger != NULL) load_library(self, self->coro, *((library_t*)self->debugger));
        lua_getglobal(self->coro, "redstone");
        lua_setglobal(self->coro, "rs");
        lua_getglobal(L, "os");
        lua_getglobal(L, "os_date");
        lua_setfield(L, -2, "date");
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_setglobal(L, "os_date");

        // Load any plugins available
        if (!config.vanilla) {
            if (!globalPluginErrors.empty()) {
                lua_getglobal(L, "_CCPC_PLUGIN_ERRORS");
                if (lua_isnil(L, -1)) {
                    lua_newtable(L);
                    lua_pushvalue(L, -1);
                    lua_setglobal(L, "_CCPC_PLUGIN_ERRORS");
                }
                for (const auto& err : globalPluginErrors) {
                    path_t bname = err.first.substr(err.first.find_last_of(PATH_SEPC) + 1);
                    lua_pushstring(L, astr(bname.substr(0, bname.find_first_of('.'))).c_str());
                    lua_pushstring(L, err.second.c_str());
                    lua_settable(L, -3);
                }
                lua_pop(L, 1);
            }
            loadPlugins(self);
        }

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
        if (!config.debug_enable) {
            lua_pushnil(L);
            lua_setglobal(L, "collectgarbage");
            lua_pushnil(L);
            lua_setglobal(L, "debug");
            lua_pushnil(L);
            lua_setglobal(L, "newproxy");
        }
        if (config.vanilla) {
            lua_pushnil(L);
            lua_setglobal(L, "config");
            lua_pushnil(L);
            lua_setglobal(L, "mounter");
            lua_pushnil(L);
            lua_setglobal(L, "periphemu");
            lua_getglobal(L, "term");
            lua_pushnil(L);
            lua_setfield(L, -2, "getGraphicsMode");
            lua_pushnil(L);
            lua_setfield(L, -2, "setGraphicsMode");
            lua_pushnil(L);
            lua_setfield(L, -2, "getPixel");
            lua_pushnil(L);
            lua_setfield(L, -2, "setPixel");
            lua_pushnil(L);
            lua_setfield(L, -2, "drawPixels");
            lua_pushnil(L);
            lua_setfield(L, -2, "screenshot");
            lua_pop(L, 1);
            if (config.http_enable) {
                lua_getglobal(L, "http");
                lua_pushnil(L);
                lua_setfield(L, -2, "addListener");
                lua_pushnil(L);
                lua_setfield(L, -2, "removeListener");
                lua_pop(L, 1);
            }
            if (config.debug_enable) {
                lua_getglobal(L, "debug");
                lua_pushnil(L);
                lua_setfield(L, -2, "setbreakpoint");
                lua_pushnil(L);
                lua_setfield(L, -2, "unsetbreakpoint");
                lua_pop(L, 1);
            }
        }
        if (config.serverMode) {
            lua_getglobal(L, "http");
            lua_pushnil(L);
            lua_setfield(L, -2, "addListener");
            lua_pushnil(L);
            lua_setfield(L, -2, "removeListener");
            lua_pop(L, 1);
            lua_pushnil(L);
            lua_setglobal(L, "mounter");
        }

        // Set default globals
        lua_pushstring(L, ::config.default_computer_settings.c_str());
        lua_setglobal(L, "_CC_DEFAULT_SETTINGS");
        lua_pushboolean(L, ::config.disable_lua51_features);
        lua_setglobal(L, "_CC_DISABLE_LUA51_FEATURES");
#if CRAFTOSPC_INDEV == true && defined(CRAFTOSPC_COMMIT)
        lua_pushstring(L, "ComputerCraft " CRAFTOSPC_CC_VERSION " (CraftOS-PC " CRAFTOSPC_VERSION "@" CRAFTOSPC_COMMIT ")");
#else
        lua_pushstring(L, "ComputerCraft " CRAFTOSPC_CC_VERSION " (CraftOS-PC " CRAFTOSPC_VERSION ")");
#endif
        lua_setglobal(L, "_HOST");
        if (selectedRenderer == 1) {
            lua_pushboolean(L, true);
            lua_setglobal(L, "_HEADLESS");
        }
        if (onboardingMode == 1) {
            lua_pushboolean(L, true);
            lua_setglobal(L, "_CCPC_FIRST_RUN");
            onboardingMode = 0;
        } else if (onboardingMode == 2) {
            lua_pushboolean(L, true);
            lua_setglobal(L, "_CCPC_UPDATED_VERSION");
            onboardingMode = 0;
        }
        if (!script_file.empty()) {
            std::string script;
            if (script_file[0] == '\x1b') script = script_file.substr(1);
            else {
                FILE* in = fopen(script_file.c_str(), "r");
                char tmp[4096];
                while (!feof(in)) {
                    const size_t read = fread(tmp, 1, 4096, in);
                    if (read == 0) break;
                    script += std::string(tmp, read);
                }
                fclose(in);
            }
            lua_pushlstring(L, script.c_str(), script.size());
            lua_setglobal(L, "_CCPC_STARTUP_SCRIPT");
        }
        if (!script_args.empty()) {
            lua_pushlstring(L, script_args.c_str(), script_args.length());
            lua_setglobal(L, "_CCPC_STARTUP_ARGS");
        }
        lua_pushcfunction(L, term_benchmark);
        lua_setfield(L, LUA_REGISTRYINDEX, "benchmark");

        /* Load the file containing the script we are going to run */
#ifdef STANDALONE_ROM
        status = luaL_loadstring(self->coro, astr(bios_name).c_str());
        path_t bios_path_expanded = WS("standalone ROM");
#else
#ifdef WIN32
        path_t bios_path_expanded = getROMPath() + WS("\\") + bios_name;
#else
        path_t bios_path_expanded = getROMPath() + WS("/") + bios_name;
#endif
        FILE * bios_file = platform_fopen(bios_path_expanded.c_str(), "r");
        status = lua_load(self->coro, file_reader, bios_file, "@bios.lua");
        fclose(bios_file);
#endif
        if (status || !lua_isfunction(self->coro, -1)) {
            /* If something went wrong, error message is at the top of */
            /* the stack */
            fprintf(stderr, "Couldn't load BIOS: %s (%s). Please make sure the CraftOS ROM is installed properly. (See https://www.craftos-pc.cc/docs/error-messages for more information.)\n", astr(bios_path_expanded).c_str(), lua_tostring(L, -1));
            if (::config.standardsMode) displayFailure(self->term, "Error loading bios.lua");
            else queueTask([bios_path_expanded](void* term)->void*{
                ((Terminal*)term)->showMessage(
                    SDL_MESSAGEBOX_ERROR, "Couldn't load BIOS", 
                    std::string(
                        "Couldn't load BIOS from " + astr(bios_path_expanded) + ". Please make sure the CraftOS ROM is installed properly. (See https://www.craftos-pc.cc/docs/error-messages for more information.)"
                    ).c_str()
                ); 
                return NULL;
            }, self->term);
            return;
        }

        /* Ask Lua to run our little script */
        status = LUA_YIELD;
        int narg = 0;
        self->running = 1;
        self->eventTimeout = SDL_AddTimer(::config.standardsMode ? 7000 : ::config.abortTimeout, eventTimeoutEvent, self);
        while (status == LUA_YIELD && self->running == 1) {
            status = lua_resume(self->coro, narg);
            if (status == LUA_YIELD) {
                if (lua_isstring(self->coro, -1)) narg = getNextEvent(self->coro, std::string(lua_tostring(self->coro, -1), lua_strlen(self->coro, -1)));
                else narg = getNextEvent(self->coro, "");
            } else if (status != 0) {
                // Catch runtime error
                self->running = 0;
                lua_pushcfunction(self->coro, termPanic);
                if (lua_isstring(self->coro, -2)) lua_pushvalue(self->coro, -2);
                else lua_pushnil(self->coro);
                lua_call(self->coro, 1, 0);
                break;
            } else self->running = 0;
        }
        
        // Shutdown threads
        self->event_lock.notify_all();
        for (library_t * lib : libraries) if (lib->deinit != NULL) lib->deinit(self);
        lua_close(L);   /* Cya, Lua */
        self->L = NULL;
    }
}

// Gets the next event for the given computer
bool Computer_getEvent(Computer * self, SDL_Event* e) {
    std::lock_guard<std::mutex> lock(self->termEventQueueMutex);
    if (self->termEventQueue.empty()) return false;
    SDL_Event& front = self->termEventQueue.front();
    if (&front == NULL || e == NULL) return false;
    memcpy(e, &front, sizeof(SDL_Event));
    self->termEventQueue.pop();
    return true;
}

// Thread wrapper for running a computer
void* computerThread(void* data) {
    Computer * comp = (Computer*)data;
#ifdef __APPLE__
    pthread_setname_np(std::string("Computer " + std::to_string(comp->id) + " Thread").c_str());
#endif
    // seed the Lua RNG
    srand(std::chrono::high_resolution_clock::now().time_since_epoch().count() & UINT_MAX);
    // in case the allocator decides to reuse pointers
    if (freedComputers.find(comp) != freedComputers.end())
        freedComputers.erase(comp);
    try {
#ifdef STANDALONE_ROM
        runComputer(comp, wstr(standaloneBIOS));
#else
        runComputer(comp, WS("bios.lua"));
#endif
    } catch (std::exception &e) {
        fprintf(stderr, "Uncaught exception while executing computer %d: %s\n", comp->id, e.what());
        queueTask([e](void*t)->void* {const std::string m = std::string("Uh oh, an uncaught exception has occurred! Please report this to https://www.craftos-pc.cc/bugreport. When writing the report, include the following exception message: \"Exception on computer thread: ") + e.what() + "\". The computer will now shut down.";  if (t != NULL) ((Terminal*)t)->showMessage(SDL_MESSAGEBOX_ERROR, "Uncaught Exception", m.c_str()); else if (selectedRenderer == 0 || selectedRenderer == 5) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Uncaught Exception", m.c_str(), NULL); return NULL; }, comp->term);
        if (comp->L != NULL) {
            comp->event_lock.notify_all();
            for (library_t * lib : libraries) if (lib->deinit != NULL) lib->deinit(comp);
            lua_close(comp->L);   /* Cya, Lua */
            comp->L = NULL;
        }
    }
    freedComputers.insert(comp);
    {
        LockGuard lock(computers);
        for (auto it = computers->begin(); it != computers->end(); ++it) {
            if (*it == comp) {
                it = computers->erase(it);
                queueTask([](void* arg)->void* {delete (Computer*)arg; return NULL;}, comp);
                if (it == computers->end()) break;
            }
        }
    }
    if (selectedRenderer != 0 && selectedRenderer != 2 && selectedRenderer != 5 && !exiting) {
        {LockGuard lock(taskQueue);}
        while (taskQueueReady && !exiting) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        taskQueueReady = true;
        taskQueueNotify.notify_all();
        while (taskQueueReady && !exiting) {std::this_thread::yield(); taskQueueNotify.notify_all();}
    }
    return NULL;
}

/* export */ std::list<std::thread*> computerThreads;

// Spin up a new computer
Computer * startComputer(int id) {
    Computer * comp;
    try {comp = new Computer(id);} catch (std::exception &e) {
        if ((selectedRenderer == 0 || selectedRenderer == 5) && !config.standardsMode) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to open computer", std::string("An error occurred while opening the computer session: " + std::string(e.what()) + ". See https://www.craftos-pc.cc/docs/error-messages for more info.").c_str(), NULL);
        else fprintf(stderr, "An error occurred while opening the computer session: %s", e.what());
        return NULL;
    }
    {
        LockGuard lock(computers);
        computers->push_back(comp);
    }
    std::thread * th = new std::thread(computerThread, comp);
    setThreadName(*th, "Computer " + std::to_string(id) + " Thread");
    computerThreads.push_back(th);
    return comp;
}
