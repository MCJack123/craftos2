/*
 * runtime.cpp
 * CraftOS-PC 2
 * 
 * This file implements some common functions relating to the functioning of
 * the CraftOS-PC emulation session.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#include <cerrno>
#include <cstring>
#include <chrono>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <configuration.hpp>
#include <dirent.h>
#include <sys/stat.h>
#include "main.hpp"
#include "runtime.hpp"
#include "platform.hpp"
#include "terminal/SDLTerminal.hpp"
#include "terminal/CLITerminal.hpp"
#include "terminal/RawTerminal.hpp"
#include "terminal/HardwareSDLTerminal.hpp"
#include "termsupport.hpp"
#ifdef WIN32
#define R_OK 0x04
#define W_OK 0x02
#endif
#ifndef NO_CLI
#include <csignal>
#endif
#ifndef WIN32
#include <libgen.h>
#include <unistd.h>
#endif

#define termHasEvent(computer) ((computer)->running == 1 && (!(computer)->event_provider_queue.empty() || (computer)->lastResizeEvent || !(computer)->termEventQueue.empty()))

int nextTaskID = 0;
ProtectedObject<std::queue< std::tuple<int, std::function<void*(void*)>, void*, bool> > > taskQueue;
ProtectedObject<std::unordered_map<int, void*> > taskQueueReturns;
std::condition_variable taskQueueNotify;
bool exiting = false;
std::thread::id mainThreadID;
std::atomic_bool taskQueueReady(false);
static std::unordered_map<std::string, std::list<std::pair<const event_hook&, void*> > > globalEventHooks;

monitor * findMonitorFromWindowID(Computer *comp, unsigned id, std::string& sideReturn) {
    std::lock_guard<std::mutex> lock(comp->peripherals_mutex);
    for (const auto& p : comp->peripherals) {
        if (p.second != NULL && strcmp(p.second->getMethods().name, "monitor") == 0) {
            monitor * m = (monitor*)p.second;
            if (m->term->id == id) {
                sideReturn.assign(p.first);
                return m;
            }
        }
    }
    return NULL;
}

void* queueTask(const std::function<void*(void*)>& func, void* arg, bool async) {
    if (std::this_thread::get_id() == mainThreadID) return func(arg);
    int myID;
    {
        LockGuard lock(taskQueue);
        myID = nextTaskID++;
        taskQueue->push(std::make_tuple(myID, func, arg, async));
    }
    if ((selectedRenderer == 0 || selectedRenderer == 5) && !exiting) {
        SDL_Event ev;
        ev.type = task_event_type;
        SDL_PushEvent(&ev);
    }
    if (selectedRenderer != 0 && selectedRenderer != 2 && selectedRenderer != 5) {
        {LockGuard lock(taskQueue);}
        while (taskQueueReady) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        taskQueueReady = true;
        taskQueueNotify.notify_all();
        while (taskQueueReady) {std::this_thread::yield(); taskQueueNotify.notify_all();}
    }
    if (async) return NULL;
    while (([]()->bool{taskQueueReturns.lock(); return true;})() && taskQueueReturns->find(myID) == taskQueueReturns->end() && !exiting) {taskQueueReturns.unlock(); std::this_thread::yield();}
    void* retval = (*taskQueueReturns)[myID];
    taskQueueReturns->erase(myID);
    taskQueueReturns.unlock();
    return retval;
}

void awaitTasks(const std::function<bool()>& predicate = []()->bool{return true;}) {
    while (predicate()) {
        if (!taskQueue->empty()) {
            auto v = taskQueue->front();
            void* retval = std::get<1>(v)(std::get<2>(v));
            (*taskQueueReturns)[std::get<0>(v)] = retval;
            taskQueue->pop();
        }
        SDL_PumpEvents();
        std::this_thread::yield();
    }
}

void mainLoop() {
#ifndef __EMSCRIPTEN__
    while (rawClient ? !exiting : !computers->empty() || !orphanedTerminals.empty()) {
#endif
        //bool res = false; // I forgot what this is for
        if (selectedRenderer == 0) /*res =*/ SDLTerminal::pollEvents();
#ifndef NO_CLI
        else if (selectedRenderer == 2) /*res =*/ CLITerminal::pollEvents();
#endif
        else if (selectedRenderer == 5) HardwareSDLTerminal::pollEvents();
        else {
            std::unique_lock<std::mutex> lock(taskQueue.getMutex());
            while (!taskQueueReady) taskQueueNotify.wait_for(lock, std::chrono::seconds(5));
            while (!taskQueue->empty()) {
                auto v = taskQueue->front();
                void* retval = std::get<1>(v)(std::get<2>(v));
                if (!std::get<3>(v)) {
                    LockGuard lock2(taskQueueReturns);
                    (*taskQueueReturns)[std::get<0>(v)] = retval;
                }
                taskQueue->pop();
            }
            taskQueueReady = false;
        }

        std::this_thread::yield();
