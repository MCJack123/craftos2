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
#include <algorithm>
#include <unordered_set>
#include "os.hpp"
#include "platform.hpp"
#include "term.hpp"
#include "config.hpp"
#include "CLITerminalWindow.hpp"
#include "peripheral/monitor.hpp"

void gettingEvent(Computer *comp);
void gotEvent(Computer *comp);
extern monitor * findMonitorFromWindowID(Computer *comp, unsigned id, std::string& sideReturn);

int nextTaskID = 0;
std::queue< std::tuple<int, std::function<void*(void*)>, void*> > taskQueue;
std::unordered_map<int, void*> taskQueueReturns;
bool exiting = false;
extern bool cli, headless;
extern Uint32 task_event_type;
extern Uint32 render_event_type;
extern std::unordered_map<int, unsigned char> keymap_cli;
extern std::unordered_map<int, unsigned char> keymap;

void* queueTask(std::function<void*(void*)> func, void* arg) {
    int myID = nextTaskID++;
    taskQueue.push(std::make_tuple(myID, func, arg));
    if (!headless && !cli && !exiting) {
        SDL_Event ev;
        ev.type = task_event_type;
        SDL_PushEvent(&ev);
    }
    while (taskQueueReturns.find(myID) == taskQueueReturns.end() && !exiting) std::this_thread::yield();
    void* retval = taskQueueReturns[myID];
    taskQueueReturns.erase(myID);
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
    SDL_Event e;
    std::string tmps;
#ifndef NO_CLI
    MEVENT me;
    WINDOW * tmpwin;
    std::list<int> lastch;
    if (cli) { 
        tmpwin = newwin(0, 0, 1, 1);
        nodelay(tmpwin, TRUE);
        keypad(tmpwin, TRUE);
    }
#endif
    while (computers.size() > 0) {
        if (!headless && !cli && SDL_WaitEvent(&e)) { 
            if (e.type == task_event_type) {
                while (taskQueue.size() > 0) {
                    auto v = taskQueue.front();
                    void* retval = std::get<1>(v)(std::get<2>(v));
                    taskQueueReturns[std::get<0>(v)] = retval;
                    taskQueue.pop();
                }
            } else if (e.type == render_event_type) {
                for (TerminalWindow* term : TerminalWindow::renderTargets) {
                    term->locked.lock();
                    SDL_BlitSurface(term->surf, NULL, SDL_GetWindowSurface(term->win), NULL);
                    SDL_UpdateWindowSurface(term->win);
                    SDL_FreeSurface(term->surf);
                    term->surf = NULL;
                    term->locked.unlock();
                }
            } else {
                for (Computer * c : computers) {
                    if (((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && (e.key.windowID == c->term->id || findMonitorFromWindowID(c, e.key.windowID, tmps) != NULL)) ||
                        ((e.type == SDL_DROPFILE || e.type == SDL_DROPTEXT || e.type == SDL_DROPBEGIN || e.type == SDL_DROPCOMPLETE) && (e.drop.windowID == c->term->id || findMonitorFromWindowID(c, e.drop.windowID, tmps) != NULL)) ||
                        ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && (e.button.windowID == c->term->id || findMonitorFromWindowID(c, e.button.windowID, tmps) != NULL)) ||
                        (e.type == SDL_MOUSEMOTION && (e.motion.windowID == c->term->id || findMonitorFromWindowID(c, e.motion.windowID, tmps) != NULL)) ||
                        (e.type == SDL_MOUSEWHEEL && (e.wheel.windowID == c->term->id || findMonitorFromWindowID(c, e.wheel.windowID, tmps) != NULL)) ||
                        (e.type == SDL_TEXTINPUT && (e.text.windowID == c->term->id || findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL)) ||
                        (e.type == SDL_WINDOWEVENT && (e.window.windowID == c->term->id || findMonitorFromWindowID(c, e.window.windowID, tmps) != NULL)) ||
                        e.type == SDL_QUIT) {
                        c->termEventQueue.push(e);
                        c->event_lock.notify_all();
                    }
                }
            }
#ifndef NO_CLI
        } else if (cli) {
            int ch = wgetch(tmpwin);
            if (ch == ERR) {
                for (int cc : lastch) {
                    e.type = SDL_KEYUP;
                    e.key.keysym.scancode = (SDL_Scancode)(keymap_cli.find(cc) != keymap_cli.end() ? keymap_cli.at(cc) : cc);
                    for (Computer * c : computers) {
                        if (*CLITerminalWindow::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
                            e.key.windowID = c->term->id;
                            c->termEventQueue.push(e);
                            c->event_lock.notify_all();
                        }
                    }
                }
                lastch.clear();
                nodelay(tmpwin, TRUE);
                keypad(tmpwin, TRUE);
                while (ch == ERR && taskQueue.size() == 0) ch = wgetch(tmpwin);
            }
            while (taskQueue.size() > 0) {
                auto v = taskQueue.front();
                void* retval = std::get<1>(v)(std::get<2>(v));
                taskQueueReturns[std::get<0>(v)] = retval;
                taskQueue.pop();
            }
            if (ch == KEY_SLEFT) {CLITerminalWindow::previousWindow(); CLITerminalWindow::renderNavbar("");}
            else if (ch == KEY_SRIGHT) {CLITerminalWindow::nextWindow(); CLITerminalWindow::renderNavbar("");}
            else if (ch == KEY_MOUSE) {
                getmouse(&me);
                if (me.bstate & NCURSES_BUTTON_PRESSED) e.type = SDL_MOUSEBUTTONDOWN;
                else if (me.bstate & NCURSES_BUTTON_RELEASED) e.type = SDL_MOUSEBUTTONUP;
                else continue;
                if ((me.bstate & BUTTON1_PRESSED) || (me.bstate & BUTTON1_RELEASED)) e.button.button = SDL_BUTTON_LEFT;
                else if ((me.bstate & BUTTON2_PRESSED) || (me.bstate & BUTTON2_RELEASED)) e.button.button = SDL_BUTTON_RIGHT;
                else if ((me.bstate & BUTTON3_PRESSED) || (me.bstate & BUTTON3_RELEASED)) e.button.button = SDL_BUTTON_MIDDLE;
                else continue;
                e.button.x = me.x + 1;
                e.button.y = me.y + 1;
                for (Computer * c : computers) {
                    if (*CLITerminalWindow::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
                        e.button.windowID = c->term->id;
                        c->termEventQueue.push(e);
                        c->event_lock.notify_all();
                    }
                }
            } else if (ch != ERR) {
                if ((ch >= 32 && ch < 127)) {
                    e.type = SDL_TEXTINPUT;
                    e.text.text[0] = ch;
                    e.text.text[1] = '\0';
                    for (Computer * c : computers) {
                        if (*CLITerminalWindow::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
                            e.text.windowID = c->term->id;
                            c->termEventQueue.push(e);
                            c->event_lock.notify_all();
                        }
                    }
                }
                e.type = SDL_KEYDOWN;
                e.key.keysym.scancode = (SDL_Scancode)(keymap_cli.find(ch) != keymap_cli.end() ? keymap_cli.at(ch) : ch);
                if (ch == '\n') e.key.keysym.scancode = (SDL_Scancode)28;
                for (Computer * c : computers) {
                    if (*CLITerminalWindow::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
                        e.key.windowID = c->term->id;
                        c->termEventQueue.push(e);
                        c->event_lock.notify_all();
                    }
                }
                lastch.push_back(ch);
            }
#endif
        } else {
            while (taskQueue.size() == 0) std::this_thread::yield();
            while (taskQueue.size() > 0) {
                auto v = taskQueue.front();
                void* retval = std::get<1>(v)(std::get<2>(v));
                taskQueueReturns[std::get<0>(v)] = retval;
                taskQueue.pop();
            }
        }
        std::this_thread::yield();
    }
#ifndef NO_CLI
    if (cli) delwin(tmpwin);
#endif
    exiting = true;
}

