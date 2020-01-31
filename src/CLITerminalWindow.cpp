/*
 * CLITerminalWindow.cpp
 * CraftOS-PC 2
 * 
 * This file implements the CLITerminalWindow class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef NO_CLI
#include "CLITerminalWindow.hpp"
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <ncurses.h>
#include <panel.h>

extern void termRenderLoop();
extern std::thread * renderThread;
extern std::unordered_map<int, unsigned char> keymap_cli;
std::set<unsigned> currentIDs;
std::set<unsigned>::iterator CLITerminalWindow::selectedWindow = currentIDs.begin();
bool CLITerminalWindow::stopRender = false;
bool CLITerminalWindow::forceRender = false;

void CLITerminalWindow::renderNavbar(std::string title) {
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
}

CLITerminalWindow::CLITerminalWindow(std::string title): TerminalWindow(COLS, LINES-1), title(title) {
    overridden = true;
    for (id = 0; currentIDs.find(id) != currentIDs.end(); id++);
    selectedWindow = currentIDs.insert(currentIDs.end(), id);
    last_pair = 0;
    renderTargets.push_back(this);
}

CLITerminalWindow::~CLITerminalWindow() {
    auto pos = currentIDs.find(id);
    auto next = currentIDs.erase(pos);
    if (currentIDs.size() == 0) return;
    if (next == currentIDs.end()) next--;
    selectedWindow = next;
}

bool CLITerminalWindow::drawChar(char c, int x, int y, Color fg, Color bg, bool transparent) {
    return false;
}

void CLITerminalWindow::render() {
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
        for (int y = 0; (unsigned)y < height; y++) {
            for (int x = 0; (unsigned)x < width; x++) {
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

void CLITerminalWindow::getMouse(int *x, int *y) {
    *x = -1;
    *y = -1;
}

void CLITerminalWindow::showMessage(Uint32 flags, const char * title, const char * message) {
    fprintf(stderr, "%s: %s\n", title, message);
}

void CLITerminalWindow::nextWindow() {
    if (++selectedWindow == currentIDs.end()) selectedWindow = currentIDs.begin();
    forceRender = true;
}

void CLITerminalWindow::previousWindow() {
    if (selectedWindow == currentIDs.begin()) selectedWindow = currentIDs.end();
    selectedWindow--;
    forceRender = true;
}

void cliInit() {
    SDL_Init(SDL_INIT_AUDIO);
    initscr();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();
    nodelay(stdscr, TRUE);
    mousemask(BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON2_PRESSED | BUTTON2_CLICKED | BUTTON2_RELEASED | BUTTON3_PRESSED | BUTTON3_CLICKED | BUTTON3_RELEASED | REPORT_MOUSE_POSITION, NULL);
    mouseinterval(0);
    start_color();
    for (int i = 0; i < 256; i++) init_pair(i, 15 - (i & 0x0f), 15 - ((i >> 4) & 0xf));
    renderThread = new std::thread(termRenderLoop);
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
}

void cliClose() {
    renderThread->join();
    delete renderThread;
    echo();
    nocbreak();
    nodelay(stdscr, FALSE);
    keypad(stdscr, FALSE);
    mousemask(0, NULL);
    endwin();
    SDL_Quit();
} 
#endif