#ifdef __EMSCRIPTEN__
        if (!rawClient && computers->size() == 0) exiting = true;
#else
    }
    exiting = true;
#endif
}

extern library_t * libraries[8];
Uint32 eventTimeoutEvent(Uint32 interval, void* param) {
    Computer * computer = (Computer*)param;
    if (freedComputers.find(computer) != freedComputers.end()) return 0;
    if (computer->L == NULL || computer->getting_event) return 0;
    if (++computer->timeoutCheckCount >= 5) {
        if (config.standardsMode) {
            // In standards mode we give no second chances - just crash and burn
            displayFailure(computer->term, "Error running computer", "Too long without yielding");
            computer->running = 0;
            lua_halt(computer->L);
            return 0;
        } else if (dynamic_cast<SDLTerminal*>(computer->term) != NULL) {
            SDL_MessageBoxData msg;
            SDL_MessageBoxButtonData buttons[] = {
                {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Restart"},
                {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Wait"}
            };
            msg.flags = SDL_MESSAGEBOX_WARNING;
            msg.window = dynamic_cast<SDLTerminal*>(computer->term)->win;
            msg.title = "Computer not responding";
            msg.message = "A long-running task has caused this computer to stop responding. You can either force restart the computer, or wait for the program to respond.";
            msg.numbuttons = 2;
            msg.buttons = buttons;
            msg.colorScheme = NULL;
            if (queueTask([](void* arg)->void* {int num = 0; SDL_ShowMessageBox((SDL_MessageBoxData*)arg, &num); return (void*)(ptrdiff_t)num; }, &msg) != NULL) {
                computer->running = 2;
                lua_halt(computer->L);
                return 0;
            } else {
                computer->timeoutCheckCount = -15;
                return 1000;
            }
        }
    }
    lua_externalerror(computer->L, "Too long without yielding");
    return 1000;
}

void queueEvent(Computer *comp, const event_provider& p, void* data) {
    if (freedComputers.find(comp) != freedComputers.end()) return;
    {
        std::lock_guard<std::mutex> lock(comp->event_provider_queue_mutex);
        comp->event_provider_queue.push(std::make_pair(p, data));
    }
    comp->event_lock.notify_all();
}

int getNextEvent(lua_State *L, const std::string& filter) {
    Computer * computer = get_comp(L);
    if (computer->running != 1) return 0;
    computer->timeoutCheckCount = 0;
    std::string ev;
    computer->getting_event = true;
    lua_State *param;
    do {
        if (!lua_checkstack(computer->paramQueue, 1)) luaL_error(L, "Could not allocate space for event");
        param = lua_newthread(computer->paramQueue);
        while (termHasEvent(computer)/* && computer->eventQueue.size() < 25*/) {
            if (!lua_checkstack(param, 4)) fprintf(stderr, "Could not allocate event\n");
            std::string name = termGetEvent(param);
            if (!name.empty()) {
                if (name == "die") { computer->running = 0; name = "terminate"; }
                computer->eventQueue.push(name);
                if (!lua_checkstack(computer->paramQueue, 1)) luaL_error(L, "Could not allocate space for event");
                param = lua_newthread(computer->paramQueue);
            }
        }
        if (computer->running != 1) return 0;
        while (computer->eventQueue.empty()) {
            std::mutex m;
            std::unique_lock<std::mutex> l(m);
            while (computer->running == 1 && !termHasEvent(computer)) 
                computer->event_lock.wait_for(l, std::chrono::seconds(5), [computer]()->bool{return termHasEvent(computer) || computer->running != 1;});
            if (computer->running != 1) return 0;
            while (termHasEvent(computer)/* && computer->eventQueue.size() < 25*/) {
                if (!lua_checkstack(param, 4)) fprintf(stderr, "Could not allocate event\n");
                std::string name = termGetEvent(param);
                if (!name.empty() && computer->eventHooks.find(name) != computer->eventHooks.end()) {
                    for (const auto& h : computer->eventHooks[name]) {
                        name = h.first(L, name, h.second);
                        if (name.empty()) break;
                    }
                }
                if (!name.empty() && globalEventHooks.find(name) != globalEventHooks.end()) {
                    for (const auto& h : globalEventHooks[name]) {
                        name = h.first(L, name, h.second);
                        if (name.empty()) break;
                    }
                }
                if (!name.empty()) {
                    if (name == "die") { computer->running = 0; name = "terminate"; }
                    computer->eventQueue.push(name);
                    if (!lua_checkstack(computer->paramQueue, 1)) luaL_error(L, "Could not allocate space for event");
                    param = lua_newthread(computer->paramQueue);
                }
            }
        }
        ev = computer->eventQueue.front();
        computer->eventQueue.pop();
        if (!filter.empty() && ev != filter && ev != "terminate") lua_remove(computer->paramQueue, 1);
        lua_pop(computer->paramQueue, 1);
        std::this_thread::yield();
    } while (!filter.empty() && ev != filter && ev != "terminate");
    if ((size_t)lua_gettop(computer->paramQueue) != computer->eventQueue.size() + 1) {
        fprintf(stderr, "Warning: Queue sizes are incorrect! Expect misaligned event parameters.\n");
    }
    param = lua_tothread(computer->paramQueue, 1);
    if (param == NULL) {
        fprintf(stderr, "Queue item is not a thread for event \"%s\"!\n", ev.c_str()); 
        if (lua_gettop(computer->paramQueue) > 0) lua_remove(computer->paramQueue, 1);
        return 0;
    }
    const int count = lua_gettop(param);
    if (!lua_checkstack(L, count + 1)) {
        fprintf(stderr, "Could not allocate enough space in the stack for %d elements, skipping event \"%s\"\n", count, ev.c_str());
        if (lua_gettop(computer->paramQueue) > 0) lua_remove(computer->paramQueue, 1);
        return 0;
    }
    lua_pushstring(L, ev.c_str());
    lua_xmove(param, L, count);
    lua_remove(computer->paramQueue, 1);
    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - computer->last_event).count() > 200) {
#ifdef __EMSCRIPTEN__
        queueTask([computer](void*)->void*{
#endif
        if (computer->eventTimeout != 0) SDL_RemoveTimer(computer->eventTimeout);
        computer->eventTimeout = SDL_AddTimer(config.standardsMode ? 7000 : config.abortTimeout, eventTimeoutEvent, computer);
#ifdef __EMSCRIPTEN__
        return NULL;}, NULL);
#endif
        computer->last_event = std::chrono::high_resolution_clock::now();
    }
    computer->getting_event = false;
    return count + 1;
}

