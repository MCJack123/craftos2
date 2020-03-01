/*
 * CLITerminal.cpp
 * CraftOS-PC 2
 * 
 * This file implements the CLITerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#define CRAFTOSPC_INTERNAL
#ifndef NO_CLI
#include "CLITerminal.hpp"
#include "SDLTerminal.hpp"
#include "../peripheral/monitor.hpp"
#include <thread>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <ncurses.h>
#include <panel.h>
#include <signal.h>

extern void termRenderLoop();
extern std::thread * renderThread;
extern std::unordered_map<int, unsigned char> keymap_cli;
std::set<unsigned> CLITerminal::currentIDs;
std::set<unsigned>::iterator CLITerminal::selectedWindow = currentIDs.begin();
bool CLITerminal::stopRender = false;
bool CLITerminal::forceRender = false;

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
    Terminal::renderTargetsLock.lock();
    std::lock_guard<std::mutex> locked_g(locked);
    for (auto it = renderTargets.begin(); it != renderTargets.end(); it++) {
        if (*it == this)
            it = renderTargets.erase(it);
        if (it == renderTargets.end()) break;
    }
    Terminal::renderTargetsLock.unlock();
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
    if (*selectedWindow == id && changed) {
        changed = false;
        std::lock_guard<std::mutex> locked_g(locked);
        move(0, 0);
        if (stopRender) {stopRender = false; return;}
        clear();
        if (stopRender) {stopRender = false; return;}
        if (can_change_color()) {
            unsigned short checksum = 0;
            for (int i = 0; i < 48; i++) 
                checksum = (checksum >> 1) + ((checksum & 1) << 15) + ((unsigned char*)palette)[i];
            if (checksum != lastPaletteChecksum)
                for (int i = 0; i < 16; i++) 
                    init_color(15-i, palette[i].r * (1000/255), palette[i].g * (1000/255), palette[i].b * (1000/255));
            lastPaletteChecksum = checksum;
        }
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
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

void CLITerminal::showMessage(Uint32 flags, const char * title, const char * message) {
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

short original_colors[16][3];
MEVENT me;
WINDOW * tmpwin;
std::set<int> lastch;
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
		if (*CLITerminal::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
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
		if (*CLITerminal::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
			e.key.windowID = c->term->id;
			c->termEventQueue.push(e);
			e.type = SDL_KEYUP;
			e.key.keysym.scancode = (SDL_Scancode)56;
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
	tmpwin = newwin(0, 0, 1, 1);
	nodelay(tmpwin, TRUE);
	keypad(tmpwin, TRUE);
	signal(SIGWINCH, handle_winch);
	if (config.cliControlKeyMode == 3) {
		signal(SIGINT, pressControl);
		signal(SIGQUIT, pressAlt);
	}
    renderThread = new std::thread(termRenderLoop);
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

extern std::queue< std::tuple<int, std::function<void*(void*)>, void*, bool> > taskQueue;
extern std::unordered_map<int, void*> taskQueueReturns;
extern monitor * findMonitorFromWindowID(Computer *comp, unsigned id, std::string& sideReturn);

#ifdef __EMSCRIPTEN__
#define checkWindowID(c, wid) (c->term == *TerminalWindow::renderTarget || findMonitorFromWindowID(c, (*TerminalWindow::renderTarget)->id, tmps) != NULL)
#else
#define checkWindowID(c, wid) (wid == c->term->id || findMonitorFromWindowID(c, wid, tmps) != NULL)
#endif

bool CLITerminal::pollEvents() {
    SDL_Event e;
    std::string tmps;
	int ch = wgetch(tmpwin);
	if (ch == ERR) {
		for (int cc : lastch) {
			if (cc != 27) {
				e.type = SDL_KEYUP;
				e.key.keysym.scancode = (SDL_Scancode)(keymap_cli.find(cc) != keymap_cli.end() ? keymap_cli.at(cc) : cc);
				for (Computer * c : computers) {
					if (*CLITerminal::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
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
		CLITerminal::stopRender = true;
		delwin(tmpwin);
		endwin();
		refresh();
		tmpwin = newwin(0, 0, 1, 1);
		nodelay(tmpwin, TRUE);
		keypad(tmpwin, TRUE);
		e.type = SDL_WINDOWEVENT;
		e.window.event = SDL_WINDOWEVENT_RESIZED;
		for (Computer * c : computers) {
			e.window.data1 = COLS * (Terminal::fontWidth * 2 / SDLTerminal::fontScale * 2) + 4 * (2 / SDLTerminal::fontScale) * (Terminal::fontWidth * 2 / SDLTerminal::fontScale * 2);
			e.window.data2 = (LINES - 1) * (Terminal::fontHeight * 2 / SDLTerminal::fontScale * 2) + 4 * (2 / SDLTerminal::fontScale) * (Terminal::fontHeight * 2 / SDLTerminal::fontScale * 2);
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
	if (ch == KEY_SLEFT) { CLITerminal::previousWindow(); CLITerminal::renderNavbar(""); } 
    else if (ch == KEY_SRIGHT) { CLITerminal::nextWindow(); CLITerminal::renderNavbar(""); } 
    else if (ch == KEY_MOUSE) {
		if (getmouse(&me) != OK) return false;
		if (me.y == LINES - 1) {
			if (me.bstate & BUTTON1_PRESSED) {
				if (me.x == COLS - 1) {
					e.type = SDL_WINDOWEVENT;
					e.window.event = SDL_WINDOWEVENT_CLOSE;
					for (Computer * c : computers) {
						if (*CLITerminal::selectedWindow == c->term->id || findMonitorFromWindowID(c, *CLITerminal::selectedWindow, tmps) != NULL) {
							e.button.windowID = *CLITerminal::selectedWindow;
							c->termEventQueue.push(e);
							c->event_lock.notify_all();
						}
					}
				} else if (me.x == COLS - 2) { CLITerminal::nextWindow(); CLITerminal::renderNavbar(""); } else if (me.x == COLS - 3) { CLITerminal::previousWindow(); CLITerminal::renderNavbar(""); }
			}
			return false;
		}
		if (me.bstate & NCURSES_BUTTON_PRESSED) e.type = SDL_MOUSEBUTTONDOWN;
		else if (me.bstate & NCURSES_BUTTON_RELEASED) e.type = SDL_MOUSEBUTTONUP;
		else return false;
		if ((me.bstate & BUTTON1_PRESSED) || (me.bstate & BUTTON1_RELEASED)) e.button.button = SDL_BUTTON_LEFT;
		else if ((me.bstate & BUTTON2_PRESSED) || (me.bstate & BUTTON2_RELEASED)) e.button.button = SDL_BUTTON_RIGHT;
		else if ((me.bstate & BUTTON3_PRESSED) || (me.bstate & BUTTON3_RELEASED)) e.button.button = SDL_BUTTON_MIDDLE;
		else return false;
		e.button.x = me.x + 1;
		e.button.y = me.y + 1;
		for (Computer * c : computers) {
			if (*CLITerminal::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
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
				if (*CLITerminal::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
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
				if (*CLITerminal::selectedWindow == c->term->id/*|| findMonitorFromWindowID(c, e.text.windowID, tmps) != NULL*/) {
					e.key.windowID = c->term->id;
					c->termEventQueue.push(e);
					c->event_lock.notify_all();
				}
			}
		}
		lastch.insert(ch);
	}
    return false;
}
#endif