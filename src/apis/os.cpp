/*
 * apis/os.cpp
 * CraftOS-PC 2
 *
 * This file implements the functions for the os API.
 *
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include <Computer.hpp>
#include "../main.hpp"
#include "../runtime.hpp"
#include "../util.hpp"

static int os_getComputerID(lua_State *L) { lastCFunction = __func__; lua_pushinteger(L, get_comp(L)->id); return 1; }

static int os_getComputerLabel(lua_State *L) {
    lastCFunction = __func__;
    struct computer_configuration * cfg = get_comp(L)->config;
    if (cfg->label.empty()) return 0;
    lua_pushstring(L, cfg->label.c_str());
    return 1;
}

static int os_setComputerLabel(lua_State *L) {
    lastCFunction = __func__;
    Computer * comp = get_comp(L);
    comp->config->label = std::string(luaL_optstring(L, 1, ""), lua_isstring(L, 1) ? lua_strlen(L, 1) : 0);
    if (comp->term != NULL) comp->term->setLabel(comp->config->label.empty() ? "CraftOS Terminal: " + std::string(comp->isDebugger ? "Debugger" : "Computer") + " " + std::to_string(comp->id) : "CraftOS Terminal: " + asciify(comp->config->label));
    return 0;
}

static int os_queueEvent(lua_State *L) {
    lastCFunction = __func__;
    Computer * computer = get_comp(L);
    const std::string name = std::string(luaL_checkstring(L, 1), lua_strlen(L, 1));
    if (!lua_checkstack(computer->paramQueue, 1)) luaL_error(L, "Could not allocate space for event");
    lua_State *param = lua_newthread(computer->paramQueue);
    lua_remove(L, 1);
    const int count = lua_gettop(L);
    lua_checkstack(param, count);
    lua_xmove(L, param, count);
    computer->eventQueue.push(name);
    computer->event_lock.notify_all();
    return 0;
}

static int os_clock(lua_State *L) {
    lastCFunction = __func__;
    Computer * computer = get_comp(L);
    lua_pushnumber(L, (double)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - computer->system_start).count() / 1000.0);
    return 1;
}

struct timer_data_t {
    Computer * comp;
    SDL_TimerID timer;
    std::mutex * lock;
    bool isAlarm;
};

template<typename T>
class PointerProtector {
    T* ptr;
public:
    PointerProtector(T* p) : ptr(p) {}
    ~PointerProtector() { delete ptr; }
};

static ProtectedObject<std::unordered_map<SDL_TimerID, struct timer_data_t*> > runningTimerData;

static std::string timer_event(lua_State *L, void* param) {
    struct timer_data_t * data = (timer_data_t*)param;
    bool found = false;
    runningTimerData.lock();
    for (const auto& i : *runningTimerData) if (i.second == param) { found = true; break; }
    runningTimerData.unlock();
    if (!found) return "";
    data->lock->lock();
    lua_pushinteger(L, data->timer);
    runningTimerData->erase(data->timer);
    data->lock->unlock();
    const bool isAlarm = data->isAlarm;
    delete data->lock;
    delete data;
    return isAlarm ? "alarm" : "timer";
}

static Uint32 notifyEvent(Uint32 interval, void* param) {
    struct timer_data_t * data = (timer_data_t*)param;
    bool found = false;
    runningTimerData.lock();
    for (const auto& i : *runningTimerData) if (i.second == param) { found = true; break; }
    runningTimerData.unlock();
    if (!found) return 0;
    data->lock->lock();
    if (exiting || data->comp == NULL) {
        runningTimerData->erase(data->timer);
        data->lock->unlock();
        delete data->lock;
        delete data;
        return 0;
    }
    {
        LockGuard lock(freedTimers);
        if (freedTimers->find(data->timer) != freedTimers->end()) {
            freedTimers->erase(data->timer);
            runningTimerData->erase(data->timer);
            data->lock->unlock();
            delete data->lock;
            delete data;
            return 0;
        }
    }
    {
        std::lock_guard<std::mutex> lock(data->comp->timerIDsMutex);
        if (data->comp->timerIDs.find(data->timer) != data->comp->timerIDs.end()) data->comp->timerIDs.erase(data->timer);
    }
    queueEvent(data->comp, timer_event, data);
    data->comp->event_lock.notify_all();
    data->lock->unlock();
    return 0;
}

static int os_startTimer(lua_State *L) {
    lastCFunction = __func__;
    Computer * computer = get_comp(L);
    if (luaL_checknumber(L, 1) <= 0.0) {
        queueEvent(computer, [](lua_State *L, void*)->std::string {lua_pushinteger(L, 1); return "timer"; }, NULL);
        lua_pushinteger(L, 1);
        return 1;
    }
    struct timer_data_t * data = new struct timer_data_t;
    data->comp = computer;
    data->lock = new std::mutex;
    data->isAlarm = false;
    queueTask([L](void*a)->void* {
        struct timer_data_t * data = (timer_data_t*)a;
        Uint32 time = (Uint32)(lua_tonumber(L, 1) * 1000);
        if (config.standardsMode) time = (Uint32)ceil(time / 50.0) * 50;
        data->timer = SDL_AddTimer(time + 3, notifyEvent, data);
        return NULL;
    }, data);
    runningTimerData->insert(std::make_pair(data->timer, data));
    lua_pushinteger(L, data->timer);
    std::lock_guard<std::mutex> lock(computer->timerIDsMutex);
    computer->timerIDs.insert(data->timer);
    return 1;
}

static int os_cancelTimer(lua_State *L) {
    lastCFunction = __func__;
    const SDL_TimerID id = (SDL_TimerID)luaL_checkinteger(L, 1);
    if (runningTimerData->find(id) == runningTimerData->end()) return 0;
    timer_data_t * data = (*runningTimerData)[id];
    runningTimerData->erase(id);
    data->lock->lock();
#ifdef __EMSCRIPTEN__
    queueTask([id](void*)->void* {SDL_RemoveTimer(id); return NULL; }, NULL);
#else
    SDL_RemoveTimer(id);
#endif
    data->lock->unlock();
    delete data->lock;
    delete data;
    return 0;
}

static int getfield(lua_State *L, const char *key, int d) {
    int res;
    lua_getfield(L, -1, key);
    if (lua_isnumber(L, -1))
        res = (int)lua_tointeger(L, -1);
    else {
        if (d < 0)
            return luaL_error(L, "field " LUA_QS " missing in date table", key);
        res = d;
    }
    lua_pop(L, 1);
    return res;
}

static int getboolfield(lua_State *L, const char *key) {
    lua_getfield(L, -1, key);
    const int res = lua_isnil(L, -1) ? -1 : lua_toboolean(L, -1);
    lua_pop(L, 1);
    return res;
}

static int os_time(lua_State *L) {
    lastCFunction = __func__;
    if (lua_istable(L, 1)) {
        struct tm ts;
        lua_settop(L, 1);  /* make sure table is at the top */
        ts.tm_sec = getfield(L, "sec", 0);
        ts.tm_min = getfield(L, "min", 0);
        ts.tm_hour = getfield(L, "hour", 12);
        ts.tm_mday = getfield(L, "day", -1);
        ts.tm_mon = getfield(L, "month", -1) - 1;
        ts.tm_year = getfield(L, "year", -1) - 1900;
        ts.tm_isdst = getboolfield(L, "isdst");
        lua_pushinteger(L, mktime(&ts));
        return 1;
    }
    std::string tmp(luaL_optstring(L, 1, "ingame"));
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), [](unsigned char c) {return std::tolower(c); });
    if (tmp == "ingame") {
        lua_pushnumber(L, floor((double)((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - get_comp(L)->system_start).count() + 300000LL) % 1200000LL) / 50.0) / 1000.0);
        return 1;
    } else if (tmp != "utc" && tmp != "local") luaL_error(L, "Unsupported operation");
    time_t t = time(NULL);
    struct tm rightNow;
    if (tmp == "utc") rightNow = *gmtime(&t);
    else rightNow = *localtime(&t);
    const int hour = rightNow.tm_hour;
    const int minute = rightNow.tm_min;
    const int second = rightNow.tm_sec;
    const int milli = (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() % 1000LL;
    lua_pushnumber(L, (double)hour + ((double)minute / 60.0) + ((double)second / 3600.0) + (milli / 3600000.0));
    return 1;
}

