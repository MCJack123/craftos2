/*
 * terminal/CLITerminal.cpp
 * CraftOS-PC 2
 * 
 * This file implements the CLITerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef NO_CLI
static void pressControl(int sig);
static void pressAlt(int sig);
#define PDC_NCMOUSE
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <thread>
#include <unordered_map>
#include <curses.h>
#include <panel.h>
#include "CLITerminal.hpp"
#include "SDLTerminal.hpp"
#include "RawTerminal.hpp"
#include "../main.hpp"
#include "../peripheral/monitor.hpp"
#include "../termsupport.hpp"
#include "../runtime.hpp"
#ifdef NCURSES_BUTTON_PRESSED
#define BUTTON_PRESSED NCURSES_BUTTON_PRESSED
#define BUTTON_RELEASED NCURSES_BUTTON_RELEASED
#endif

std::set<unsigned> CLITerminal::currentIDs;
std::set<unsigned>::iterator CLITerminal::selectedWindow = currentIDs.begin();
bool CLITerminal::stopRender = false;
bool CLITerminal::forceRender = false;
unsigned short CLITerminal::lastPaletteChecksum = 0;

void CLITerminal::renderNavbar(std::string title) {
    move(LINES-1, 0);
    if (stopRender) return;
    attron(COLOR_PAIR(0x78));
    if (stopRender) return;
    clrtoeol();
    if (stopRender) return;
    printw("%d: %s", *selectedWindow+1, title.c_str());
    if (stopRender) return;
    for (int i = getcurx(stdscr); i < COLS-3; i++) addch(' ');
    if (stopRender) return;
    attroff(COLOR_PAIR(0x78));
    if (stopRender) return;
    attron(COLOR_PAIR(0x70));
    if (stopRender) return;
    addstr("<>");
    if (stopRender) return;
    attroff(COLOR_PAIR(0x70));
    if (stopRender) return;
    attron(COLOR_PAIR(0xE0));
    if (stopRender) return;
    addch('X');
    if (stopRender) return;
    attroff(COLOR_PAIR(0xE0));
    if (stopRender) return;
    printf("\033]0;%s\007", title.c_str());
    fflush(stdout);
}

CLITerminal::CLITerminal(std::string title): Terminal(COLS, LINES-1) {
    this->title = title;
    for (id = 0; currentIDs.find(id) != currentIDs.end(); id++);
    selectedWindow = currentIDs.insert(currentIDs.end(), id);
    last_pair = 0;
    renderTargets.push_back(this);
}

CLITerminal::~CLITerminal() {
    auto pos = currentIDs.find(id);
    auto next = currentIDs.erase(pos);
    if (currentIDs.size() == 0) return;
    if (next == currentIDs.end()) next--;
    selectedWindow = next;
    renderTargetsLock.lock();
    std::lock_guard<std::mutex> locked_g(locked);
    for (auto it = renderTargets.begin(); it != renderTargets.end(); it++) {
        if (*it == this)
            it = renderTargets.erase(it);
        if (it == renderTargets.end()) break;
    }
    renderTargetsLock.unlock();
}

bool CLITerminal::drawChar(char c, int x, int y, Color fg, Color bg, bool transparent) {
    return false;
}

void CLITerminal::render() {
    if (forceRender) changed = true;
    if (gotResizeEvent) {
        gotResizeEvent = false;
        this->screen.resize(newWidth, newHeight, ' ');
        this->colors.resize(newWidth, newHeight, 0xF0);
        this->pixels.resize(newWidth * fontWidth, newHeight * fontHeight, 0x0F);
        this->pixelBuffer.resize(newWidth * fontWidth, newHeight * fontHeight, 0x0F);
        this->width = newWidth;
        this->height = newHeight;
        changed = true;
    }
    if (*selectedWindow == id && changed) {
        changed = false;
        std::lock_guard<std::mutex> locked_g(locked);
        move(0, 0);
        if (stopRender) {stopRender = false; return;}
        clear();
        if (stopRender) {stopRender = false; return;}
        if (can_change_color()) {
            unsigned short checksum = grayscale;
            for (int i = 0; i < 48; i++) 
                checksum = (checksum >> 1) + ((checksum & 1) << 15) + ((unsigned char*)palette)[i];
            if (checksum != lastPaletteChecksum) {
                for (int i = 0; i < 16; i++) {
                    if (grayscale) {
                        int c = (palette[i].r + palette[i].g + palette[i].b) * 1000 / 765;
                        init_color(15-i, c, c, c);
                    }
                    else init_color(15-i, palette[i].r * (1000/255), palette[i].g * (1000/255), palette[i].b * (1000/255));
                }
            }
            lastPaletteChecksum = checksum;
        }
        for (unsigned y = 0; y < height; y++) {
            for (unsigned x = 0; x < width; x++) {
                move(y, x);
                addch((screen[y][x] ? screen[y][x] : ' ') | COLOR_PAIR(colors[y][x]));
                if (stopRender) {stopRender = false; return;}
            }
        }
        renderNavbar(title);
        if (stopRender) {stopRender = false; return;}
        move(blinkY, blinkX);
        if (stopRender) {stopRender = false; return;}
        curs_set(canBlink);
        if (stopRender) {stopRender = false; return;}
        refresh();
    }
}

void CLITerminal::getMouse(int *x, int *y) {
    *x = -1;
    *y = -1;
}

bool CLITerminal::resize(unsigned w, unsigned h) {
    newWidth = w;
    newHeight = h;
    gotResizeEvent = (newWidth != width || newHeight != height);
    if (!gotResizeEvent) return false;
    while (gotResizeEvent) std::this_thread::yield();
    return true;
}

void CLITerminal::showMessage(uint32_t flags, const char * title, const char * message) {
    fprintf(stderr, "%s: %s\n", title, message);
}

void CLITerminal::nextWindow() {
    if (++selectedWindow == currentIDs.end()) selectedWindow = currentIDs.begin();
    forceRender = true;
}

void CLITerminal::previousWindow() {
    if (selectedWindow == currentIDs.begin()) selectedWindow = currentIDs.end();
    selectedWindow--;
    forceRender = true;
}

void CLITerminal::setLabel(std::string label) {
    title = label;
    if (*selectedWindow == id) renderNavbar(label);
}

static short original_colors[16][3];
static MEVENT me;
static WINDOW * tmpwin;
static std::set<int> lastch;
static bool resizeRefresh = false;

static void handle_winch(int sig) {
    resizeRefresh = true;
    endwin();
    refresh();
    clear();
}

static void pressControl(int sig) {
    SDL_Event e;
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = (SDL_Keycode)29;
    LockGuard lock(computers);
    for (Computer * c : *computers) {
        if (*CLITerminal::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
            std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
            e.key.windowID = c->term->id;
            c->termEventQueue.push(e);
            e.type = SDL_KEYUP;
            e.key.keysym.sym = (SDL_Keycode)29;
            c->termEventQueue.push(e);
            c->event_lock.notify_all();
        }
    }
}

static void pressAlt(int sig) {
    SDL_Event e;
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = (SDL_Keycode)56;
    LockGuard lock(computers);
    for (Computer * c : *computers) {
        if (*CLITerminal::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
            std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
            e.key.windowID = c->term->id;
            c->termEventQueue.push(e);
            e.type = SDL_KEYUP;
            e.key.keysym.sym = (SDL_Keycode)56;
            c->termEventQueue.push(e);
            c->event_lock.notify_all();
        }
    }
}

void CLITerminal::init() {
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER);
    initscr();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();
    halfdelay(1);
    mousemask(BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON2_PRESSED | BUTTON2_CLICKED | BUTTON2_RELEASED | BUTTON3_PRESSED | BUTTON3_CLICKED | BUTTON3_RELEASED | REPORT_MOUSE_POSITION, NULL);
    mouseinterval(0);
    start_color();
    if (can_change_color()) {
        for (int i = 0; i < 16 && i < COLORS; i++) {
            short r = 0, g = 0, b = 0;
            color_content(i, &r, &g, &b);
            original_colors[i][0] = r;
            original_colors[i][1] = g;
            original_colors[i][2] = b;
        }
    }
    for (int i = 0; i < 256; i++) init_pair(i, 15 - (i & 0x0f), 15 - ((i >> 4) & 0xf));
    if (config.cliControlKeyMode == 0) {
        keymap_cli[KEY_SHOME] = 199;
        keymap_cli[KEY_SEND] = 207;
        keymap_cli[KEY_HOME] = 29;
        keymap_cli[KEY_END] = 56;
    } else {
        keymap_cli[KEY_HOME] = 199;
        keymap_cli[KEY_END] = 207;
        if (config.cliControlKeyMode == 1) {
            keymap_cli[KEY_SHOME] = 29;
            keymap_cli[KEY_SEND] = 56;
        }
    }
    tmpwin = newpad(1, 1);
    nodelay(tmpwin, TRUE);
    keypad(tmpwin, TRUE);
#ifdef SIGWINCH
    signal(SIGWINCH, handle_winch);
    if (config.cliControlKeyMode == 3) {
        signal(SIGINT, pressControl);
        signal(SIGQUIT, pressAlt);
    }
#endif
    renderThread = new std::thread(termRenderLoop);
    setThreadName(*renderThread, "Render Thread");
}

void CLITerminal::quit() {
    delwin(tmpwin);
    renderThread->join();
    delete renderThread;
    if (can_change_color()) for (int i = 0; i < 16 && i < COLORS; i++) init_color(i, original_colors[i][0], original_colors[i][1], original_colors[i][2]);
    echo();
    nocbreak();
    nodelay(stdscr, FALSE);
    keypad(stdscr, FALSE);
    mousemask(0, NULL);
    endwin();
    SDL_Quit();
}

#ifdef __EMSCRIPTEN__
#define checkWindowID(c, wid) (c->term == *SDLTerminal::renderTarget || findMonitorFromWindowID(c, (*SDLTerminal::renderTarget)->id, tmps) != NULL)
#else
#define checkWindowID(c, wid) (wid == c->term->id || findMonitorFromWindowID(c, wid, tmps) != NULL)
#endif

#define sendEventToTermQueue(e, TYPE) \
    if (rawClient) {e.TYPE.windowID = *CLITerminal::selectedWindow; sendRawEvent(e);}\
    else {LockGuard lock(computers);\
        for (Computer * c : *computers) {if (*CLITerminal::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {\
            std::lock_guard<std::mutex> lock(c->termEventQueueMutex);\
            e.TYPE.windowID = c->term->id;\
            c->termEventQueue.push(e);\
            c->event_lock.notify_all();\
        }\
    }}

bool CLITerminal::pollEvents() {
    SDL_Event e;
    std::string tmps;
    int ch = wgetch(tmpwin);
    if (ch == ERR) {
        for (int cc : lastch) {
            if (cc != 27) {
                e.type = SDL_KEYUP;
                e.key.keysym.sym = (SDL_Keycode)(keymap_cli.find(cc) != keymap_cli.end() ? keymap_cli.at(cc) : cc);
                sendEventToTermQueue(e, key);
            }
        }
        lastch.clear();
        nodelay(tmpwin, TRUE);
        keypad(tmpwin, TRUE);
        while (ch == ERR && taskQueue->size() == 0 && !resizeRefresh) ch = wgetch(tmpwin);
    }
    if (resizeRefresh) {
        resizeRefresh = false;
        CLITerminal::stopRender = true;
        delwin(tmpwin);
        endwin();
        refresh();
        tmpwin = newwin(0, 0, 1, 1);
        nodelay(tmpwin, TRUE);
        keypad(tmpwin, TRUE);
        // TODO: Fix this for raw client mode
        e.type = SDL_WINDOWEVENT;
        e.window.event = SDL_WINDOWEVENT_RESIZED;
        LockGuard lock(computers);
        for (Computer * c : *computers) {
            std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
            e.window.data1 = COLS;
            e.window.data2 = LINES - 1;
            e.window.windowID = c->term->id;
            c->term->changed = true;
            c->termEventQueue.push(e);
            c->event_lock.notify_all();
        }
    }
    while (taskQueue->size() > 0) {
        auto v = taskQueue->front();
        void* retval = std::get<1>(v)(std::get<2>(v));
        if (!std::get<3>(v)) {
            LockGuard lock2(taskQueueReturns);
            (*taskQueueReturns)[std::get<0>(v)] = retval;
        }
        taskQueue->pop();
    }
    if (ch == KEY_SLEFT) { CLITerminal::previousWindow(); CLITerminal::renderNavbar(""); } 
    else if (ch == KEY_SRIGHT) { CLITerminal::nextWindow(); CLITerminal::renderNavbar(""); } 
    else if (ch == KEY_MOUSE) {
        if (getmouse(&me) != OK) return false;
        if (me.y == LINES - 1) {
            if (me.bstate & BUTTON1_PRESSED) {
                if (me.x == COLS - 1) {
                    e.type = SDL_WINDOWEVENT;
                    e.window.event = SDL_WINDOWEVENT_CLOSE;
                    if (rawClient) {e.button.windowID = *CLITerminal::selectedWindow; sendRawEvent(e);}
                    else {
                        LockGuard lock(computers);
                        for (Computer * c : *computers) {
                            if (*CLITerminal::selectedWindow == c->term->id || findMonitorFromWindowID(c, *CLITerminal::selectedWindow, tmps) != NULL) {
                                std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
                                e.button.windowID = *CLITerminal::selectedWindow;
                                c->termEventQueue.push(e);
                                c->event_lock.notify_all();
                            }
                        }
                        for (Terminal * t : orphanedTerminals) {
                            if (t->id == *CLITerminal::selectedWindow) {
                                orphanedTerminals.erase(t);
                                delete t;
                                break;
                            }
                        }
                    }
                } else if (me.x == COLS - 2) { CLITerminal::nextWindow(); CLITerminal::renderNavbar(""); } 
                else if (me.x == COLS - 3) { CLITerminal::previousWindow(); CLITerminal::renderNavbar(""); }
            }
            return false;
        }
        if (me.bstate & BUTTON_PRESSED) e.type = SDL_MOUSEBUTTONDOWN;
        else if (me.bstate & BUTTON_RELEASED) e.type = SDL_MOUSEBUTTONUP;
        else return false;
        if ((me.bstate & BUTTON1_PRESSED) || (me.bstate & BUTTON1_RELEASED)) e.button.button = SDL_BUTTON_LEFT;
        else if ((me.bstate & BUTTON2_PRESSED) || (me.bstate & BUTTON2_RELEASED)) e.button.button = SDL_BUTTON_RIGHT;
        else if ((me.bstate & BUTTON3_PRESSED) || (me.bstate & BUTTON3_RELEASED)) e.button.button = SDL_BUTTON_MIDDLE;
        else return false;
        e.button.x = me.x + 1;
        e.button.y = me.y + 1;
        sendEventToTermQueue(e, button);
    } else if (ch != ERR && ch != KEY_RESIZE) {
        if (config.cliControlKeyMode == 2 && ch == 'c' && lastch.find(27) != lastch.end()) ch = (SDL_Keycode)1025;
        else if (config.cliControlKeyMode == 2 && ch == 'a' && lastch.find(27) != lastch.end()) ch = (SDL_Keycode)1026;
        if ((ch >= 32 && ch < 127)) {
            e.type = SDL_TEXTINPUT;
            e.text.text[0] = ch;
            e.text.text[1] = '\0';
            sendEventToTermQueue(e, text);
        }
        e.type = SDL_KEYDOWN;
        e.key.keysym.sym = (SDL_Keycode)(keymap_cli.find(ch) != keymap_cli.end() ? keymap_cli.at(ch) : ch);
        if (ch == '\n') e.key.keysym.sym = (SDL_Keycode)28;
        if (ch != 27) {
            sendEventToTermQueue(e, key);
        }
        lastch.insert(ch);
    }
    return false;
}
#endif
