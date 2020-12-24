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
#include <cstring>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

// A structure that represents one RGB color.
struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

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
    const char * what() const noexcept override {return err.c_str();}
    window_exception(const std::string& error): err(error) {err = "Could not create new terminal: " + err;}
    window_exception(): err("Unknown error") {}
};

// This will likely be going away soon ;)
template<typename T>
class vector2d {
    unsigned width;
    unsigned height;
    std::vector<T> vec;
public:
    class row {
        std::vector<T> * vec;
        unsigned ypos;
        unsigned size;
        class val {
            std::vector<T> * vec;
            unsigned pos;
        public:
            val(std::vector<T> *v, unsigned p): vec(v), pos(p) {}
            operator T() {return (*vec)[pos];}
            val& operator=(T val) {(*vec)[pos] = val; return *this;}
            T* operator&() {return &(*vec)[pos];}
        };
    public:
        row(std::vector<T> *v, unsigned y, unsigned s): vec(v), ypos(y), size(s) {}
        val operator[](unsigned idx) { 
            if (idx >= size) throw std::out_of_range("Vector2D index out of range");
            return val(vec, ypos + idx);
        }
        row& operator=(std::vector<T> v) {std::copy(v.begin(), v.begin() + (v.size() > size ? v.size() : size), vec->begin() + ypos); return *this;}
        row& operator=(row v) {std::copy(v.vec->begin() + v.ypos, v.vec->begin() + v.ypos + v.size, vec->begin() + ypos); return *this;}
    };
    vector2d(unsigned w, unsigned h, T v): width(w), height(h), vec((size_t)w*h, v) {}
    row operator[](unsigned idx) {
        if (idx >= height) throw std::out_of_range("Vector2D index out of range");
        return row(&vec, idx * width, width);
    }
    void resize(unsigned w, unsigned h, T v) {
        if (w == width) vec.resize(width * h);
        else {
            std::vector<T> newvec(w * h);
            for (unsigned y = 0; y < height && y < h; y++) {
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
    size_t dataSize() { return vec.size(); }
};

// The Terminal class is the base class for all renderers. It stores the basic info about all terminal objects, as well as its contents.
class Terminal {
public:
    int type = -1;
    unsigned id = 0; // The ID of the terminal
    unsigned width; // The width of the terminal in characters
    unsigned height; // The height of the terminal in characters
    static constexpr unsigned fontWidth = 6; // A constant storing the standard width of one character in pixels @1x
    static constexpr unsigned fontHeight = 9; // A constant storing the standard height of one character in pixels @1x
    bool changed = true; // Whether the terminal's data has been changed and needs to be redrawn - the renderer will not update your changes until you set this!
    bool gotResizeEvent = false; // Whether a resize event was sent and is awaiting processing
    unsigned newWidth = 0, newHeight = 0; // If a resize event was sent, these store the new size of the window
    std::string title; // The window's title
    std::mutex locked; // A mutex locking access to the terminal - lock this before making any changes!
    vector2d<unsigned char> screen; // The buffer storing the characters displayed on screen
    vector2d<unsigned char> colors; // The buffer storing the foreground and background colors for each character
    vector2d<unsigned char> pixels; // The on-screen buffer storing the values of all pixels in graphics mode
    vector2d<unsigned char> pixelBuffer; // The off-screen buffer storing the values of all pixels in graphics mode
    bool bufferPixels = false; // True if pixel updates should go to the pixelBuffer; false if they go straight to pixels
    int mode = 0; // The current mode of the screen: 0 = text, 1 = 16-color GFX, 2 = 256-color GFX
    Color palette[256]; // The color palette for the computer
    Color background = {0x1f, 0x1f, 0x1f}; // The color of the computer's background
    int blinkX = 0; // The X position of the cursor
    int blinkY = 0; // The Y position of the cursor
    bool blink = true; // Whether the cursor is currently drawn on-screen
    bool canBlink = true; // Whether the cursor should blink
    std::chrono::high_resolution_clock::time_point last_blink = std::chrono::high_resolution_clock::now(); // The time that the cursor last blinked
    int framecount = 0; // The number of frames that have been rendered
    int errorcount = 0; // The number of consecutive errors that have happened while rendering (used to detect crashes)
    bool grayscale = false; // Whether the terminal should display in grayscale
    bool errorMode = false; // For standards mode: whether the screen should stay on-screen after the computer terminates, for use with CC-style error screens
protected:
    // Initial constructor to fill the contents with their defaults for the specified width and height
    Terminal(unsigned w, unsigned h): width(w), height(h), screen(w, h, ' '), colors(w, h, 0xF0), pixels(w*fontWidth, h*fontHeight, 0x0F), pixelBuffer(w*fontWidth, h*fontHeight, 0x0F) {
        memcpy(palette, defaultPalette, sizeof(defaultPalette));
    }
    // Helper function to handle conversion to grayscale
    Color grayscalify(const Color& c) const {
        if (!grayscale) return c;
        const uint8_t avg = ((int)c.r + (int)c.g + (int)c.b) / 3;
        return {avg, avg, avg};
    }
public:
    virtual ~Terminal() = default;
    virtual void render()=0; // Called every render tick to update the window with the terminal's contents
    virtual void showMessage(uint32_t flags, const char * title, const char * message)=0; // Displays a message on screen outside the bounds of CC
    virtual void setLabel(std::string label)=0; // Sets the title of the window
    virtual bool resize(unsigned w, unsigned h)=0; // Safely sets the size of the window

    vector2d<unsigned char>* currentPixels() {
        if (bufferPixels) {
            return &pixelBuffer;
        }

        return &pixels;
    }
};

#endif
