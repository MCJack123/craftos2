/*
 * terminal/CLITerminal.cpp
 * CraftOS-PC 2
 * 
 * This file implements the CLITerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#ifndef NO_CLI
static void pressControl(int sig);
static void pressAlt(int sig);
#define PDC_NCMOUSE
#define PDC_WIDE
#include <cwchar>
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

int width, height;

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
    printw("%d: %s", (*renderTarget)->id+1, title.c_str());
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

CLITerminal::CLITerminal(std::string title): Terminal(::width, ::height-1) {
    this->title = title;
    for (id = 0; currentWindowIDs.find(id) != currentWindowIDs.end(); id++);
    last_pair = 0;
    renderTargets.push_back(this);
    renderTarget = --renderTargets.end();
    onActivate();
}

CLITerminal::~CLITerminal() {
    const auto pos = currentWindowIDs.find(id);
    if (pos != currentWindowIDs.end()) currentWindowIDs.erase(pos);
    if (*renderTarget == this) previousRenderTarget();
    std::lock_guard<std::mutex> lock(renderTargetsLock);
    std::lock_guard<std::mutex> locked_g(locked);
    for (auto it = renderTargets.begin(); it != renderTargets.end(); ++it) {
        if (*it == this)
            it = renderTargets.erase(it);
        if (it == renderTargets.end()) break;
    }
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
        this->width = newWidth;
        this->height = newHeight;
        changed = true;
    }
    if (changed) {
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
                wchar_t ch[2] = {charsetConversion[screen[y][x]], 0};
                if (ch[0] < 0x20) ch[0] = 0x20;
#ifdef WACS_ULCORNER
                cchar_t cc;
                setcchar(&cc, ch, 0, colors[y][x], NULL);
                add_wch(&cc);
#else
                addch((ch[0] < 0x100 ? ch[0] : '?') | COLOR_PAIR(colors[y][x]));
#endif
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

void CLITerminal::setLabel(std::string label) {
    title = label;
    if (*renderTarget == this) renderNavbar(label);
}

void CLITerminal::onActivate() {
    renderNavbar(title);
    forceRender = true;
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
        if (*renderTarget == c->term/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
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
        if (*renderTarget == c->term/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
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
    singleWindowMode = true;
    setlocale(LC_ALL, "");
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
    if (config.defaultWidth == 51 && config.defaultHeight == 19) {
        ::width = COLS;
        ::height = LINES;
    } else {
        ::width = config.defaultWidth;
        ::height = config.defaultHeight;
    }
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
#define checkWindowID(c, wid) (c->term == *renderTarget || findMonitorFromWindowID(c, (*renderTarget)->id, tmps) != NULL)
#else
#define checkWindowID(c, wid) (wid == c->term->id || findMonitorFromWindowID(c, wid, tmps) != NULL)
#endif

#define sendEventToTermQueue(e, TYPE) \
    if (rawClient) {e.TYPE.windowID = (*renderTarget)->id; sendRawEvent(e);}\
    else {LockGuard lock(computers);\
        for (Computer * c : *computers) {if (*renderTarget == c->term/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {\
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
    pumpTaskQueue();
    if (ch == KEY_SLEFT) { previousRenderTarget(); CLITerminal::renderNavbar(""); } 
    else if (ch == KEY_SRIGHT) { nextRenderTarget(); CLITerminal::renderNavbar(""); } 
    else if (ch == KEY_MOUSE) {
        if (getmouse(&me) != OK) return false;
        if (me.y == LINES - 1) {
            if (me.bstate & BUTTON1_PRESSED) {
                if (me.x == COLS - 1) {
                    e.type = SDL_WINDOWEVENT;
                    e.window.event = SDL_WINDOWEVENT_CLOSE;
                    if (rawClient) {e.button.windowID = (*renderTarget)->id; sendRawEvent(e);}
                    else {
                        LockGuard lock(computers);
                        for (Computer * c : *computers) {
                            if (*renderTarget == c->term || findMonitorFromWindowID(c, (*renderTarget)->id, NULL) != NULL) {
                                std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
                                e.button.windowID = (*renderTarget)->id;
                                c->termEventQueue.push(e);
                                c->event_lock.notify_all();
                            }
                        }
                        for (Terminal * t : orphanedTerminals) {
                            if (t == *renderTarget) {
                                orphanedTerminals.erase(t);
                                t->factory->deleteTerminal(t);
                                break;
                            }
                        }
                    }
                } else if (me.x == COLS - 2) { nextRenderTarget(); CLITerminal::renderNavbar(""); } 
                else if (me.x == COLS - 3) { previousRenderTarget(); CLITerminal::renderNavbar(""); }
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
