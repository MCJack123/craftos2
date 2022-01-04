/*
 * Computer.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods of the Computer class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
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
#include "terminal/RawTerminal.hpp"
#include "termsupport.hpp"

#ifdef __ANDROID__
extern "C" {extern int Android_JNI_SetupThread(void);}
#endif

#ifdef STANDALONE_ROM
extern FileEntry standaloneROM;
extern FileEntry standaloneDebug;
extern std::string standaloneBIOS;
#endif

extern Uint32 eventTimeoutEvent(Uint32 interval, void* param);
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

// Context structure for yieldable load
struct load_ctx {
    std::thread thread;
    std::mutex lock;
    std::condition_variable notify;
    int oldtop;
    int status;
    int argcount;
    lua_State *L;
    lua_State *coro;
    const char * name;
};

// Basic CraftOS libraries
library_t * libraries[] = {
    &config_lib,
    &fs_lib,
#ifndef NO_MOUNTER
    &mounter_lib,
#endif
    &os_lib,
    &peripheral_lib,
    &periphemu_lib,
    &rs_lib,
    &term_lib,
    NULL
};

// Constructor
Computer::Computer(int i, bool debug): isDebugger(debug) {
    id = i;
    // Load config
    const computer_configuration _config = getComputerConfig(id);
    // Create the terminal
    const std::string term_title = _config.label.empty() ? "CraftOS Terminal: " + std::string(debug ? "Debugger" : "Computer") + " " + std::to_string(id) : "CraftOS Terminal: " + asciify(_config.label);
    term = createTerminal(term_title);
    if (selectedRenderer == 3) ((RawTerminal*)term)->computerID = id + 1;
    if (term) {
        term->grayscale = !_config.isColor;
        unsigned w = term->width, h = term->height;
        if (_config.computerWidth > 0) w = (unsigned)_config.computerWidth;
        if (_config.computerHeight > 0) h = (unsigned)_config.computerHeight;
        if (w != term->width || h != term->height) term->resize(w, h);
    }
    // Tell the mounter it's initializing to prevent checking rom remounts
    mounter_initializing = true;
#ifdef STANDALONE_ROM
    addVirtualMount(this, standaloneROM, "rom");
    if (debug) addVirtualMount(this, standaloneDebug, "debug");
#else
#ifdef _WIN32
    if (!addMount(this, getROMPath() + WS("\\rom"), "rom", ::config.romReadOnly)) { if (::config.standardsMode && term) { displayFailure(term, "Cannot mount ROM"); orphanedTerminals.insert(term); } else if (term) term->factory->deleteTerminal(term); throw std::runtime_error("Could not mount ROM"); }
    if (debug) if (!addMount(this, getROMPath() + WS("\\debug"), "debug", true)) { if (::config.standardsMode && term) { displayFailure(term, "Cannot mount ROM"); orphanedTerminals.insert(term); } else if (term) term->factory->deleteTerminal(term); throw std::runtime_error("Could not mount debugger ROM"); }
#else
    if (!addMount(this, getROMPath() + WS("/rom"), "rom", ::config.romReadOnly)) { if (::config.standardsMode && term) { displayFailure(term, "Cannot mount ROM"); orphanedTerminals.insert(term); } else if (term) term->factory->deleteTerminal(term); throw std::runtime_error("Could not mount ROM"); }
    if (debug) if (!addMount(this, getROMPath() + WS("/debug"), "debug", true)) { if (::config.standardsMode && term) { displayFailure(term, "Cannot mount ROM"); orphanedTerminals.insert(term); } else if (term) term->factory->deleteTerminal(term); throw std::runtime_error("Could not mount debugger ROM"); }
#endif // _WIN32
#endif // STANDALONE_ROM
    // Mount custom directories from the command line
    for (auto m : customMounts) {
        bool ok = false;
        switch (std::get<2>(m)) {
            case -1: if (::config.mount_mode != MOUNT_MODE_NONE) ok = addMount(this, wstr(std::get<1>(m)), std::get<0>(m).c_str(), ::config.mount_mode != MOUNT_MODE_RW); break; // use default mode
            case 0: ok = addMount(this, wstr(std::get<1>(m)), std::get<0>(m).c_str(), true); break; // force RO
            default: ok = addMount(this, wstr(std::get<1>(m)), std::get<0>(m).c_str(), false); break; // force RW
        }
        if (!ok) fprintf(stderr, "Could not mount custom mount path at %s\n", std::get<1>(m).c_str());
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
    if (createDirectory(dataDir) != 0) {
        if (term) term->factory->deleteTerminal(term);
        throw std::runtime_error("Could not create computer data directory");
    }
    config = new computer_configuration(_config);
}

// Destructor
Computer::~Computer() {
    // Deinitialize any plugins that registered a destructor
    for (const auto& d : userdata_destructors) d.second(this, d.first, userdata[d.first]);
    // Destroy terminal
    if (term != NULL) {
        if (term->errorMode) orphanedTerminals.insert(term);
        else term->factory->deleteTerminal(term);
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
                //lua_sethook(computer->L, termHook, LUA_MASKCOUNT | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
                //lua_sethook(L, termHook, LUA_MASKCOUNT | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 1000000);
                lua_sethook(computer->L, NULL, 0, 0);
                lua_sethook(L, NULL, 0, 0);
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

// These functions implement a strategy for allowing `load` to yield.
// Basically, it spins up a new thread that runs the actual parser, and
// when the function yields, the thread signals the computer thread to
// yield itself. Once the computer thread resumes loading, the contents
// are sent to the loader thread, and parsing continues. This continues
// until the loader finishes, at which point the loader signals the
// computer thread that it's done. The results are copied back to the
// main state, and the loader returns.

static const char * yield_loader(lua_State *L, void* data, size_t *size) {
    load_ctx* ctx = (load_ctx*)data;
    lua_State *coro = lua_newthread(L);
    lua_pushvalue(ctx->coro, 1);
    lua_xmove(ctx->coro, coro, 1);
    ctx->argcount = 0;
    int status;
    do {
        status = lua_resume(coro, ctx->argcount);
        if (status == 0) {
            if (lua_isnoneornil(coro, 1)) return NULL;
            else if (lua_isstring(coro, 1)) return lua_tolstring(coro, 1, size);
            else luaL_error(L, "reader function must return a string");
        } else if (status == LUA_YIELD) {
            std::unique_lock<std::mutex> lock(ctx->lock);
            ctx->status = 1;
            ctx->argcount = lua_gettop(coro);
            lua_xmove(coro, ctx->L, ctx->argcount);
            ctx->notify.notify_all();
            while (ctx->status == 1) ctx->notify.wait(lock);
            if (ctx->status == 3) luaL_error(L, "");
            lua_xmove(ctx->L, coro, ctx->argcount);
            ctx->status = 0;
        } else {
            lua_error(L);
        }
    } while (status == LUA_YIELD);
    return NULL;
}

static void load_thread(load_ctx* ctx) {
    int status = lua_load(ctx->coro, yield_loader, ctx, ctx->name);
    if (ctx->status == 3) return;
    std::unique_lock<std::mutex> lock(ctx->lock);
    if (status == 0) {
        ctx->argcount = 1;
        lua_xmove(ctx->coro, ctx->L, 1);
    } else {
        ctx->argcount = 2;
        lua_pushnil(ctx->L);
        lua_xmove(ctx->coro, ctx->L, 1);
    }
    ctx->status = 2;
    ctx->notify.notify_all();
}

static int load_ctx_gc(lua_State *L) {
    load_ctx* ctx = (load_ctx*)lua_touserdata(L, 1);
    if (ctx->thread.joinable()) {
        {
            std::unique_lock<std::mutex> lock(ctx->lock);
            ctx->status = 3;
            ctx->notify.notify_all();
        }
        ctx->thread.join();
    }
    ctx->thread.~thread();
    ctx->lock.~mutex();
    ctx->notify.~condition_variable();
    return 0;
}

static int yieldable_load(lua_State *L) {
    load_ctx* ctx;
    if (lua_vcontext(L)) {
        ctx = (load_ctx*)lua_vcontext(L);
        std::unique_lock<std::mutex> lock(ctx->lock);
        ctx->status = 0;
        ctx->L = L;
        ctx->argcount = lua_gettop(L) - ctx->argcount;
        ctx->notify.notify_all();
    } else {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        const char * name = luaL_optstring(L, 2, "=(load)");
        load_ctx * basectx = new load_ctx;
        ctx = (load_ctx*)lua_newuserdata(L, sizeof(load_ctx));
        memcpy(ctx, basectx, sizeof(load_ctx));
        delete basectx;
        lua_createtable(L, 0, 1);
        lua_pushcfunction(L, load_ctx_gc);
        lua_setfield(L, -2, "__gc");
        lua_setmetatable(L, -2);
        ctx->thread = std::thread(load_thread, ctx);
        setThreadName(ctx->thread, "Loader Thread: " + std::string(name));
        ctx->status = 0;
        ctx->name = name;
        ctx->L = L;
        ctx->coro = lua_newthread(L);
        lua_pushvalue(L, 1);
        lua_xmove(L, ctx->coro, 1);
    }
    while (ctx->status != 2) {
        std::unique_lock<std::mutex> lock(ctx->lock);
        ctx->notify.wait(lock);
        if (ctx->status == 1) {
            int argcount = ctx->argcount;
            ctx->argcount = lua_gettop(L) - ctx->argcount;
            return lua_vyield(L, argcount, ctx);
        } else if (ctx->status == 3) return 0; // this should never happen
    }
    return ctx->argcount;
}

#if defined(__ANDROID__) || defined(__IPHONEOS__)
extern int mobile_luaopen(lua_State *L);
#endif

static const luaL_Reg lualibs[] = {
  {"", luaopen_base},
  {LUA_OSLIBNAME, luaopen_os},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
  {LUA_DBLIBNAME, luaopen_debug},
  {LUA_UTF8LIBNAME, luaopen_utf8},
  {LUA_BITLIBNAME, luaopen_bit32},
  {NULL, NULL}
};

static int doNothing(lua_State *L) {return 0;}

// Main computer loop
void runComputer(Computer * self, const path_t& bios_name) {
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
            memcpy(self->term->palette, defaultPalette, sizeof(defaultPalette));
            self->term->mode = 0;
            self->term->blink = false;
            self->term->canBlink = false;
            self->term->frozen = false;
            if (dynamic_cast<SDLTerminal*>(self->term) != NULL) ((SDLTerminal*)self->term)->cursorColor = 0;
            self->term->changed = true;
        }
        self->colors = 0xF0;
        self->system_start = std::chrono::system_clock::now();

        /*
        * All Lua contexts are held in this structure. We work with it almost
        * all the time.
        */
        lua_State *L = self->L = luaL_newstate();
        uncache_state(L);

        self->coro = lua_newthread(L);
        self->paramQueue = lua_newthread(L);
        if (selectedRenderer == 3) {
            std::lock_guard<std::mutex> lock(self->rawFileStackMutex);
            self->rawFileStack = luaL_newstate();
            lua_pushinteger(self->rawFileStack, 1);
            lua_pushlightuserdata(self->rawFileStack, self);
            lua_settable(self->rawFileStack, LUA_REGISTRYINDEX);
        }
        while (!self->eventQueue.empty()) self->eventQueue.pop();
        lua_setlockstate(L, false);

        // Reinitialize any peripherals that were connected before rebooting
        for (auto p : self->peripherals) p.second->reinitialize(L);

        // Push reference to this to the registry
        lua_pushinteger(L, 1);
        lua_pushlightuserdata(L, self);
        lua_settable(L, LUA_REGISTRYINDEX);
        lua_newtable(L);
        lua_createtable(L, 0, 1);
        lua_pushstring(L, "v");
        lua_setfield(L, -2, "__mode");
        lua_setmetatable(L, -2);
        lua_setfield(L, LUA_REGISTRYINDEX, "_coroutine_stack");

        // Disable luaL_register using package.loaded by making it a dummy table
        lua_newtable(L);
        lua_createtable(L, 0, 1);
        lua_pushcfunction(L, doNothing);
        lua_setfield(L, -2, "__newindex");
        lua_setmetatable(L, -2);
        lua_setfield(L, LUA_REGISTRYINDEX, "_LOADED");

        // Load libraries
        const luaL_Reg *lib = lualibs;
        for (; lib->func; lib++) {
            lua_pushcfunction(L, lib->func);
            lua_pushstring(L, lib->name);
            lua_call(L, 1, 0);
        }
        lua_getglobal(L, "os");
        lua_getfield(L, -1, "date");
        lua_setglobal(L, "os_date");
        lua_pop(L, 1);
        lua_pushnil(L);
        lua_setglobal(L, "os");
        // TODO: Fix logErrors since error hooks are no longer enabled
        if (self->debugger != NULL && !self->isDebugger) lua_sethook(self->coro, termHook, LUA_MASKLINE | LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 0);
        //else if (!self->isDebugger) lua_sethook(self->coro, termHook, LUA_MASKRET | LUA_MASKCALL | LUA_MASKERROR | LUA_MASKRESUME | LUA_MASKYIELD, 0);
        //else lua_sethook(self->coro, termHook, LUA_MASKERROR, 0);
        lua_atpanic(L, termPanic);
        for (library_t ** lib = libraries; *lib != NULL; lib++) load_library(self, self->coro, **lib);
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
        if (config.standardsMode) {
            // Override the default loader to allow yielding from `load`
            lua_pushcfunction(L, yieldable_load);
            lua_setglobal(L, "load");
        }

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
#if defined(__ANDROID__) || defined(__IPHONEOS__)
        mobile_luaopen(L);