int getNextEvent(lua_State *L, const char * filter) {
    Computer * computer = get_comp(L);
    if (computer->paramQueue == NULL) computer->paramQueue = lua_newthread(L);
    std::string ev;
    gettingEvent(computer);
    if (!lua_checkstack(computer->paramQueue, 1)) {
        lua_pushstring(L, "Could not allocate space for event");
        lua_error(L);
    }
    lua_State *param = lua_newthread(computer->paramQueue);
    do {
        while (termHasEvent(computer)) {
            if (!lua_checkstack(param, 4)) printf("Could not allocate event\n");
            const char * name = termGetEvent(param);
            if (name != NULL) {
                if (strcmp(name, "die") == 0) computer->running = 0;
                computer->eventQueue.push(name);
                param = lua_newthread(computer->paramQueue);
            }
        }
        while (computer->eventQueue.size() == 0) {
            if (computer->alarms.size() == 0) {
                std::mutex m;
                std::unique_lock<std::mutex> l(m);
                computer->event_lock.wait(l);
            }
            if (computer->running != 1) return 0;
            if (computer->timers.size() > 0 && computer->timers.back().time_since_epoch().count() == 0) computer->timers.pop_back();
            if (computer->alarms.size() > 0 && computer->alarms.back() == -1) computer->alarms.pop_back();
            if (computer->timers.size() > 0) {
                std::chrono::steady_clock::time_point t = std::chrono::steady_clock::now();
                for (unsigned i = 0; i < computer->timers.size(); i++) {
                    if (t >= computer->timers[i] && computer->timers[i].time_since_epoch().count() > 0) {
                        lua_pushinteger(param, i);
                        computer->eventQueue.push("timer");
                        computer->timers[i] = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(0));
                        param = lua_newthread(computer->paramQueue);
                    }
                }
            }
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
            while (termHasEvent(computer)) {
                if (!lua_checkstack(param, 4)) printf("Could not allocate event\n");
                const char * name = termGetEvent(param);
                if (name != NULL) {
                    if (strcmp(name, "die") == 0) computer->running = 0;
                    computer->eventQueue.push(name);
                    param = lua_newthread(computer->paramQueue);
                }
            }
        }
        ev = computer->eventQueue.front();
        computer->eventQueue.pop();
        std::this_thread::yield();
    } while (strlen(filter) > 0 && ev != std::string(filter));
    lua_pop(computer->paramQueue, 1);
    param = lua_tothread(computer->paramQueue, 1);
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
    struct computer_configuration cfg = get_comp(L)->config;
    if (cfg.label.empty()) return 0;
    lua_pushstring(L, cfg.label.c_str());
    return 1;
}