static int os_epoch(lua_State *L) {
    lastCFunction = __func__;
    std::string tmp(luaL_optstring(L, 1, "ingame"));
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), [](unsigned char c) {return std::tolower(c); });
    if (tmp == "utc") {
        lua_pushinteger(L, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    } else if (tmp == "local") {
        time_t t = time(NULL);
        const long long off = (long long)mktime(localtime(&t)) - t;
        lua_pushinteger(L, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count() + (off * 1000));
    } else if (tmp == "ingame") {
        const double m_time = (double)((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - get_comp(L)->system_start).count() + 300000LL) % 1200000LL) / 50000.0;
        const double m_day = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now() - get_comp(L)->system_start).count() / 20 + 1;
        lua_Integer epoch = (lua_Integer)(m_day * 86400000) + (lua_Integer)(m_time * 3600000.0);
        if (config.standardsMode) epoch = (lua_Integer)floor(epoch / 200) * 200;
        lua_pushinteger(L, epoch);
    } else luaL_error(L, "Unsupported operation");
    return 1;
}

static int os_day(lua_State *L) {
    lastCFunction = __func__;
    std::string tmp(luaL_optstring(L, 1, "ingame"));
    std::transform(tmp.begin(), tmp.end(), tmp.begin(), [](unsigned char c) {return std::tolower(c); });
    time_t t = time(NULL);
    if (tmp == "ingame") {
        lua_pushinteger(L, std::chrono::duration_cast<std::chrono::minutes>(std::chrono::system_clock::now() - get_comp(L)->system_start).count() / 20 + 1);
        return 1;
    } else if (tmp == "local") t = mktime(localtime(&t));
    else if (tmp != "utc") luaL_error(L, "Unsupported operation");
    lua_pushinteger(L, t / (60 * 60 * 24));
    return 1;
}

