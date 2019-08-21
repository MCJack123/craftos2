/*
 * main.cpp
 * CraftOS-PC 2
 * 
 * This file controls the Lua VM, loads the CraftOS BIOS, and sends events back.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "Computer.hpp"
#include "config.hpp"

extern void termInit();
extern void termClose();
extern void config_init();
extern void config_save(bool deinit);
extern void mainLoop();
extern std::list<void*> computerThreads;
extern bool exiting;
bool headless = false;
std::string script_file = "";

int main(int argc, char*argv[]) {
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--headless") headless = true;
        else if (std::string(argv[i]) == "--script") script_file = argv[++i];
    }
    termInit();
    config_init();
    startComputer(0);
    mainLoop();
    for (void* t : computerThreads) joinThread(t);
#ifndef _WIN32
    for (void* t : computerThreads) delete (pthread_t*)t;
#endif
    termClose();
    platformFree();
    config_save(true);
    return 0;
}
