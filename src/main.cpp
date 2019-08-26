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
#include "peripheral/drive.hpp"

extern void termInit();
extern void termClose();
extern void config_init();
extern void config_save(bool deinit);
extern void mainLoop();
extern void http_server_stop();
extern std::list<std::thread*> computerThreads;
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
    driveInit();
    startComputer(0);
    mainLoop();
    for (std::thread *t : computerThreads) { t->join(); delete t; }
    driveQuit();
    termClose();
    http_server_stop();
    platformFree();
    config_save(true);
    return 0;
}
