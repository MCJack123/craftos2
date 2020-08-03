/*
 * Terminal.hpp
 * CraftOS-PC 2
 * 
 * This file defines the Terminal base class, which is implemented by all 
 * renderer classes.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

class Terminal;
#ifndef TERMINAL_TERMINAL_HPP
#define TERMINAL_TERMINAL_HPP

#include <cstdint>
#include <vector>
#include <string>
#include "../lib.hpp"

typedef struct color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color;

extern Color defaultPalette[16];

class window_exception: public std::exception {
    std::string err;
public:
    virtual const char * what() const throw() {return err.c_str();}
    window_exception(std::string error): err(error) {err = "Could not create new terminal: " + err;}
    window_exception(): err("Unknown error") {}
};
template<typename T>
class vector2d {
    int width;
    int height;
    std::vector<T> vec;
public:
    class row {
        std::vector<T> * vec;
        int ypos;
        int size;
        class val {
            std::vector<T> * vec;
            int pos;
        public:
            val(std::vector<T> *v, int p): vec(v), pos(p) {}
            operator T() {return (*vec)[pos];}
            T operator=(T val) {return (*vec)[pos] = val;}
            T* operator&() {return &(*vec)[pos];}
        };
    public:
        row(std::vector<T> *v, int y, int s): vec(v), ypos(y), size(s) {}
        val operator[](int idx) { 
            if (idx >= size) throw std::out_of_range("Vector2D index out of range");
            return val(vec, ypos + idx);
        }
        void operator=(std::vector<T> v) {std::copy(v.begin(), v.begin() + max((int)v.size(), size), vec->begin() + ypos);}
        void operator=(row v) {std::copy(v.vec->begin() + v.ypos, v.vec->begin() + v.ypos + v.size, vec->begin() + ypos);}
    };
    vector2d(int w, int h, T v): width(w), height(h), vec((size_t)w*h, v) {}
    row operator[](int idx) {
        if (idx >= height) throw std::out_of_range("Vector2D index out of range");
        return row(&vec, idx * width, width);
    }
    void resize(int w, int h, T v) {
        if (w == width) vec.resize(width * h);
        else {
            std::vector<T> newvec(w * h);
            for (int y = 0; y < height && y < h; y++) {
                std::copy(vec.begin() + (y * width), vec.begin() + (y * width) + min(w, width), newvec.begin() + (y * w));
                if (w > width) std::fill(newvec.begin() + (y * w) + width, newvec.begin() + ((y + 1) * w), v);
            }
            vec = newvec;
        }
        if (h > height) std::fill(vec.begin() + (w * height), vec.end(), v);
        width = w;
        height = h;
    }
};

class Terminal {
public:
    int type = -1;
    unsigned id = 0;
    int width;
    int height;
    static const int fontWidth = 6;
    static const int fontHeight = 9;
    bool changed = true;
    bool gotResizeEvent = false;
    int newWidth = 0, newHeight = 0;
    std::string title;
    static std::list<Terminal*> renderTargets;
    static std::mutex renderTargetsLock;
#ifdef __EMSCRIPTEN__
    static std::list<Terminal*>::iterator renderTarget;
#endif
    std::mutex locked;
    vector2d<unsigned char> screen;
    vector2d<unsigned char> colors;
    vector2d<unsigned char> pixels;
    volatile int mode = 0;
    Color palette[256];
    Color background = {0x1f, 0x1f, 0x1f};
    int blinkX = 0;
    int blinkY = 0;
    bool blink = true;
    bool canBlink = true;
    std::chrono::high_resolution_clock::time_point last_blink = std::chrono::high_resolution_clock::now();
    int framecount;
protected:
    Terminal(int w, int h): width(w), height(h), screen(w, h, ' '), colors(w, h, 0xF0), pixels(w*fontWidth, h*fontHeight, 0x0F) {
        memcpy(palette, defaultPalette, sizeof(defaultPalette));
    }
public:
    virtual ~Terminal(){}
    virtual void render()=0;
    virtual void showMessage(Uint32 flags, const char * title, const char * message)=0;
    virtual void setLabel(std::string label)=0;
    virtual bool resize(int w, int h)=0;
};

#endif