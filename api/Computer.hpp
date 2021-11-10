/*
 * Computer.hpp
 * CraftOS-PC 2
 *
 * This file defines the Computer class, which stores the state of each running
 * computer.
 *
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#ifndef CRAFTOS_PC_COMPUTER_HPP
#define CRAFTOS_PC_COMPUTER_HPP
extern "C" {
#include <lua.h>
}
#include <csetjmp>
#include <cstdint>
#include <condition_variable>
#include <functional>
#include <list>
#include <map>
#include <queue>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <SDL2/SDL.h>
#include "configuration.hpp"
#include "FileEntry.hpp"
#include "lib.hpp"
#include "peripheral.hpp"
#include "Terminal.hpp"

/// This is the type for functions that return event information. This type is used by queueEvent.
/// To construct the event data, push the values for the event parameters in order and return the name of the event.
typedef std::function<std::string(lua_State *, void*)> event_provider;

/// This is the type for even hook functions. This type is used for addEventHook.
typedef std::function<std::string(lua_State *, const std::string&, void*)> event_hook;

/// This is the type for functions that are called for SDL event hooks.
/// The Computer and Terminal objects provided point to the topmost window as a suggestion as to which terminal the event is intended for. They may or may not be NULL.
/// The SDL_Event structure will not exist after the function returns. Copy any values you need elsewhere before returning.
typedef std::function<bool(SDL_Event *, Computer *, Terminal *, void*)> sdl_event_handler;

/// The main Computer structure. Functions that require access to the computer will take a pointer to this structure.
struct Computer {
    // The following fields are available in API version 10.0. No structure version check is required to use these.

    // These properties are the most likely to be useful for reading data about a computer.
    int id;                                                                 // The computer's ID
    int running = 0;                                                        // The current run state for the computer (0 = off, 1 = on, 2 = restarting)
    lua_State *L = NULL;                                                    // The global Lua state for the computer
    Terminal * term;                                                        // A pointer to the terminal object for the computer (warning: may be NULL in headless mode!)
    struct computer_configuration * config;                                 // The configuration structure for this computer
    path_t dataDir;                                                         // The path to the computer's data directory
    std::vector< std::tuple<std::list<std::string>, path_t, bool> > mounts; // A list of all current mounts on the computer. Each entry is stored as a tuple with 1) a list of internal path components, 2) the real path that's mounted, and 3) whether the mount is read-only
    std::unordered_map<std::string, peripheral*> peripherals;               // A dictionary holding information about what peripherals are attached on each side
    std::mutex peripherals_mutex;                                           // A mutex locking access to the peripheral dictionary (lock this before modifying the peripheral list!)
    unsigned char colors = 0xF0;                                            // The current foreground/background color pair for drawing text to the terminal
    
    // These properties are provided explicitly for the use of plugin developers.
    std::unordered_map<int, void *> userdata;                                                  // A free dictionary for use by plugins to store any data that is linked to a single computer
    std::unordered_map<int, std::function<void(Computer*, int, void*)> > userdata_destructors; // A dictionary that is used to store any destructors/cleanup functions for userdata entries
    
    // These properties will likely be of little use to anything outside of CraftOS-PC. They store info about the internal state of the computer, and modifying these values may break things.
    // Do not use these unless you know what you're doing! (They would be private, but there are many non-members that use these values and would need to be listed as friends.)
    std::queue<std::string> eventQueue; // A queue holding the names of each event in the queue
    lua_State * paramQueue; // A Lua stack that stores the parameters for each event in the queue
    std::queue<SDL_Event> termEventQueue; // A queue holding all UI events that have not been processed yet
    std::mutex termEventQueueMutex; // A mutex locking access to the termEventQueue queue
    std::queue<std::pair<event_provider, void*> > event_provider_queue; // A queue holding events that have been queued from C++ (use queueEvent, don't modify this directly!)
    std::mutex event_provider_queue_mutex; // A mutex locking access to the event_provider_queue queue
    std::chrono::high_resolution_clock::time_point last_event = std::chrono::high_resolution_clock::now(); // The last time an event was waited for
    std::condition_variable event_lock; // A condition variable that is notified when an event is available in the queue
    SDL_TimerID eventTimeout = 0; // A timer that fires after config.abortTimeout to notify the computer to error
    int timeoutCheckCount = 0; // The number of seconds the computer has attempted to terminate a long-running task
    bool getting_event = false; // Whether the computer is currently waiting for an event
    bool lastResizeEvent = false; // Whether the last event sent was a resize event (no longer used)
    mouse_event_data nextMouseMove = {0, 0, 0, 0, std::string()}; // Storage for the next mouse_move event if it was debounced (deprecated - no longer used as of v2.5.6)
    mouse_event_data lastMouse = {-1, -1, 0, 16, std::string()}; // Data about the last mouse event (deprecated - no longer used as of v2.5.6)
    SDL_TimerID mouseMoveDebounceTimer = 0; // A timer that fires when the next mouse movement event is ready (deprecated - no longer used as of v2.5.6)
    int waitingForTerminate = 0; // A bitmask of termination shortcuts that have been held
    void * cli_panel; // A PANEL object for CLI mode
    void * cli_term; // A WINDOW object for CLI mode
    void * debugger = NULL; // A pointer to an attached debugger, or if this computer is a debugger, the debugger library object
    bool isDebugger = false; // Whether this computer is a debugger
    int hookMask = 0; // The Lua bitmask of hooks to be executed on the computer
    bool hasBreakpoints = false; // Whether any breakpoints have been set
    bool shouldDeinitDebugger = false; // Whether the debugger has been detached and should be deallocated on the next hook run
    std::map< int, std::pair<std::string, lua_Integer> > breakpoints; // A list of breakpoints for the debugger
    lua_State *coro; // The top-level coroutine
    jmp_buf on_panic; // A jump pointer on the computer thread to restart execution of the computer - close the Lua thread before jumping here!
    std::chrono::system_clock::time_point system_start = std::chrono::system_clock::now(); // The time that the computer was started
    std::unordered_set<SDL_TimerID> timerIDs; // A list of currently active timers
    std::vector<void*> openWebsockets; // A list of open WebSocket handles that need to be closed
    int requests_open = 0; // The number of HTTP requests currently open
    std::unordered_set<int> usedDriveMounts; // A list of drive mount IDs that are in use
    std::list<Computer*> referencers; // A list of computers that have attached this computer as a peripheral (used to notify attachers when this computer shuts down)
    int files_open = 0; // The number of files currently open
    bool mounter_initializing = false; // Set to true when the computer is initializing to allow mounting the ROM
    std::unordered_map<unsigned, const FileEntry *> virtualMounts; // Maps virtual mount IDs to virtual filesystem references
    bool requestedExit = false; // Stores a temporary value indicating whether the quit button was pressed and is waiting for exit
    std::mutex timerIDsMutex; // A mutex locking timerIDs

    // The following fields are available in API version 10.1 and later.
    bool forceCheckTimeout = false; // Whether to force checking for a timeout
    uint8_t redstoneInputs[6] = {0, 0, 0, 0, 0, 0}; // Standard redstone inputs (for plugins)
    uint8_t redstoneOutputs[6] = {0, 0, 0, 0, 0, 0}; // Standard redstone outputs
    uint16_t bundledRedstoneInputs[6] = {0, 0, 0, 0, 0, 0}; // Bundled redstone inputs (for plugins)
    uint16_t bundledRedstoneOutputs[6] = {0, 0, 0, 0, 0, 0}; // Bundled redstone outputs

    // The following fields are available in API version 10.4 and later.
    std::unordered_map<std::string, std::list<std::pair<const event_hook&, void*> > > eventHooks; // List of hooks for events
    lua_State *rawFileStack = NULL; // Temporary stack for raw mode file access function calls
    std::mutex rawFileStackMutex; // A mutex locking rawFileStack
    int fileUploadCount = 0; // Stores the number of files that have been uploaded in the current set (if 0, no set is active)

    // The following fields are available in API version 10.5 and later.
    std::queue<void*> httpRequestQueue; // Queue for HTTP requests that are past the limit
    std::mutex httpRequestQueueMutex; // A mutex locking httpRequestQueueMutex

    // The following fields are available in API version 10.6 and later.
    std::unordered_set<uint16_t> openWebsocketServers; // List of ports currently in use by WebSocket servers

    bool shouldDeleteDebugger = true;

private:
    // The constructor is marked private to avoid having to implement it in this file.
    // It isn't necessary to construct a Computer directly; just use the startComputer function instead.
    friend Computer* startComputer(int id);
    friend void showReleaseNotes();
    friend void* computerThread(void* data);
    friend void debuggerThread(Computer * comp, void * dbgv, std::string side);
    friend void* releaseNotesThread(void* data);
    friend class debugger;
    Computer(int i) : Computer(i, false) {}
    Computer(int i, bool debug);
    ~Computer();
};

#endif