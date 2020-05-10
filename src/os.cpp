/*
 * os.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the os API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#define CRAFTOSPC_INTERNAL
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <chrono>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include "os.hpp"
#include "platform.hpp"
#include "term.hpp"
#include "config.hpp"
#include "terminal/SDLTerminal.hpp"
#include "terminal/CLITerminal.hpp"
#include "terminal/RawTerminal.hpp"
#include "peripheral/monitor.hpp"
#ifndef NO_CLI
#include <signal.h>
#endif

void gettingEvent(Computer *comp);
void gotEvent(Computer *comp);
extern monitor * findMonitorFromWindowID(Computer *comp, unsigned id, std::string& sideReturn);

int nextTaskID = 0;
std::queue< std::tuple<int, std::function<void*(void*)>, void*, bool> > taskQueue;
std::unordered_map<int, void*> taskQueueReturns;
std::mutex taskQueueReturnsMutex;
std::condition_variable taskQueueNotify;
std::mutex taskQueueMutex;
bool exiting = false;
bool forceCheckTimeout = false;
extern int selectedRenderer;
extern bool rawClient;
extern Uint32 task_event_type;
extern Uint32 render_event_type;
extern std::unordered_map<int, unsigned char> keymap_cli;
extern std::unordered_map<int, unsigned char> keymap;
std::thread::id mainThreadID;
std::atomic_bool taskQueueReady(false);

void* queueTask(std::function<void*(void*)> func, void* arg, bool async) {
    if (std::this_thread::get_id() == mainThreadID) return func(arg);
    int myID;
    {
        std::lock_guard<std::mutex> lock(taskQueueMutex);
        myID = nextTaskID++;
        taskQueue.push(std::make_tuple(myID, func, arg, async));
    }
    if (selectedRenderer == 0 && !exiting) {
        SDL_Event ev;
        ev.type = task_event_type;
        SDL_PushEvent(&ev);
    }
    if (selectedRenderer != 0 && selectedRenderer != 2) {
        {std::lock_guard<std::mutex> lock(taskQueueMutex);}
        while (taskQueueReady) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        taskQueueReady = true;
        taskQueueNotify.notify_all();
        while (taskQueueReady) {std::this_thread::yield(); taskQueueNotify.notify_all();}
    }
    if (async) return NULL;
    while (([]()->bool{taskQueueReturnsMutex.lock(); return true;})() && taskQueueReturns.find(myID) == taskQueueReturns.end() && !exiting) {taskQueueReturnsMutex.unlock(); std::this_thread::yield();}
    void* retval = taskQueueReturns[myID];
    taskQueueReturns.erase(myID);
    taskQueueReturnsMutex.unlock();
    return retval;
}

void awaitTasks() {
    while (true) {
        if (taskQueue.size() > 0) {
            auto v = taskQueue.front();
            void* retval = std::get<1>(v)(std::get<2>(v));
            taskQueueReturns[std::get<0>(v)] = retval;
            taskQueue.pop();
        }
        SDL_PumpEvents();
        std::this_thread::yield();
    }
}

void mainLoop() {
    mainThreadID = std::this_thread::get_id();
#ifndef __EMSCRIPTEN__
    while (rawClient ? !exiting : computers.size() > 0) {
#endif
		//bool res = false; // I forgot what this is for
		if (selectedRenderer == 0) /*res =*/ SDLTerminal::pollEvents();
#ifndef NO_CLI
		else if (selectedRenderer == 2) /*res =*/ CLITerminal::pollEvents();
#endif
        else {
            std::unique_lock<std::mutex> lock(taskQueueMutex);
            while (!taskQueueReady) taskQueueNotify.wait_for(lock, std::chrono::seconds(5));
            while (taskQueue.size() > 0) {
                auto v = taskQueue.front();
                void* retval = std::get<1>(v)(std::get<2>(v));
                if (!std::get<3>(v)) {
                    std::lock_guard<std::mutex> lock2(taskQueueReturnsMutex);
                    taskQueueReturns[std::get<0>(v)] = retval;
                }
                taskQueue.pop();
            }
            taskQueueReady = false;
        }

        std::this_thread::yield();
#ifdef __EMSCRIPTEN__
		if (!rawClient && computers.size() == 0) exiting = true;
#else
    }
    exiting = true;
#endif
}

Uint32 eventTimeoutEvent(Uint32 interval, void* param) {
    //Computer * comp = (Computer*)param;
    forceCheckTimeout = true;
    return 1000;
}

