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

#ifndef CRAFTOS_PC_TERMINAL_HPP
#define CRAFTOS_PC_TERMINAL_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <chrono>
#include <mutex>

// A structure that represents one RGB color.
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color;

// A list of the default ComputerCraft colors.
static Color defaultPalette[16] = {
    {0xf0, 0xf0, 0xf0},
    {0xf2, 0xb2, 0x33},
    {0xe5, 0x7f, 0xd8},
    {0x99, 0xb2, 0xf2},
    {0xde, 0xde, 0x6c},
    {0x7f, 0xcc, 0x19},
    {0xf2, 0xb2, 0xcc},
    {0x4c, 0x4c, 0x4c},
    {0x99, 0x99, 0x99},
    {0x4c, 0x99, 0xb2},
    {0xb2, 0x66, 0xe5},
    {0x33, 0x66, 0xcc},
    {0x7f, 0x66, 0x4c},
    {0x57, 0xa6, 0x4e},
    {0xcc, 0x4c, 0x4c},
    {0x11, 0x11, 0x11}
};

// An exception type that is thrown if a terminal fails to initialize.
class window_exception: public std::exception {
    std::string err;
public:
    virtual const char * what() const throw() {return err.c_str();}
    window_exception(std::string error): err(error) {err = "Could not create new terminal: " + err;}
    window_exception(): err("Unknown error") {}
};

// This will likely be going away soon ;)
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
        void operator=(std::vector<T> v) {std::copy(v.begin(), v.begin() + (v.size() > size ? v.size() : size), vec->begin() + ypos);}
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
                std::copy(vec.begin() + (y * width), vec.begin() + (y * width) + (w < width ? w : width), newvec.begin() + (y * w));
                if (w > width) std::fill(newvec.begin() + (y * w) + width, newvec.begin() + ((y + 1) * w), v);
            }
            vec = newvec;
        }
        if (h > height) std::fill(vec.begin() + (w * height), vec.end(), v);
        width = w;
        height = h;
    }
    T* data() { return vec.data(); }
};

// The Terminal class is the base class for all renderers. It stores the basic info about all terminal objects, as well as its contents.
class Terminal {
public:
    int type = -1;
    unsigned id = 0; // The ID of the terminal
    unsigned width; // The width of the terminal in characters
    unsigned height; // The height of the terminal in characters
    static const int fontWidth = 6; // A constant storing the standard width of one character in pixels @1x
    static const int fontHeight = 9; // A constant storing the standard height of one character in pixels @1x
    bool changed = true; // Whether the terminal's data has been changed and needs to be redrawn - the renderer will not update your changes until you set this!
    bool gotResizeEvent = false; // Whether a resize event was sent and is awaiting processing
    int newWidth = 0, newHeight = 0; // If a resize event was sent, these store the new size of the window
    std::string title; // The window's title
    std::mutex locked; // A mutex locking access to the terminal - lock this before making any changes!
    vector2d<unsigned char> screen; // The buffer storing the characters displayed on screen
    vector2d<unsigned char> colors; // The buffer storing the foreground and background colors for each character
    vector2d<unsigned char> pixels; // The buffer storing the values of all pixels in graphics mode
    int mode = 0; // The current mode of the screen: 0 = text, 1 = 16-color GFX, 2 = 256-color GFX
    Color palette[256]; // The color palette for the computer
    Color background = {0x1f, 0x1f, 0x1f}; // The color of the computer's background
    int blinkX = 0; // The X position of the cursor
    int blinkY = 0; // The Y position of the cursor
    bool blink = true; // Whether the cursor is currently drawn on-screen
    bool canBlink = true; // Whether the cursor should blink
    std::chrono::high_resolution_clock::time_point last_blink = std::chrono::high_resolution_clock::now(); // The time that the cursor last blinked
    int framecount; // The number of frames that have been rendered
    int errorcount = 0; // The number of consecutive errors that have happened while rendering (used to detect crashes)
    bool grayscale = false; // Whether the terminal should display in grayscale
    bool errorMode = false; // For standards mode: whether the screen should stay on-screen after the computer terminates, for use with CC-style error screens
protected:
    // Initial constructor to fill the contents with their defaults for the specified width and height
    Terminal(int w, int h): width(w), height(h), screen(w, h, ' '), colors(w, h, 0xF0), pixels(w*fontWidth, h*fontHeight, 0x0F) {
        memcpy(palette, defaultPalette, sizeof(defaultPalette));
    }
    // Helper function to handle conversion to grayscale
    inline Color grayscalify(Color c) {
        if (!grayscale) return c;
        uint8_t avg = ((int)c.r + (int)c.g + (int)c.b) / 3;
        return {avg, avg, avg};
    }
public:
    virtual ~Terminal(){}
    virtual void render()=0; // Called every render tick to update the window with the terminal's contents
    virtual void showMessage(uint32_t flags, const char * title, const char * message)=0; // Displays a message on screen outside the bounds of CC
    virtual void setLabel(std::string label)=0; // Sets the title of the window
    virtual bool resize(int w, int h)=0; // Safely sets the size of the window
};

#endif