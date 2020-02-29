/*
 * RawTerminal.hpp
 * CraftOS-PC 2
 * 
 * This file defines the RawTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include "Terminal.hpp"
#include <thread>

class RawTerminal: public Terminal {
public:
    static void init();
    static void quit();
    RawTerminal(int w, int h);
    ~RawTerminal() override;
    void render() override;
    void showMessage(uint32_t flags, const char * title, const char * message) override;
    void setLabel(std::string label) override;
};