int getNextEvent(lua_State *L, std::string filter) {
    Computer * computer = get_comp(L);
    if (computer->running != 1) return 0;
    if (computer->eventTimeout != 0) {
#ifdef __EMSCRIPTEN__
        queueTask([computer](void*)->void*{SDL_RemoveTimer(computer->eventTimeout); return NULL;}, NULL);
#else
        SDL_RemoveTimer(computer->eventTimeout);
#endif
        computer->eventTimeout = 0;
        computer->timeoutCheckCount = 0;
    }
    std::string ev;
    gettingEvent(computer);
    if (!lua_checkstack(computer->paramQueue, 1)) luaL_error(L, "Could not allocate space for event");
    lua_State *param = lua_newthread(computer->paramQueue);
    do {
        while (termHasEvent(computer) && computer->eventQueue.size() < 25) {
            if (!lua_checkstack(param, 4)) printf("Could not allocate event\n");
            const char * name = termGetEvent(param);
            if (name != NULL) {
                if (strcmp(name, "die") == 0) { computer->running = 0; name = "terminate"; }
                computer->eventQueue.push(name);
                param = lua_newthread(computer->paramQueue);
            }
        }
		if (computer->running != 1) return 0;
        while (computer->eventQueue.size() == 0) {
            if (computer->alarms.size() == 0) {
                std::mutex m;
                std::unique_lock<std::mutex> l(m);
                while (computer->running == 1 && computer->alarms.size() == 0 && !termHasEvent(computer)) 
                    computer->event_lock.wait_for(l, std::chrono::seconds(5), [computer]()->bool{return computer->alarms.size() != 0 || termHasEvent(computer) || computer->running != 1;});
            }
            if (computer->running != 1) return 0;
            if (computer->alarms.size() > 0 && computer->alarms.back() == -1) computer->alarms.pop_back();
            if (computer->alarms.size() > 0) {
                time_t t = time(NULL);
                struct tm tm = *localtime(&t);
                for (unsigned i = 0; i < computer->alarms.size(); i++) {
                    if ((double)tm.tm_hour + ((double)tm.tm_min/60.0) + ((double)tm.tm_sec/3600.0) == computer->alarms[i]) {
                        lua_pushinteger(param, i);
                        computer->eventQueue.push("alarm");
                        computer->alarms[i] = -1;
                        param = lua_newthread(computer->paramQueue);
                    }
                }
            }
            while (termHasEvent(computer) && computer->eventQueue.size() < 25) {
                if (!lua_checkstack(param, 4)) printf("Could not allocate event\n");
                const char * name = termGetEvent(param);
                if (name != NULL) {
                    if (strcmp(name, "die") == 0) { computer->running = 0; name = "terminate"; }
                    computer->eventQueue.push(name);
                    param = lua_newthread(computer->paramQueue);
                }
            }
        }
        ev = computer->eventQueue.front();
        computer->eventQueue.pop();
        std::this_thread::yield();
    } while (!filter.empty() && ev != filter);
    lua_pop(computer->paramQueue, 1);
    param = lua_tothread(computer->paramQueue, 1);
    if (param == NULL) {
        printf("Queue item is not a thread for event \"%s\"!\n", ev.c_str()); 
        if (lua_gettop(computer->paramQueue) > 0) lua_remove(computer->paramQueue, 1);
        return 0;
    }
    int count = lua_gettop(param);
    if (!lua_checkstack(L, count + 1)) {
        printf("Could not allocate enough space in the stack for %d elements, skipping event \"%s\"\n", count, ev.c_str());
        if (lua_gettop(computer->paramQueue) > 0) lua_remove(computer->paramQueue, 1);
        return 0;
    }
    lua_pushstring(L, ev.c_str());
    lua_xmove(param, L, count);
    lua_remove(computer->paramQueue, 1);
    //lua_close(param);
    gotEvent(computer);
    computer->eventTimeout = SDL_AddTimer(config.abortTimeout, eventTimeoutEvent, computer);
    return count + 1;
}

int os_getComputerID(lua_State *L) {lua_pushinteger(L, get_comp(L)->id); return 1;}
int os_getComputerLabel(lua_State *L) {
    struct computer_configuration cfg = get_comp(L)->config;
    if (cfg.label.empty()) return 0;
    lua_pushstring(L, cfg.label.c_str());
    return 1;
}

std::string asciify(std::string str) {
    std::string retval;
    for (char c : str) {if (c < 32 || c > 127) retval += '?'; else retval += c;}
    return retval;
}

