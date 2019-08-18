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

int main(int argc, char*argv[]) {
    termInit();
    config_init();
    startComputer(0);
    mainLoop();
    termClose();
    platformFree();
    config_save(true);
    return 0;
}
