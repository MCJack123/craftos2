/*
 * terminal/TRoRTerminal.cpp
 * CraftOS-PC 2
 * 
 * This file implements the TRoRTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include <iostream>
#include <cstdio>
#include <thread>
#include "TRoRTerminal.hpp"
#include "SDLTerminal.hpp"
#include "../peripheral/monitor.hpp"
#include "../runtime.hpp"
#include "../termsupport.hpp"

/*
CraftOS-PC adds the "ccpcTerm" extension to add some extra features. Even if the
client doesn't support "ccpcTerm", the ID of the window is always sent in the 
metadata field. If the client sends a packet without an ID in the metadata, it
is assumed to be meant for the first window (ID 0).
| Code | Payload       | Description |
|------|---------------|-------------|
| `TN` | `<title>`     | Alerts the client to a newly opened window. |
| `TQ` | None          | Alerts the client or server that the window has been closed. Clients and servers MAY send this at any time. |
| `TZ` | `<title>`     | Alerts the client that the window's title has changed. |
| `TA` | `"<title>","<message>"` | Shows a message on the client's screen. The title and message will both be in quotes and separated by a comma. The client SHOULD NOT split the string at the first comma since there may be commas before the separator. |
| `TR` | `<w>,<h>`     | With the CraftOS-PC extension, clients MAY send a resize message to the server as well. |
| `SC` | `<message>`   | With the CraftOS-PC extension, clients MAY send a close message to the server as well. |
*/

std::set<unsigned> TRoRTerminal::currentIDs;
static std::unordered_set<std::string> trorExtensions;
static std::thread * inputThread;

#ifdef __EMSCRIPTEN__
#define checkWindowID(c, wid) (c->term == *SDLTerminal::renderTarget || findMonitorFromWindowID(c, (*SDLTerminal::renderTarget)->id, tmps) != NULL)
#else
#define checkWindowID(c, wid) ((wid) == (c)->term->id || findMonitorFromWindowID((c), (wid), tmps) != NULL)
#endif

static std::string trorEvent(lua_State *L, void* userp) {
    std::string * str = (std::string*)userp;
    if (luaL_loadstring(L, ("return " + *str).c_str())) {
        std::cerr << "Could not load function (" << *str << ")\n";
        delete str;
        return NULL;
    }
    delete str;
    lua_newtable(L);
    lua_setfenv(L, -2);
    lua_call(L, 0, LUA_MULTRET);
    std::string name = lua_tostring(L, 1);
    lua_remove(L, 1);
    return name;
}