#endif

        // Delete unwanted globals
        lua_pushnil(L);
        lua_setglobal(L, "dofile");
        lua_pushnil(L);
        lua_setglobal(L, "loadfile");
        lua_pushnil(L);
        lua_setglobal(L, "print");
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
            lua_setfield(L, -2, "getPixels");
            lua_pushnil(L);
            lua_setfield(L, -2, "screenshot");
            lua_pushnil(L);
            lua_setfield(L, -2, "showMouse");
            lua_pushnil(L);
            lua_setfield(L, -2, "setFrozen");
            lua_pushnil(L);
            lua_setfield(L, -2, "getFrozen");
            lua_pop(L, 1);
            if (config.http_enable) {
                lua_getglobal(L, "http");
                lua_pushnil(L);
                lua_setfield(L, -2, "addListener");
                lua_pushnil(L);
                lua_setfield(L, -2, "removeListener");
                lua_pop(L, 1);
            }
            lua_getglobal(L, "debug");
            lua_pushnil(L);
            lua_setfield(L, -2, "setbreakpoint");
            lua_pushnil(L);
            lua_setfield(L, -2, "unsetbreakpoint");
            lua_pop(L, 1);
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
                if (in != NULL) {
                    char tmp[4096];
                    while (!feof(in)) {
                        const size_t read = fread(tmp, 1, 4096, in);
                        if (read == 0) break;
                        script += std::string(tmp, read);
                    }
                    fclose(in);
                } else script = "printError('Could not load startup script: " + std::string(strerror(errno)) + "')";
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
        if (bios_file != NULL) {
            status = lua_load(self->coro, file_reader, bios_file, "@bios.lua");
            fclose(bios_file);
        } else {
            status = LUA_ERRFILE;
            lua_pushstring(L, strerror(errno));
        }
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
#ifdef __EMSCRIPTEN__
        queueTask([self](void*)->void*{
            if (self->eventTimeout != 0) SDL_RemoveTimer(self->eventTimeout);
            if (config.abortTimeout > 0 || config.standardsMode) self->eventTimeout = SDL_AddTimer(::config.standardsMode ? 7000 : ::config.abortTimeout, eventTimeoutEvent, self);
            return NULL;
        }, NULL);
