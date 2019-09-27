/*
 * CLITerminalWindow.cpp
 * CraftOS-PC 2
 * 
 * This file implements the CLITerminalWindow class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#ifndef NO_CLI
#include "CLITerminalWindow.hpp"
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <ncurses.h>
#include <panel.h>

std::set<unsigned> currentIDs;
std::set<unsigned>::iterator CLITerminalWindow::selectedWindow = currentIDs.begin();

void CLITerminalWindow::renderNavbar(std::string title) {
    move(LINES-1, 0);
    attron(COLOR_PAIR(0x70));
    clrtoeol();
    printw("%d: %s", *selectedWindow+1, title.c_str());
    for (int i = getcurx(stdscr); i < COLS; i++) mvaddch(LINES-1, i, ' ' | COLOR_PAIR(0x70));
    attroff(COLOR_PAIR(0x70));
}

CLITerminalWindow::CLITerminalWindow(std::string title): title(title), TerminalWindow(COLS, LINES-1) {
    overridden = true;
    for (id = 0; currentIDs.find(id) != currentIDs.end(); id++);
    selectedWindow = currentIDs.insert(currentIDs.end(), id);
    last_pair = 0;
}

CLITerminalWindow::~CLITerminalWindow() {
    auto pos = currentIDs.find(id);
    auto next = currentIDs.erase(pos);
    if (next == currentIDs.end()) next--;
    selectedWindow = next;
}

bool CLITerminalWindow::drawChar(char c, int x, int y, Color fg, Color bg, bool transparent) {
    return false;
}

void CLITerminalWindow::render() {
    if (*selectedWindow == id) {
        std::lock_guard<std::mutex> locked_g(locked);
        move(0, 0);
        clear();
        if (can_change_color()) {
            unsigned short checksum = 0;
            for (int i = 0; i < 48; i++) {
                checksum = (checksum >> 1) + ((checksum & 1) << 15);
                checksum += ((unsigned char*)palette)[i];
            }
            if (checksum != lastPaletteChecksum)
                for (int i = 0; i < 16; i++) 
                    init_color(15-i, palette[i].r * (1000/255), palette[i].g * (1000/255), palette[i].b * (1000/255));
            lastPaletteChecksum = checksum;
        }
        for (int y = 0; y < screen.size(); y++) {
            for (int x = 0; x < screen[y].size(); x++) {
                move(y, x);
                addch(screen[y][x] | COLOR_PAIR(colors[y][x]));
            }
        }
        renderNavbar(title);
        move(blinkY, blinkX);
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
}

void CLITerminalWindow::previousWindow() {
    if (selectedWindow == currentIDs.begin()) selectedWindow = currentIDs.end();
    selectedWindow--;
}

void cliInit() {
    SDL_Init(SDL_INIT_AUDIO);
    initscr();
    keypad(stdscr, TRUE);
    noecho();
    cbreak();
    nodelay(stdscr, TRUE);
    mousemask(BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON2_PRESSED | BUTTON2_CLICKED | BUTTON2_RELEASED | BUTTON3_PRESSED | BUTTON3_CLICKED | BUTTON3_RELEASED | REPORT_MOUSE_POSITION, NULL);
    start_color();
    for (int i = 0; i < 256; i++) init_pair(i, 15 - (i & 0x0f), 15 - ((i >> 4) & 0xf));
}

void cliClose() {
    echo();
    nocbreak();
    nodelay(stdscr, FALSE);
    keypad(stdscr, FALSE);
    mousemask(0, NULL);
    endwin();
    SDL_Quit();
} 
#endif