static int os_setAlarm(lua_State *L) {
    lastCFunction = __func__;
    const double time = luaL_checknumber(L, 1);
    if (time < 0.0 || time >= 24.0) luaL_error(L, "Number out of range");
    Computer * computer = get_comp(L);
    const double current_time = floor((double)((std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - computer->system_start).count() + 300000LL) % 1200000LL) / 50.0) / 1000.0;
    double delta_time;
    if (time >= current_time) delta_time = time - current_time;
    else delta_time = (time + 24.0) - current_time;
    Uint32 real_time = (Uint32)(delta_time * 50000.0);
    struct timer_data_t * data = new struct timer_data_t;
    data->comp = computer;
    data->lock = new std::mutex;
    data->isAlarm = true;
    queueTask([real_time](void*a)->void* {
        struct timer_data_t * data = (timer_data_t*)a;
        Uint32 time = real_time;
        if (config.standardsMode) time = (Uint32)ceil(time / 50.0) * 50;
        data->timer = SDL_AddTimer(time + 3, notifyEvent, data);
        return NULL;
    }, data);
    runningTimerData->insert(std::make_pair(data->timer, data));
    lua_pushinteger(L, data->timer);
    std::lock_guard<std::mutex> lock(computer->timerIDsMutex);
    computer->timerIDs.insert(data->timer);
    return 1;
}

static int os_cancelAlarm(lua_State *L) {
    lastCFunction = __func__;
    const SDL_TimerID id = (SDL_TimerID)luaL_checkinteger(L, 1);
    if (runningTimerData->find(id) == runningTimerData->end()) return 0;
    timer_data_t * data = (*runningTimerData)[id];
    runningTimerData->erase(id);
    data->lock->lock();
#ifdef __EMSCRIPTEN__
    queueTask([id](void*)->void* {SDL_RemoveTimer(id); return NULL; }, NULL);
#else
    SDL_RemoveTimer(id);
#endif
    data->lock->unlock();
    delete data->lock;
    delete data;
    return 0;
}

static int os_shutdown(lua_State *L) {
    lastCFunction = __func__;
    get_comp(L)->running = 0;
    if (selectedRenderer == 1 && lua_isnumber(L, 1)) returnValue = (int)lua_tointeger(L, 1);
    return 0;
}

static int os_reboot(lua_State *L) {
    lastCFunction = __func__;
    get_comp(L)->running = 2;
    return 0;
}

static int os_about(lua_State *L) {
    lastCFunction = __func__;
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

static luaL_Reg os_reg[] = {
    {"getComputerID", os_getComputerID},
    {"computerID", os_getComputerID},
    {"getComputerLabel", os_getComputerLabel},
    {"computerLabel", os_getComputerLabel},
    {"setComputerLabel", os_setComputerLabel},
    {"queueEvent", os_queueEvent},
    {"clock", os_clock},
    {"startTimer", os_startTimer},
    {"cancelTimer", os_cancelTimer},
    {"time", os_time},
    {"epoch", os_epoch},
    {"day", os_day},
    {"setAlarm", os_setAlarm},
    {"cancelAlarm", os_cancelAlarm},
    {"shutdown", os_shutdown},
    {"reboot", os_reboot},
    {"about", os_about},
    {NULL, NULL}
};

library_t os_lib = {"os", os_reg, nullptr, nullptr};