int os_setComputerLabel(lua_State *L) {
    if (!lua_isnoneornil(L, 1) && !lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * comp = get_comp(L);
    comp->config.label = lua_isstring(L, 1) ? std::string(lua_tostring(L, 1), lua_strlen(L, 1)) : "";
    if (comp->term != NULL) comp->term->setLabel(comp->config.label.empty() ? "CraftOS Terminal: " + std::string(comp->isDebugger ? "Debugger" : "Computer") + " " + std::to_string(comp->id) : "CraftOS Terminal: " + asciify(comp->config.label));
    return 0;
}

int os_queueEvent(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
    //if (paramQueue == NULL) paramQueue = lua_newthread(L);
    std::string name = std::string(lua_tostring(L, 1), lua_strlen(L, 1));
    if (!lua_checkstack(computer->paramQueue, 1)) luaL_error(L, "Could not allocate space for event");
    lua_State *param = lua_newthread(computer->paramQueue);
    lua_remove(L, 1);
    int count = lua_gettop(L);
    lua_checkstack(param, count);
    lua_xmove(L, param, count);
    computer->eventQueue.push(name);
    computer->event_lock.notify_all();
    return 0;
}

int os_clock(lua_State *L) {
    Computer * computer = get_comp(L);
    lua_pushnumber(L, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - computer->system_start).count() / 1000.0);
    return 1;
}

struct timer_data_t {
	Computer * comp;
	SDL_TimerID timer;
};

template<typename T>
class PointerProtector {
	T* ptr;
public:
	PointerProtector(T* p) : ptr(p) {}
	~PointerProtector() { delete ptr; }
};

std::unordered_map<SDL_TimerID, struct timer_data_t*> runningTimerData;

const char * timer_event(lua_State *L, void* param) {
    struct timer_data_t * data = (struct timer_data_t*)param;
    lua_pushinteger(L, data->timer);
    runningTimerData.erase(data->timer);
    delete data;
    return "timer";
}

Uint32 notifyEvent(Uint32 interval, void* param) {
	struct timer_data_t * data = (struct timer_data_t*)param;
    bool found = false;
    for (auto i : runningTimerData) if (i.second == param) {found = true; break;}
    if (!found) return 0;
    if (exiting || data->comp == NULL) {runningTimerData.erase(data->timer); delete data; return 0;}
	{
		std::lock_guard<std::mutex> lock(freedTimersMutex);
		if (freedTimers.find(data->timer) != freedTimers.end()) { 
			freedTimers.erase(data->timer);
            runningTimerData.erase(data->timer);
            delete data;
			return 0;
		}
	}
	if (data->comp->timerIDs.find(data->timer) != data->comp->timerIDs.end()) data->comp->timerIDs.erase(data->timer);
    data->comp->event_lock.notify_all();
    termQueueProvider(data->comp, timer_event, data);
    return 0;
}

int os_startTimer(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    Computer * computer = get_comp(L);
    if (lua_tonumber(L, 1) <= 0.0) {
        int* id = new int;
        *id = 1;
        termQueueProvider(computer, [](lua_State *L, void*)->const char*{lua_pushinteger(L, 1); return "timer";}, NULL);
        lua_pushinteger(L, *id);
        return 1;
    }
	struct timer_data_t * data = new struct timer_data_t;
	data->comp = computer;
    queueTask([L](void*a)->void*{
        struct timer_data_t * data = (struct timer_data_t*)a;
        data->timer = SDL_AddTimer(lua_tonumber(L, 1) * 1000 + 3, notifyEvent, data);
        return NULL;
    }, data);
    runningTimerData.insert(std::make_pair(data->timer, data));
    lua_pushinteger(L, data->timer);
	computer->timerIDs.insert(data->timer);
    return 1;
}

int os_cancelTimer(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    if (runningTimerData.find(lua_tointeger(L, 1)) == runningTimerData.end()) return 0;
#ifdef __EMSCRIPTEN__
    queueTask([L](void*)->void*{SDL_RemoveTimer(lua_tointeger(L, 1)); return NULL;}, NULL);
#else
    SDL_RemoveTimer(lua_tointeger(L, 1));
#endif
    delete runningTimerData[lua_tointeger(L, 1)];
    runningTimerData.erase(lua_tointeger(L, 1));
    return 0;
}

int os_time(lua_State *L) {
    const char * type = "ingame";
    if (lua_isstring(L, 1)) type = lua_tostring(L, 1);
    std::string tmp(type);
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), [ ](unsigned char c){return std::tolower(c);});
    time_t t = time(NULL);
    struct tm rightNow;
    if (tmp == "utc") rightNow = *gmtime(&t);
    else rightNow = *localtime(&t);
    int hour = rightNow.tm_hour;
    int minute = rightNow.tm_min;
    int second = rightNow.tm_sec;
    int milli = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() % 1000;
    lua_pushnumber(L, (double)hour + ((double)minute/60.0) + ((double)second/3600.0) + (milli / 3600000.0));
    return 1;
}