int os_setComputerLabel(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    get_comp(L)->config.label = lua_tostring(L, 1);
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
    computer->event_lock.notify_all();
    return 0;
}

int os_clock(lua_State *L) {
    Computer * computer = get_comp(L);
    lua_pushnumber(L, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - computer->system_start).count() / 1000.0);
    return 1;
}

Uint32 notifyEvent(Uint32 interval, void* param) {
    if (exiting) return interval;
    ((Computer*)param)->event_lock.notify_all();
    return interval;
}

int os_startTimer(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    Computer * computer = get_comp(L);
    computer->timers.push_back(std::chrono::steady_clock::now() + std::chrono::milliseconds((long)(lua_tonumber(L, 1) * 1000)));
    lua_pushinteger(L, computer->timers.size() - 1);
    SDL_AddTimer(lua_tonumber(L, 1) * 1000, notifyEvent, computer);
    return 1;
}

int os_cancelTimer(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    Computer * computer = get_comp(L);
    unsigned id = lua_tointeger(L, 1);
    if (id == computer->timers.size() - 1) computer->timers.pop_back();
    else computer->timers[id] = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(0));
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
    if (!config.debug_enable) return 0;
    lua_pushinteger(L, system(lua_tostring(L, 1)));
    return 1;
}


int os_about(lua_State *L) {
    lua_pushstring(L, "CraftOS-PC " CRAFTOSPC_VERSION "\n\nCraftOS-PC 2 is licensed under the MIT License.\nMIT License\n\
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