bool addMount(Computer *comp, const path_t& real_path, const char * comp_path, bool read_only) {
    struct_stat st;
    if (platform_stat(real_path.c_str(), &st) != 0 || platform_access(real_path.c_str(), R_OK | (read_only ? 0 : W_OK)) != 0) return false;
    std::vector<std::string> elems = split(comp_path, "/\\");
    std::list<std::string> pathc;
    for (const std::string& s : elems) {
        if (s == "..") { if (pathc.empty()) return false; else pathc.pop_back(); }
        else if (!s.empty() && !std::all_of(s.begin(), s.end(), [](const char c)->bool{return c == '.';})) pathc.push_back(s);
    }
    for (const auto& m : comp->mounts)
        if (std::get<0>(m) == pathc && std::get<1>(m) == real_path) return false;
    int selected = 1;
    if (!comp->mounter_initializing && config.showMountPrompt && dynamic_cast<SDLTerminal*>(comp->term) != NULL) {
        SDL_MessageBoxData data;
        data.flags = SDL_MESSAGEBOX_WARNING;
        data.window = dynamic_cast<SDLTerminal*>(comp->term)->win;
        data.title = "Mount requested";
        // see apis/config.cpp:101 for why this is a pointer (TL;DR Windows is dumb)
        std::string * message = new std::string("A script is attempting to mount the REAL path " + std::string(astr(real_path)) + ". Any script will be able to read" + (read_only ? " " : " AND WRITE ") + "any files in this directory. Do you want to allow mounting this path?");
        data.message = message->c_str();
        data.numbuttons = 2;
        SDL_MessageBoxButtonData buttons[2];
        buttons[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
        buttons[0].buttonid = 0;
        buttons[0].text = "Deny";
        buttons[1].flags = 0;
        buttons[1].buttonid = 1;
        buttons[1].text = "Allow";
        data.buttons = buttons;
        data.colorScheme = NULL;
        queueTask([data](void*selected_)->void* {SDL_ShowMessageBox(&data, (int*)selected_); return NULL; }, &selected);
        delete message;
    }
    if (!selected) return false;
    comp->mounts.push_back(std::make_tuple(std::list<std::string>(pathc), real_path, read_only));
    return true;
}

bool operator==(const FileEntry& lhs, const FileEntry& rhs) {
    if (lhs.isDir != rhs.isDir) return false;
    if (lhs.isDir) {
        return lhs.dir == rhs.dir;
    } else {
        return lhs.data == rhs.data;
    }
}

bool addVirtualMount(Computer * comp, const FileEntry& vfs, const char * comp_path) {
    std::vector<std::string> elems = split(comp_path, "/\\");
    std::list<std::string> pathc;
    for (const std::string& s : elems) {
        if (s == "..") { if (pathc.empty()) return false; else pathc.pop_back(); }
        else if (!s.empty() && !std::all_of(s.begin(), s.end(), [](const char c)->bool{return c == '.';})) pathc.push_back(s);
    }
    unsigned idx;
    for (idx = 0; comp->virtualMounts.find(idx) != comp->virtualMounts.end() && idx < UINT_MAX; idx++) {}
    for (const auto& v : comp->mounts) {
        const path_t& path = std::get<1>(v);
        if (!std::isdigit(path[0])) continue;
        int end = 0;
        for (const auto& c : path) {
            if (c == ':') break;
            else if (!std::isdigit(c)) {end = -1; break;}
            end++;
        }
        if (end > 0 && std::get<0>(v) == pathc && *comp->virtualMounts[std::stoi(path.substr(0, end))] == vfs) return false;
    }
    comp->virtualMounts[idx] = &vfs;
    comp->mounts.push_back(std::make_tuple(std::list<std::string>(pathc), to_path_t(idx) + WS(":"), true));
    return true;
}

void registerSDLEvent(SDL_EventType type, const sdl_event_handler& handler, void* userdata) {
    SDLTerminal::eventHandlers.insert(std::make_pair(type, std::make_pair(handler, userdata)));
    switch (type) {case SDL_JOYAXISMOTION: case SDL_JOYBALLMOTION: case SDL_JOYBUTTONDOWN: case SDL_JOYBUTTONUP: case SDL_JOYDEVICEADDED: case SDL_JOYDEVICEREMOVED: case SDL_JOYHATMOTION: SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER); break; default: break;}
}