static void trorInputLoop() {
    std::string tmps;
    while (!exiting) {
        std::string line;
        std::getline(std::cin, line);
        if (line[2] != ':') continue;
        std::string code = line.substr(0, 2);
        std::string meta = line.substr(3, line.find(';') - 3);
        std::string payload = line.substr(line.find(';') + 1);
        const unsigned id = meta.empty() ? 0 : std::stoi(meta);
        if (code == "SP") {
            std::vector<std::string> args = split(payload, '-');
            for (const std::string& a : args) if (!a.empty()) trorExtensions.insert(a);
        } else if (code == "EV") {
            LockGuard lock(computers);
            for (Computer * c : *computers)
                if (checkWindowID(c, id))
                    queueEvent(c, trorEvent, new std::string(payload));
        } else if (code == "SC") {
            SDL_Event e;
            memset(&e, 0, sizeof(SDL_Event));
            e.type = SDL_QUIT;
            LockGuard lockc(computers);
            for (Computer * c : *computers) {
                std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
                c->termEventQueue.push(e);
                c->event_lock.notify_all();
            }
        } else if (code == "TR") {
            const int newWidth = std::stoi(payload.substr(0, payload.find(','))), newHeight = std::stoi(payload.substr(payload.find(',') + 1));
            LockGuard lockc(computers);
            for (Computer * c : *computers) {
                if (id == c->term->id) {
                    std::lock_guard<std::mutex> lock(c->term->locked);
                    c->term->screen.resize(newWidth, newHeight, ' ');
                    c->term->colors.resize(newWidth, newHeight, 0xF0);
                    c->term->pixels.resize(newWidth * Terminal::fontWidth, newHeight * Terminal::fontHeight, 0x0F);
                    c->term->pixelBuffer.resize(newWidth * Terminal::fontWidth, newHeight * Terminal::fontHeight, 0x0F);
                    c->term->width = newWidth;
                    c->term->height = newHeight;
                } else {
                    monitor * m = findMonitorFromWindowID(c, id, tmps);
                    if (m != NULL) {
                        std::lock_guard<std::mutex> lock(m->term->locked);
                        m->term->screen.resize(newWidth, newHeight, ' ');
                        m->term->colors.resize(newWidth, newHeight, 0xF0);
                        m->term->pixels.resize(newWidth * Terminal::fontWidth, newHeight * Terminal::fontHeight, 0x0F);
                        m->term->pixelBuffer.resize(newWidth * Terminal::fontWidth, newHeight * Terminal::fontHeight, 0x0F);
                        m->term->width = newWidth;
                        m->term->height = newHeight;
                    }
                }
            }
        } else if (code == "TQ") {
            SDL_Event e;
            memset(&e, 0, sizeof(SDL_Event));
            e.type = SDL_WINDOWEVENT;
            e.window.event = SDL_WINDOWEVENT_CLOSE;
            e.window.windowID = id;
            LockGuard lockc(computers);
            for (Computer * c : *computers) {
                if (checkWindowID(c, id)) {
                    std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
                    c->termEventQueue.push(e);
                    c->event_lock.notify_all();
                }
            }
            for (Terminal * t : orphanedTerminals) {
                if (t->id == id) {
                    orphanedTerminals.erase(t);
                    delete t;
                    break;
                }
            }
        }
    }
}

void TRoRTerminal::init() {
    SDL_Init(SDL_INIT_TIMER);
    renderThread = new std::thread(termRenderLoop);
    inputThread = new std::thread(trorInputLoop);
    setThreadName(*renderThread, "Render Thread");
    printf("SP:;-ccpcTerm-\n");
}

void TRoRTerminal::quit() {
    printf("SC:;Server closed\n");
    renderThread->join();
    delete renderThread;
    inputThread->join();
    delete inputThread;
    SDL_Quit();
}

void TRoRTerminal::showGlobalMessage(Uint32 flags, const char * title, const char * message) {
    // This may be called before initialization, so we're always sending it
    printf("TA:;\"%s\",\"%s\"\n", title, message);
}

TRoRTerminal::TRoRTerminal(std::string title): Terminal(config.defaultWidth, config.defaultHeight) {
    this->title = title;
    for (id = 0; currentIDs.find(id) != currentIDs.end(); id++) 
        ;
    if (trorExtensions.find("ccpcTerm") != trorExtensions.end()) printf("TN:%d;%s\n", id, title.c_str());
    renderTargets.push_back(this);
}

TRoRTerminal::~TRoRTerminal() {
    if (trorExtensions.find("ccpcTerm") != trorExtensions.end()) printf("TQ:%d;\n", id);
    const auto pos = currentIDs.find(id);
    if (pos != currentIDs.end()) currentIDs.erase(pos);
    renderTargetsLock.lock();
    std::lock_guard<std::mutex> locked_g(locked);
    for (auto it = renderTargets.begin(); it != renderTargets.end(); ++it) {
        if (*it == this)
            it = renderTargets.erase(it);
        if (it == renderTargets.end()) break;
    }
    renderTargetsLock.unlock();
}

void TRoRTerminal::showMessage(Uint32 flags, const char * title, const char * message) {
    if (trorExtensions.find("ccpcTerm") != trorExtensions.end()) printf("TA:%d;\"%s\",\"%s\"\n", id, title, message);
}

void TRoRTerminal::setLabel(std::string label) {
    this->title = label;
    if (trorExtensions.find("ccpcTerm") != trorExtensions.end()) printf("TZ:%d;%s\n", id, label.c_str());
}