#else
        if (self->eventTimeout != 0) SDL_RemoveTimer(self->eventTimeout);
        if (config.abortTimeout > 0 || config.standardsMode) self->eventTimeout = SDL_AddTimer(::config.standardsMode ? 7000 : ::config.abortTimeout, eventTimeoutEvent, self);
#endif
        while (status == LUA_YIELD && self->running == 1) {
            status = lua_resume(self->coro, narg);
            if (status == LUA_YIELD) {
                if (lua_gettop(self->coro) && lua_isstring(self->coro, -1)) narg = getNextEvent(self->coro, std::string(lua_tostring(self->coro, -1), lua_strlen(self->coro, -1)));
                else narg = getNextEvent(self->coro, "");
            } else if (status != 0 && self->running == 1) {
                // Catch runtime error
                self->running = 0;
                lua_pushcfunction(self->coro, termPanic);
                if (lua_isstring(self->coro, -2)) lua_pushvalue(self->coro, -2);
                else lua_pushnil(self->coro);
                lua_call(self->coro, 1, 0);
                break;
            } else if (self->running == 1) self->running = 0;
        }

        if (status == 0 && config.standardsMode && !self->term->errorMode) displayFailure(self->term, "Error running computer");
        
        // Shutdown threads
        self->event_lock.notify_all();
        // Stop all open websockets
        while (!self->openWebsockets.empty()) stopWebsocket(*self->openWebsockets.begin());
        for (library_t ** lib = libraries; *lib != NULL; lib++) if ((*lib)->deinit != NULL) (*lib)->deinit(self);
        if (self->eventTimeout != 0) SDL_RemoveTimer(self->eventTimeout);
        self->eventTimeout = 0;
        lua_close(L);   /* Cya, Lua */
        self->L = NULL;
        if (self->rawFileStack) {
            std::lock_guard<std::mutex> lock(self->rawFileStackMutex);
            lua_close(self->rawFileStack);
            self->rawFileStack = NULL;
        }
    }
    if (self->term != NULL && !self->term->errorMode) {
        // Reset terminal contents
        std::lock_guard<std::mutex> lock(self->term->locked);
        self->term->blinkX = 0;
        self->term->blinkY = 0;
        self->term->screen = vector2d<unsigned char>(self->term->width, self->term->height, ' ');
        self->term->colors = vector2d<unsigned char>(self->term->width, self->term->height, 0xF0);
        self->term->pixels = vector2d<unsigned char>(self->term->width * Terminal::fontWidth, self->term->height * Terminal::fontHeight, 0x0F);
        memcpy(self->term->palette, defaultPalette, sizeof(defaultPalette));
        self->term->mode = 0;
        self->term->blink = false;
        self->term->canBlink = false;
        self->term->frozen = false;
        if (dynamic_cast<SDLTerminal*>(self->term) != NULL) ((SDLTerminal*)self->term)->cursorColor = 0;
        self->term->changed = true;
    }
}