void addEventHook(const std::string& event, Computer * computer, const event_hook& hook, void* userdata) {
    std::unordered_map<std::string, std::list<std::pair<const event_hook&, void*> > >& eventHooks = computer == NULL ? globalEventHooks : computer->eventHooks;
    if (eventHooks.find(event) == eventHooks.end()) eventHooks[event] = std::list<std::pair<const event_hook&, void*> >();
    eventHooks[event].push_back(std::make_pair(hook, userdata));
}

extern "C" {
    FILE* mounter_fopen_(lua_State *L, const char * filename, const char * mode) {
        lastCFunction = __func__;
        if (!((mode[0] == 'r' || mode[0] == 'w' || mode[0] == 'a') && (mode[1] == '\0' || mode[1] == 'b' || mode[1] == '+') && (mode[1] == '\0' || mode[2] == '\0' || mode[2] == 'b' || mode[2] == '+') && (mode[1] == '\0' || mode[2] == '\0' || mode[3] == '\0')))
            luaL_error(L, "Unsupported mode");
        if (get_comp(L)->files_open >= config.maximumFilesOpen) { errno = EMFILE; return NULL; }
        struct_stat st;
        const path_t newpath = mode[0] == 'r' ? fixpath(get_comp(L), lua_tostring(L, 1), true) : fixpath_mkdir(get_comp(L), lua_tostring(L, 1));
        if ((mode[0] == 'w' || mode[0] == 'a' || (mode[0] == 'r' && (mode[1] == '+' || (mode[1] == 'b' && mode[2] == '+')))) && fixpath_ro(get_comp(L), filename))
            { errno = EACCES; return NULL; }
        if (platform_stat(newpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) { errno = EISDIR; return NULL; }
        FILE* retval;
        if (mode[1] == 'b' && mode[2] == '+') retval = platform_fopen(newpath.c_str(), std::string(mode).substr(0, 2).c_str());
        else if (mode[1] == '+') {
            std::string mstr = mode;
            mstr.erase(mstr.begin() + 1);
            retval = platform_fopen(newpath.c_str(), mstr.c_str());
        } else retval = platform_fopen(newpath.c_str(), mode);
        if (retval != NULL) get_comp(L)->files_open++;
        return retval;
    }

    int mounter_fclose_(lua_State *L, FILE * stream) {
        lastCFunction = __func__;
        const int retval = fclose(stream);
        if (retval == 0 && get_comp(L)->files_open > 0) get_comp(L)->files_open--;
        return retval;
    }
}