int os_epoch(lua_State *L) {
    const char * type = "ingame";
    if (lua_isstring(L, 1)) type = lua_tostring(L, 1);
    std::string tmp(type);
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), [](unsigned char c) {return std::tolower(c); });
    if (tmp == "utc") {
        lua_pushinteger(L, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    } else if (tmp == "local") {
        time_t t = time(NULL);
        long long off = (long long)mktime(localtime(&t)) - t;
        lua_pushinteger(L, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() + (off * 1000));
    } else {
        time_t t = time(NULL);
        struct tm rightNow = *localtime(&t);
        int hour = rightNow.tm_hour;
        int minute = rightNow.tm_min;
        int second = rightNow.tm_sec;
        double m_time = (double)hour + ((double)minute/60.0) + ((double)second/3600.0);
        double m_day = rightNow.tm_yday;
        lua_pushinteger(L, m_day * 86400000 + (int) (m_time * 3600000.0f));
    }
    return 1;
}

int os_day(lua_State *L) {
    const char * type = "ingame";
    if (lua_isstring(L, 1)) type = lua_tostring(L, 1);
    std::string tmp(type);
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), [](unsigned char c) {return std::tolower(c); });
    time_t t = time(NULL);
    if (tmp == "ingame") {
        struct tm rightNow = *localtime(&t);
        lua_pushinteger(L, rightNow.tm_yday);
        return 1;
    } else if (tmp == "local") t = mktime(localtime(&t));
    lua_pushinteger(L, t/(60*60*24));
    return 1;
}

int os_setAlarm(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    Computer * computer = get_comp(L);
    computer->alarms.push_back(lua_tonumber(L, 1));
    lua_pushinteger(L, computer->alarms.size() - 1);
    computer->event_lock.notify_all();
    return 1;
}

int os_cancelAlarm(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    Computer * computer = get_comp(L);
    unsigned id = lua_tointeger(L, 1);
    if (id == computer->alarms.size() - 1) computer->alarms.pop_back();
    else computer->alarms[id] = -1;
    return 0;
}

extern int selectedRenderer;
extern int returnValue;
int os_shutdown(lua_State *L) {
    get_comp(L)->running = 0;
    if (selectedRenderer == 1 && lua_isnumber(L, 1)) returnValue = lua_tointeger(L, 1);
    return 0;
}

int os_reboot(lua_State *L) {
    get_comp(L)->running = 2;
    return 0;
}

int os_system(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!config.debug_enable) return 0;
    lua_pushinteger(L, system(lua_tostring(L, 1)));
    return 1;
}


int os_about(lua_State *L) {
    lua_pushstring(L, "CraftOS-PC " CRAFTOSPC_VERSION "\n\nCraftOS-PC 2 is licensed under the MIT License.\nMIT License\n\
\n\
Copyright (c) 2019-2020 JackMacWindows\n\
\n\
Permission is hereby granted, free of charge, to any person obtaining a copy\n\
of this software and associated documentation files (the \"Software\"), to deal\n\
in the Software without restriction, including without limitation the rights\n\
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n\
copies of the Software, and to permit persons to whom the Software is\n\
furnished to do so, subject to the following conditions:\n\
\n\
The above copyright notice and this permission notice shall be included in all\n\
copies or substantial portions of the Software.\n\
\n\
THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n\
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n\
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n\
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n\
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n\
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n\
SOFTWARE.\n\n\
Special thanks:\n\
* dan200 for creating the ComputerCraft mod and making it open source\n\
* SquidDev for picking up ComputerCraft after Dan left and creating CC: Tweaked\n\
* EveryOS for sending me a patched version of Lua that finally fixed issue #1\n\
* Everyone on the Minecraft Computer Mods Discord server for the support while developing CraftOS-PC 2");
    return 1;
}

const char * os_keys[18] = {
    "getComputerID",
    "computerID",
    "getComputerLabel",
    "computerLabel",
    "setComputerLabel",
    "queueEvent",
    "clock",
    "startTimer",
    "cancelTimer",
    "time",
    "epoch",
    "day",
    "setAlarm",
    "cancelAlarm",
    "shutdown",
    "reboot",
    "system",
    "about"
};

lua_CFunction os_values[18] = {
    os_getComputerID,
    os_getComputerID,
    os_getComputerLabel,
    os_getComputerLabel,
    os_setComputerLabel,
    os_queueEvent,
    os_clock,
    os_startTimer,
    os_cancelTimer,
    os_time,
    os_epoch,
    os_day,
    os_setAlarm,
    os_cancelAlarm,
    os_shutdown,
    os_reboot,
    os_system,
    os_about
};

library_t os_lib = {"os", 18, os_keys, os_values, nullptr, nullptr};
