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
#ifndef NO_CLI
#include <signal.h>
#endif
#ifdef __EMSCRIPTEN__
#define checkWindowID(c, wid) (c->term == *TerminalWindow::renderTarget || findMonitorFromWindowID(c, (*TerminalWindow::renderTarget)->id, tmps) != NULL)
#else
#define checkWindowID(c, wid) (wid == c->term->id || findMonitorFromWindowID(c, wid, tmps) != NULL)
#endif

void gettingEvent(Computer *comp);
void gotEvent(Computer *comp);
extern monitor * findMonitorFromWindowID(Computer *comp, unsigned id, std::string& sideReturn);

int nextTaskID = 0;
std::queue< std::tuple<int, std::function<void*(void*)>, void*, bool> > taskQueue;
std::unordered_map<int, void*> taskQueueReturns;
bool exiting = false;
bool forceCheckTimeout = false;
extern bool cli, headless;
extern Uint32 task_event_type;
extern Uint32 render_event_type;
extern std::unordered_map<int, unsigned char> keymap_cli;
extern std::unordered_map<int, unsigned char> keymap;
std::thread::id mainThreadID;

void* queueTask(std::function<void*(void*)> func, void* arg, bool async) {
    if (std::this_thread::get_id() == mainThreadID) return func(arg);
    int myID = nextTaskID++;
    taskQueue.push(std::make_tuple(myID, func, arg, async));
    if (!headless && !cli && !exiting) {
        SDL_Event ev;
        ev.type = task_event_type;
        SDL_PushEvent(&ev);
    }
    if (async) return NULL;
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

#ifndef NO_CLI
extern std::set<unsigned> currentIDs;
bool resizeRefresh = false;

void handle_winch(int sig) {
    resizeRefresh = true;
    endwin();
    refresh();
    clear();
}

void pressControl(int sig) {
    SDL_Event e;
    e.type = SDL_KEYDOWN;
    e.key.keysym.scancode = (SDL_Scancode)29;
    for (Computer * c : computers) {
        if (*CLITerminalWindow::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
            e.key.windowID = c->term->id;
            c->termEventQueue.push(e);
            e.type = SDL_KEYUP;
            e.key.keysym.scancode = (SDL_Scancode)29;
            c->termEventQueue.push(e);
            c->event_lock.notify_all();
        }
    }
}

void pressAlt(int sig) {
    SDL_Event e;
    e.type = SDL_KEYDOWN;
    e.key.keysym.scancode = (SDL_Scancode)56;
    for (Computer * c : computers) {
        if (*CLITerminalWindow::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
            e.key.windowID = c->term->id;
            c->termEventQueue.push(e);
            e.type = SDL_KEYUP;
            e.key.keysym.scancode = (SDL_Scancode)56;
            c->termEventQueue.push(e);
            c->event_lock.notify_all();
        }
    }
}
#endif

void mainLoop() {
    SDL_Event e;
    std::string tmps;
#ifndef NO_CLI
    MEVENT me;
    WINDOW * tmpwin;
    std::set<int> lastch;
    if (cli) { 
        tmpwin = newwin(0, 0, 1, 1);
        nodelay(tmpwin, TRUE);
        keypad(tmpwin, TRUE);
        signal(SIGWINCH, handle_winch);
        if (config.cliControlKeyMode == 3) {
            signal(SIGINT, pressControl);
            signal(SIGQUIT, pressAlt);
        }
    }
#endif
    mainThreadID = std::this_thread::get_id();
#ifndef __EMSCRIPTEN__
    while (computers.size() > 0) {
#endif
        if (!headless && !cli) { 
#ifdef __EMSCRIPTEN__
            if (SDL_PollEvent(&e)) {
#else
            if (SDL_WaitEvent(&e)) {
#endif
                if (e.type == task_event_type) {
                    while (taskQueue.size() > 0) {
                        auto v = taskQueue.front();
                        void* retval = std::get<1>(v)(std::get<2>(v));
                        taskQueueReturns[std::get<0>(v)] = retval;
                        taskQueue.pop();
                    }
                } else if (e.type == render_event_type) {
#ifdef __EMSCRIPTEN__
                    TerminalWindow* term = *TerminalWindow::renderTarget;
                    std::lock_guard<std::mutex> lock(term->locked);
                    if (term->surf != NULL) {
                        SDL_BlitSurface(term->surf, NULL, SDL_GetWindowSurface(TerminalWindow::win), NULL);
                        SDL_UpdateWindowSurface(TerminalWindow::win);
                        SDL_FreeSurface(term->surf);
                        term->surf = NULL;
                    }
#else
                    for (TerminalWindow* term : TerminalWindow::renderTargets) {
                        std::lock_guard<std::mutex> lock(term->locked);
                        if (term->surf != NULL) {
                            SDL_BlitSurface(term->surf, NULL, SDL_GetWindowSurface(term->win), NULL);
                            SDL_UpdateWindowSurface(term->win);
                            SDL_FreeSurface(term->surf);
                            term->surf = NULL;
                        }
                    }
#endif
                } else {
                    for (Computer * c : computers) {
                        if (((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && checkWindowID(c, e.key.windowID)) ||
                            ((e.type == SDL_DROPFILE || e.type == SDL_DROPTEXT || e.type == SDL_DROPBEGIN || e.type == SDL_DROPCOMPLETE) && checkWindowID(c, e.drop.windowID)) ||
                            ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && checkWindowID(c, e.button.windowID)) ||
                            (e.type == SDL_MOUSEMOTION && checkWindowID(c, e.motion.windowID)) ||
                            (e.type == SDL_MOUSEWHEEL && checkWindowID(c, e.wheel.windowID)) ||
                            (e.type == SDL_TEXTINPUT && checkWindowID(c, e.text.windowID)) ||
                            (e.type == SDL_WINDOWEVENT && checkWindowID(c, e.window.windowID)) ||
                            e.type == SDL_QUIT) {
                            c->termEventQueue.push(e);
                            c->event_lock.notify_all();
                        }
                    }
                    if (e.type == SDL_QUIT) 
                    #ifdef __EMSCRIPTEN__
                        return;
                    #else
                        break;
                    #endif
                }
            }
#ifndef NO_CLI
        } else if (cli) {
            int ch = wgetch(tmpwin);
            if (ch == ERR) {
                for (int cc : lastch) {
                    if (cc != 27) {
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
                }
                lastch.clear();
                nodelay(tmpwin, TRUE);
                keypad(tmpwin, TRUE);
                while (ch == ERR && taskQueue.size() == 0 && !resizeRefresh) ch = wgetch(tmpwin);
            }
            if (resizeRefresh) {
                resizeRefresh = false;
                CLITerminalWindow::stopRender = true;
                delwin(tmpwin);
                endwin();
                refresh();
                tmpwin = newwin(0, 0, 1, 1);
                nodelay(tmpwin, TRUE);
                keypad(tmpwin, TRUE);
                e.type = SDL_WINDOWEVENT;
                e.window.event = SDL_WINDOWEVENT_RESIZED;
                for (Computer * c : computers) {
                    e.window.data1 = COLS * c->term->charWidth + 4*(2/TerminalWindow::fontScale)*c->term->charScale;
                    e.window.data2 = (LINES-1) * c->term->charHeight + 4*(2/TerminalWindow::fontScale)*c->term->charScale;
                    e.window.windowID = c->term->id;
                    c->term->changed = true;
                    c->termEventQueue.push(e);
                    c->event_lock.notify_all();
                }
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
                if (getmouse(&me) != OK) continue;
                if (me.y == LINES - 1) {
                    if (me.bstate & BUTTON1_PRESSED) {
                        if (me.x == COLS - 1) {
                            e.type = SDL_WINDOWEVENT;
                            e.window.event = SDL_WINDOWEVENT_CLOSE;
                            for (Computer * c : computers) {
                                if (*CLITerminalWindow::selectedWindow == c->term->id || findMonitorFromWindowID(c, *CLITerminalWindow::selectedWindow, tmps) != NULL) {
                                    e.button.windowID = *CLITerminalWindow::selectedWindow;
                                    c->termEventQueue.push(e);
                                    c->event_lock.notify_all();
                                }
                            }
                        } else if (me.x == COLS - 2) {CLITerminalWindow::nextWindow(); CLITerminalWindow::renderNavbar("");}
                        else if (me.x == COLS - 3) {CLITerminalWindow::previousWindow(); CLITerminalWindow::renderNavbar("");}
                    }
                    continue;
                }
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
            } else if (ch != ERR && ch != KEY_RESIZE) {
                if (config.cliControlKeyMode == 2 && ch == 'c' && lastch.find(27) != lastch.end()) ch = (SDL_Scancode)1025;
                else if (config.cliControlKeyMode == 2 && ch == 'a' && lastch.find(27) != lastch.end()) ch = (SDL_Scancode)1026;
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
                if (ch != 27) {
                    for (Computer * c : computers) {
                        if (*CLITerminalWindow::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
                            e.key.windowID = c->term->id;
                            c->termEventQueue.push(e);
                            c->event_lock.notify_all();
                        }
                    }
                }
                lastch.insert(ch);
            }
#endif
        } else {
            while (taskQueue.size() == 0) std::this_thread::yield();
            while (taskQueue.size() > 0) {
                auto v = taskQueue.front();
                void* retval = std::get<1>(v)(std::get<2>(v));
                if (!std::get<3>(v)) taskQueueReturns[std::get<0>(v)] = retval;
                taskQueue.pop();
            }
        }
        std::this_thread::yield();
#ifdef __EMSCRIPTEN__
    if (computers.size() == 0) exiting = true;
#else
    }
#ifndef NO_CLI
    if (cli) delwin(tmpwin);
#endif
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
    if (computer->eventTimeout != 0) {SDL_RemoveTimer(computer->eventTimeout); computer->eventTimeout = 0; computer->timeoutCheckCount = 0;}
    std::string ev;
    gettingEvent(computer);
    if (!lua_checkstack(computer->paramQueue, 1)) {
        lua_pushstring(L, "Could not allocate space for event");
        lua_error(L);
    }
    lua_State *param = lua_newthread(computer->paramQueue);
    do {
        while (termHasEvent(computer) && computer->eventQueue.size() < 25) {
            if (!lua_checkstack(param, 4)) printf("Could not allocate event\n");
            const char * name = termGetEvent(param);
            if (name != NULL) {
                if (strcmp(name, "die") == 0) computer->running = 0;
                computer->eventQueue.push(name);
                param = lua_newthread(computer->paramQueue);
            }
        }
		if (computer->running != 1) return 0;
        while (computer->eventQueue.size() == 0) {
            if (computer->alarms.size() == 0) {
                std::mutex m;
                std::unique_lock<std::mutex> l(m);
                computer->event_lock.wait(l);
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
                    if (strcmp(name, "die") == 0) computer->running = 0;
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
        lua_remove(computer->paramQueue, 1);
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
    if (!headless) comp->term->setLabel(comp->config.label.empty() ? "CraftOS Terminal: " + std::string(comp->isDebugger ? "Debugger" : "Computer") + " " + std::to_string(comp->id) : "CraftOS Terminal: " + asciify(comp->config.label));
    return 0;
}

int os_queueEvent(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
    //if (paramQueue == NULL) paramQueue = lua_newthread(L);
    std::string name = std::string(lua_tostring(L, 1), lua_strlen(L, 1));
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

const char * timer_event(lua_State *L, void* param) {
    struct timer_data_t * data = (struct timer_data_t*)param;
    lua_pushinteger(L, data->timer);
    delete data;
    return "timer";
}

Uint32 notifyEvent(Uint32 interval, void* param) {
    if (exiting) return 0;
	struct timer_data_t * data = (struct timer_data_t*)param;
	{
		std::lock_guard<std::mutex> lock(freedTimersMutex);
		if (freedTimers.find(data->timer) != freedTimers.end()) { 
			freedTimers.erase(data->timer);
            delete data;
			return 0;
		}
	}
	if (data->comp->timerIDs.find(data->timer) != data->comp->timerIDs.end()) data->comp->timerIDs.erase(data->timer);
    termQueueProvider(data->comp, timer_event, data);
    data->comp->event_lock.notify_all();
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
    lua_pushinteger(L, data->timer);
	computer->timerIDs.insert(data->timer);
    return 1;
}

int os_cancelTimer(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    queueTask([L](void*)->void*{SDL_RemoveTimer(lua_tointeger(L, 1)); return NULL;}, NULL);
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

library_t os_lib = {"os", 19, os_keys, os_values, nullptr, nullptr};
