/*
 * os.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the os API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */


extern "C" {
#include <lauxlib.h>
}
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <chrono>
#include <queue>
#include <string>
#include <utility>
#include <vector>
#include "os.hpp"
#include "platform.hpp"
#include "term.hpp"
#include "config.hpp"

void gettingEvent(Computer *comp);
void gotEvent(Computer *comp);

int getNextEvent(lua_State *L, const char * filter) {
    Computer * computer = get_comp(L);
    if (computer->paramQueue == NULL) computer->paramQueue = lua_newthread(L);
    std::string ev;
    gettingEvent(computer);
    do {
        while (computer->eventQueue.size() == 0) {
            if (computer->running != 1) return 0;
            if (!lua_checkstack(computer->paramQueue, 1)) {
                lua_pushstring(L, "Could not allocate space for event");
                lua_error(L);
            }
            if (computer->timers.size() > 0 && computer->timers.back().time_since_epoch().count() == 0) computer->timers.pop_back();
            if (computer->alarms.size() > 0 && computer->alarms.back() == -1) computer->alarms.pop_back();
            if (computer->timers.size() > 0) {
                std::chrono::steady_clock::time_point t = std::chrono::steady_clock::now();
                for (int i = 0; i < computer->timers.size(); i++) {
                    if (t >= computer->timers[i] && computer->timers[i].time_since_epoch().count() > 0) {
                        lua_State *param = lua_newthread(computer->paramQueue);
                        lua_pushinteger(param, i);
                        computer->eventQueue.push("timer");
                        computer->timers[i] = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(0));
                    }
                }
            }
            if (computer->alarms.size() > 0) {
                time_t t = time(NULL);
                struct tm tm = *localtime(&t);
                for (int i = 0; i < computer->alarms.size(); i++) {
                    if ((double)tm.tm_hour + ((double)tm.tm_min/60.0) + ((double)tm.tm_sec/3600.0) == computer->alarms[i]) {
                        lua_State *param = lua_newthread(computer->paramQueue);
                        lua_pushinteger(param, i);
                        computer->eventQueue.push("alarm");
                        computer->alarms[i] = -1;
                    }
                }
            }
            if (computer->eventQueue.size() == 0) {
                lua_State *param = lua_newthread(computer->paramQueue);
                if (!lua_checkstack(param, 4)) printf("Could not allocate event\n");
                const char * name = termGetEvent(param);
                if (name != NULL) {
                    if (strcmp(name, "die") == 0) computer->running = 0;
                    computer->eventQueue.push(name);
                } else if (param) {
                    lua_pop(computer->paramQueue, 1);
                    param = NULL;
                }
            }
        }
        ev = computer->eventQueue.front();
        computer->eventQueue.pop();
    } while (strlen(filter) > 0 && ev != std::string(filter));
    lua_State *param = lua_tothread(computer->paramQueue, 1);
    if (param == NULL) return 0;
    int count = lua_gettop(param);
    if (!lua_checkstack(L, count + 1)) {
        printf("Could not allocate enough space in the stack for %d elements, skipping event \"%s\"\n", count, ev.c_str());
        return 0;
    }
    lua_pushstring(L, ev.c_str());
    lua_xmove(param, L, count);
    lua_remove(computer->paramQueue, 1);
    //lua_close(param);
    gotEvent(computer);
    return count + 1;
}

int os_getComputerID(lua_State *L) {lua_pushinteger(L, get_comp(L)->id); return 1;}
int os_getComputerLabel(lua_State *L) {
    struct computer_configuration cfg = getComputerConfig(get_comp(L)->id);
    if (cfg.label == NULL) return 0;
    lua_pushstring(L, cfg.label);
    freeComputerConfig(cfg);
    return 1;
}