// Gets the next event for the given computer
bool Computer_getEvent(Computer * self, SDL_Event* e) {
    std::lock_guard<std::mutex> lock(self->termEventQueueMutex);
    if (self->termEventQueue.empty()) return false;
    SDL_Event* front = &self->termEventQueue.front();
    if (front == NULL || e == NULL) {self->termEventQueue.pop(); return false;}
    memcpy(e, front, sizeof(SDL_Event));
    self->termEventQueue.pop();
    return true;
}

// Thread wrapper for running a computer
void* computerThread(void* data) {
    Computer * comp = (Computer*)data;
#ifdef __APPLE__
    pthread_setname_np(std::string("Computer " + std::to_string(comp->id) + " Thread").c_str());
#endif
#ifdef __ANDROID__
    Android_JNI_SetupThread();
#endif
    // seed the Lua RNG
    srand(std::chrono::high_resolution_clock::now().time_since_epoch().count() & UINT_MAX);
    // in case the allocator decides to reuse pointers
    if (freedComputers.find(comp) != freedComputers.end())
        freedComputers.erase(comp);
    if (comp->config->startFullscreen && dynamic_cast<SDLTerminal*>(comp->term) != NULL) ((SDLTerminal*)comp->term)->toggleFullscreen();
    bool first = true;
    do {
        if (!first) {
#if defined(__IPHONEOS__) || defined(__ANDROID__)
            {
                std::lock_guard<std::mutex> lock(comp->term->locked);
                memcpy(comp->term->screen.data(), "Tap to restart", sizeof("Tap to restart")-1);
                memcpy(comp->term->colors.data(), "\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4", sizeof("\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4\xF4")-1);
                comp->term->changed = true;
            }
            queueTask([](void*)->void*{SDL_StopTextInput(); return NULL;}, NULL, true);
#endif
            bool ok = true;
            while (true) {
                SDL_Event e;
                std::string tmpstrval;
                {
                    std::mutex m;
                    std::unique_lock<std::mutex> l(m);
                    while (comp->termEventQueue.empty()) 
                        comp->event_lock.wait_for(l, std::chrono::seconds(5), [comp]()->bool{return !comp->termEventQueue.empty();});
                }
                if (Computer_getEvent(comp, &e)) {
#if defined(__IPHONEOS__) || defined(__ANDROID__)
                    if (e.type == SDL_MOUSEBUTTONUP) {
                        break;
#else
                    if (e.type == SDL_KEYDOWN && ((selectedRenderer == 0 || selectedRenderer == 5) ? e.key.keysym.sym == SDLK_r : e.key.keysym.sym == 19) && (e.key.keysym.mod & KMOD_CTRL)) {
                        if (comp->waitingForTerminate & 16) {
                            comp->waitingForTerminate |= 32;
                            comp->waitingForTerminate &= ~16;
                            break;
                        } else if ((comp->waitingForTerminate & 48) == 0) comp->waitingForTerminate |= 16;
                    } else if (e.type == SDL_KEYUP) {
                        comp->waitingForTerminate = 0;
#endif
                    } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE) {
                        if (e.window.windowID == comp->term->id) {
                            ok = false;
                            break;
                        } else {
                            std::string side;
                            monitor * m = findMonitorFromWindowID(comp, e.window.windowID, &side);
                            if (m != NULL) detachPeripheral(comp, side);
                        }
                    } else if (e.type == SDL_QUIT) {
                        ok = false;
                        break;
                    }
                }
            }
            if (!ok) break;
#if defined(__IPHONEOS__) || defined(__ANDROID__)
            queueTask([](void*)->void*{SDL_StartTextInput(); return NULL;}, NULL, true);
#endif
        }
        try {
    #ifdef STANDALONE_ROM
            runComputer(comp, wstr(standaloneBIOS));
    #else
            runComputer(comp, WS("bios.lua"));
    #endif
        } catch (Poco::Exception &e) {
            fprintf(stderr, "Uncaught exception while executing computer %d (last C function: %s): %s\n", comp->id, lastCFunction, e.displayText().c_str());
            queueTask([e](void*t)->void* {const std::string m = "Uh oh, an uncaught exception has occurred! Please report this to https://www.craftos-pc.cc/bugreport. When writing the report, include the following exception message: \"Poco exception on computer thread: " + e.displayText() + "\". The computer will now shut down.";  if (t != NULL) ((Terminal*)t)->showMessage(SDL_MESSAGEBOX_ERROR, "Uncaught Exception", m.c_str()); else if (selectedRenderer == 0 || selectedRenderer == 5) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Uncaught Exception", m.c_str(), NULL); return NULL; }, comp->term);
            if (comp->L != NULL) {
                comp->event_lock.notify_all();
                // Stop all open websockets
                while (!comp->openWebsockets.empty()) stopWebsocket(*comp->openWebsockets.begin());
                for (library_t ** lib = libraries; *lib != NULL; lib++) if ((*lib)->deinit != NULL) (*lib)->deinit(comp);
                if (comp->eventTimeout != 0) SDL_RemoveTimer(comp->eventTimeout);
                comp->eventTimeout = 0;
                lua_close(comp->L);   /* Cya, Lua */
                comp->L = NULL;
                if (comp->rawFileStack) {
                    std::lock_guard<std::mutex> lock(comp->rawFileStackMutex);
                    lua_close(comp->rawFileStack);
                    comp->rawFileStack = NULL;
                }
            }
        } catch (std::exception &e) {
            fprintf(stderr, "Uncaught exception while executing computer %d (last C function: %s): %s\n", comp->id, lastCFunction, e.what());
            queueTask([e](void*t)->void* {const std::string m = std::string("Uh oh, an uncaught exception has occurred! Please report this to https://www.craftos-pc.cc/bugreport. When writing the report, include the following exception message: \"Exception on computer thread: ") + e.what() + "\". The computer will now shut down.";  if (t != NULL) ((Terminal*)t)->showMessage(SDL_MESSAGEBOX_ERROR, "Uncaught Exception", m.c_str()); else if (selectedRenderer == 0 || selectedRenderer == 5) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Uncaught Exception", m.c_str(), NULL); return NULL; }, comp->term);
            if (comp->L != NULL) {
                comp->event_lock.notify_all();
                // Stop all open websockets
                while (!comp->openWebsockets.empty()) stopWebsocket(*comp->openWebsockets.begin());
                for (library_t ** lib = libraries; *lib != NULL; lib++) if ((*lib)->deinit != NULL) (*lib)->deinit(comp);
                if (comp->eventTimeout != 0) SDL_RemoveTimer(comp->eventTimeout);
                comp->eventTimeout = 0;
                lua_close(comp->L);   /* Cya, Lua */
                comp->L = NULL;
                if (comp->rawFileStack) {
                    std::lock_guard<std::mutex> lock(comp->rawFileStackMutex);
                    lua_close(comp->rawFileStack);
                    comp->rawFileStack = NULL;
                }
            }
        }
        first = false;
    } while ((config.keepOpenOnShutdown || config.standardsMode) && !comp->requestedExit);
    freedComputers.insert(comp);
    queueTask([](void* arg)->void* {delete (Computer*)arg; return NULL;}, comp, true);
    {
        LockGuard lock(computers);
        for (auto it = computers->begin(); it != computers->end(); ++it) {
            if (*it == comp) {
                it = computers->erase(it);
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
    try {comp = new Computer(id);}
    catch (Poco::Exception &e) {
        if ((selectedRenderer == 0 || selectedRenderer == 5) && !config.standardsMode) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to open computer", std::string("An error occurred while opening the computer session: " + e.displayText() + ". See https://www.craftos-pc.cc/docs/error-messages for more info.").c_str(), NULL);
        fprintf(stderr, "An error occurred while opening the computer session: %s\n", e.displayText().c_str());
        return NULL;
    } catch (std::exception &e) {
        if ((selectedRenderer == 0 || selectedRenderer == 5) && !config.standardsMode) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to open computer", std::string("An error occurred while opening the computer session: " + std::string(e.what()) + ". See https://www.craftos-pc.cc/docs/error-messages for more info.").c_str(), NULL);
        fprintf(stderr, "An error occurred while opening the computer session: %s\n", e.what());
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
