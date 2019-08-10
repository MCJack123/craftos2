extern "C" {
#include "os.h"
#include "platform.h"
#include <lauxlib.h>
}
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <chrono>
#include <queue>
#include <utility>
#include <vector>
#include "term.h"

char * label;
bool label_defined = false;
std::queue<std::pair<const char *, lua_State*> > eventQueue;
std::vector<std::chrono::steady_clock::time_point> timers;
std::vector<double> alarms;
extern "C" void gettingEvent(void);
extern "C" void gotEvent(void);

extern "C" {
int running = 1;
void queueEvent(const char * name, lua_State *param) {eventQueue.push(std::make_pair(name, param));}

int getNextEvent(lua_State *L, const char * filter) {
    std::pair<const char *, lua_State*> ev;
    gettingEvent();
    do {
        while (eventQueue.size() == 0) {
            if (running != 1) return 0;
            if (timers.size() > 0 && timers.back().time_since_epoch().count() == 0) timers.pop_back();
            if (alarms.size() > 0 && alarms.back() == -1) alarms.pop_back();
            if (timers.size() > 0) {
                std::chrono::steady_clock::time_point t = std::chrono::steady_clock::now();
                for (int i = 0; i < timers.size(); i++) {
                    if (t >= timers[i] && timers[i].time_since_epoch().count() > 0) {
                        lua_State *param = lua_newthread(L);
                        lua_pushinteger(param, i);
                        eventQueue.push(std::make_pair("timer", param));
                        timers[i] = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(0));
                    }
                }
            }
            if (alarms.size() > 0) {
                time_t t = time(NULL);
                struct tm tm = *localtime(&t);
                for (int i = 0; i < alarms.size(); i++) {
                    if ((double)tm.tm_hour + ((double)tm.tm_min/60.0) + ((double)tm.tm_sec/3600.0) == alarms[i]) {
                        lua_State *param = lua_newthread(L);
                        lua_pushinteger(param, i);
                        eventQueue.push(std::make_pair("alarm", param));
                        alarms[i] = -1;
                    }
                }
            }
            lua_State *param = lua_newthread(L);
            if (!lua_checkstack(param, 4)) printf("Could not allocate event\n");
            const char * name = termGetEvent(param);
            if (name != NULL) {
                if (strcmp(name, "die") == 0) running = 0;
                eventQueue.push(std::make_pair(name, param));
            } else if (param) {
                lua_pop(L, -1);
                lua_pushnil(param);
                //lua_close(param); 
                param = NULL;
            }
        }
        ev = eventQueue.front();
        eventQueue.pop();
    } while (strlen(filter) > 0 && strcmp(std::get<0>(ev), filter) != 0);
    lua_State *param = ev.second;
    int count = lua_gettop(param);
    if (!lua_checkstack(L, count + 1)) printf("Could not allocate\n");
    lua_pushstring(L, ev.first);
    lua_xmove(param, L, count);
    //lua_close(param);
    gotEvent();
    return count + 1;
}

int os_getComputerID(lua_State *L) {lua_pushinteger(L, 0); return 1;}
int os_getComputerLabel(lua_State *L) {
    if (!label_defined) return 0;
    lua_pushstring(L, label);
    return 1;
}

int os_setComputerLabel(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (label_defined) free(label);
    label = (char*)malloc(lua_strlen(L, 1) + 1);
    strcpy(label, lua_tostring(L, 1));
    label_defined = true;
    return 0;
}

void os_free() {
    if (label_defined) free(label);
}

int os_queueEvent(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * name = lua_tostring(L, 1);
    lua_State *param = lua_newthread(L);
    lua_remove(L, 1);
    int count = lua_gettop(L);
    lua_checkstack(param, count);
    lua_xmove(L, param, count);
    lua_xmove(param, L, 1);
    eventQueue.push(std::make_pair(name, param));
    return 0;
}

int os_clock(lua_State *L) {
    lua_pushinteger(L, getUptime());
    return 1;
}

int os_startTimer(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    timers.push_back(std::chrono::steady_clock::now() + std::chrono::milliseconds((long)(lua_tonumber(L, 1) * 1000)));
    lua_pushinteger(L, timers.size() - 1);
    return 1;
}

int os_cancelTimer(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    int id = lua_tointeger(L, 1);
    if (id == timers.size() - 1) timers.pop_back();
    else timers[id] = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(0));
    return 0;
}

int os_time(lua_State *L) {
    const char * type = "ingame";
    if (lua_isstring(L, 1)) type = lua_tostring(L, 1);
    time_t t = time(NULL);
    struct tm rightNow;
    if (strcmp(type, "utc") == 0) rightNow = *gmtime(&t);
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
    if (strcmp(type, "utc") == 0) {
        lua_pushinteger(L, (long long)time(NULL) * 1000LL);
    } else if (strcmp(type, "local") == 0) {
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
    time_t t = time(NULL);
    if (strcmp(type, "ingame") == 0) {
        struct tm rightNow = *localtime(&t);
        lua_pushinteger(L, rightNow.tm_yday);
        return 1;
    } else if (strcmp(type, "local")) t = mktime(localtime(&t));
    lua_pushinteger(L, t/(60*60*24));
    return 1;
}

int os_setAlarm(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    alarms.push_back(lua_tonumber(L, 1));
    lua_pushinteger(L, alarms.size() - 1);
    return 1;
}

int os_cancelAlarm(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    int id = lua_tointeger(L, 1);
    if (id == alarms.size() - 1) alarms.pop_back();
    else alarms[id] = -1;
    return 0;
}

int os_shutdown(lua_State *L) {
    running = 0;
    return 0;
}

int os_reboot(lua_State *L) {
    running = 2;
    return 0;
}

int os_system(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    system(lua_tostring(L, 1));
    return 0;
}

int os_about(lua_State *L) {
    lua_pushstring(L, "CraftOS-PC v2.0.0-b1\n\nCraftOS-PC 2 is licensed under the MIT License.\nMIT License\n\
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

library_t os_lib = {"os", 18, os_keys, os_values, NULL, os_free};
}