int os_setComputerLabel(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    struct computer_configuration cfg = getComputerConfig(get_comp(L)->id);
    if (cfg.label != NULL) free((char*)cfg.label);
    char * label = (char*)malloc(lua_strlen(L, 1) + 1);
    strcpy(label, lua_tostring(L, 1));
    cfg.label = label;
    setComputerConfig(get_comp(L)->id, cfg);
    freeComputerConfig(cfg);
    return 0;
}

int os_queueEvent(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
    //if (paramQueue == NULL) paramQueue = lua_newthread(L);
    const char * name = lua_tostring(L, 1);
    if (!lua_checkstack(computer->paramQueue, 1)) {
        lua_pushstring(L, "Could not allocate space for event");
        lua_error(L);
    }
    lua_State *param = lua_newthread(computer->paramQueue);
    lua_remove(L, 1);
    int count = lua_gettop(L);
    lua_checkstack(param, count);
    lua_xmove(L, param, count);
    computer->eventQueue.push(name);
    return 0;
}

int os_clock(lua_State *L) {
    lua_pushinteger(L, clock() / CLOCKS_PER_SEC);
    return 1;
}

int os_startTimer(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    Computer * computer = get_comp(L);
    computer->timers.push_back(std::chrono::steady_clock::now() + std::chrono::milliseconds((long)(lua_tonumber(L, 1) * 1000)));
    lua_pushinteger(L, computer->timers.size() - 1);
    return 1;
}

int os_cancelTimer(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    Computer * computer = get_comp(L);
    int id = lua_tointeger(L, 1);
    if (id == computer->timers.size() - 1) computer->timers.pop_back();
    else computer->timers[id] = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(0));
    return 0;
}

int os_time(lua_State *L) {
    const char * type = "ingame";
    if (lua_isstring(L, 1)) type = lua_tostring(L, 1);
    std::string tmp(type);
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), [](unsigned char c){return std::tolower(c);});
    time_t t = time(NULL);
    struct tm rightNow;
    if (tmp == "utc") rightNow = *gmtime(&t);
    else rightNow = *localtime(&t);
    int hour = rightNow.tm_hour;
    int minute = rightNow.tm_min;
    int second = rightNow.tm_sec;
    lua_pushnumber(L, (double)hour + ((double)minute/60.0) + ((double)second/3600.0));
    return 1;
}

int os_epoch(lua_State *L) {
    const char * type = "ingame";
    if (lua_isstring(L, 1)) type = lua_tostring(L, 1);
    std::string tmp(type);
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), [](unsigned char c) {return std::tolower(c); });
    if (tmp == "utc") {
        lua_pushinteger(L, (long long)time(NULL) * 1000LL);
    } else if (tmp == "local") {
        time_t t = time(NULL);
        lua_pushinteger(L, (long long)mktime(localtime(&t)) * 1000LL);
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
    return 1;
}

int os_cancelAlarm(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    Computer * computer = get_comp(L);
    int id = lua_tointeger(L, 1);
    if (id == computer->alarms.size() - 1) computer->alarms.pop_back();
    else computer->alarms[id] = -1;
    return 0;
}

int os_shutdown(lua_State *L) {
    get_comp(L)->running = 0;
    return 0;
}

int os_reboot(lua_State *L) {
    get_comp(L)->running = 2;
    return 0;
}

int os_system(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    system(lua_tostring(L, 1));
    return 0;
}

int os_about(lua_State *L) {
    lua_pushstring(L, "CraftOS-PC v2.0.0-b3\n\nCraftOS-PC 2 is licensed under the MIT License.\nMIT License\n\
\n\
Copyright (c) 2019 JackMacWindows\n\
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
SOFTWARE.");
    return 1;
}

extern bool headless;
int os_exit(lua_State *L) {
    if (headless) exit(lua_isnumber(L, 1) ? lua_tointeger(L, 1) : 0);
    return 0;
}

const char * os_keys[19] = {
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
    "about",
    "exit"
};

lua_CFunction os_values[19] = {
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
    os_about,
    os_exit
};

library_t os_lib = {"os", 19, os_keys, os_values, NULL, NULL};