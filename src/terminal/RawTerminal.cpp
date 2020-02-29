/*
 * RawTerminal.cpp
 * CraftOS-PC 2
 * 
 * This file implements the RawTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include "RawTerminal.hpp"
#include "../lib.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

/* Data spec:
Offset     Bytes      Purpose
0x00       4          Header ("!CPC")
0x04       4          Size (hex string)
===================== Base64 payload
0x00       2          Width
0x02       2          Height
0x04       1          ID
0x05       1          Graphics mode
0x06       1          Cursor showing?
0x07       1          Reserved
0x08       2          Cursor X
0x0A       2          Cursor Y
===================== Screen data
--------------------- Text mode (mode 0)
0x10       *x*        RLE-encoded text (length of expanded RLE = width * height)
0x10 + x   *y*        RLE-encoded background pairs (high nybble = BG, low nybble = FG)
--------------------- Graphics modes (modes 1/2)
0x10       *x*        RLE-encoded pixel data (length of expanded RLE = width * height * 54)
===================== End screen data
===================== Palette
--------------------- Text mode / 16 color GFX mode (modes 0/1)
0x10+x<+y> 48         RGB palette x16
--------------------- 256 color GFX mode (mode 2)
0x10+x     768        RGB palette x256
===================== End palette
===================== End Base64 payload
END        1          Newline
*/

extern bool rawClient;
extern std::thread * renderThread;
extern void termRenderLoop();

void RawTerminal::init() {
    if (!rawClient) renderThread = new std::thread(termRenderLoop);
}

void RawTerminal::quit() {
    if (!rawClient) {
        renderThread->join();
        delete renderThread;
    }
}

RawTerminal::RawTerminal(int w, int h): Terminal(w, h) {
    renderTargets.push_back(this);
}

RawTerminal::~RawTerminal() {
    Terminal::renderTargetsLock.lock();
    std::lock_guard<std::mutex> locked_g(locked);
    for (auto it = renderTargets.begin(); it != renderTargets.end(); it++) {
        if (*it == this)
            it = renderTargets.erase(it);
        if (it == renderTargets.end()) break;
    }
    Terminal::renderTargetsLock.unlock();
}

void RawTerminal::render() {
    if (!changed) return;
    changed = false;
    std::stringstream output;
    output.write((char*)&width, 2);
    output.write((char*)&height, 2);
    output.put((char)id);
    output.put((char)mode);
    output.put((char)blink);
    output.put(0);
    output.write((char*)&blinkX, 2);
    output.write((char*)&blinkY, 2);
    if (mode == 0) {
        unsigned char c = screen[0][0];
        unsigned char n = 0;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (screen[y][x] != c || n == 255) {
                    output.put(c);
                    output.put(n);
                    c = screen[y][x];
                    n = 0;
                }
                n++;
            }
        }
        if (n > 0) {
            output.put(c);
            output.put(n);
        }
        c = colors[0][0];
        n = 0;
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (colors[y][x] != c || n == 255) {
                    output.put(c);
                    output.put(n);
                    c = colors[y][x];
                    n = 0;
                }
                n++;
            }
        }
        if (n > 0) {
            output.put(c);
            output.put(n);
        }
    } else {
        unsigned char c = pixels[0][0];
        unsigned char n = 0;
        for (int y = 0; y < height*9; y++) {
            for (int x = 0; x < width*6; x++) {
                if (pixels[y][x] != c || n == 255) {
                    output.put(c);
                    output.put(n);
                    c = pixels[y][x];
                    n = 0;
                }
                n++;
            }
        }
        if (n > 0) {
            output.put(c);
            output.put(n);
        }
    }
    for (int i = 0; i < (mode == 2 ? 256 : 16); i++) {
        output.put(palette[i].r);
        output.put(palette[i].g);
        output.put(palette[i].b);
    }
    std::string str = b64encode(output.str());
    str.erase(std::remove_if(str.begin(), str.end(), [](char c)->bool{return c == '\n' || c == '\r';}), str.end());
    std::cout << "!CPC" << std::hex << std::setfill('0') << std::setw(4) << str.length() << std::dec;
    std::cout << str << "\n";
    std::cout.flush();
}

void RawTerminal::showMessage(uint32_t flags, const char * title, const char * message) {
    std::cerr << title << ": " << message << "\n";
}

void RawTerminal::setLabel(std::string label) {
    // ?
}