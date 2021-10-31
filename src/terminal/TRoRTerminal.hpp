/*
 * terminal/TRoRTerminal.hpp
 * CraftOS-PC 2
 * 
 * This file defines the TRoRTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#ifndef TERMINAL_TRORTERMINAL_HPP
#define TERMINAL_TRORTERMINAL_HPP
#include <set>
#include <Terminal.hpp>

class TRoRTerminal: public Terminal {
public:
    static void init();
    static void quit();
    static void showGlobalMessage(uint32_t flags, const char * title, const char * message);
    TRoRTerminal(std::string title);
    ~TRoRTerminal() override;
    void render() override {}
    void showMessage(uint32_t flags, const char * title, const char * message) override;
    void setLabel(std::string label) override;
    void onActivate() override {}
    bool resize(unsigned w, unsigned h) override {return false;}
};

#endif