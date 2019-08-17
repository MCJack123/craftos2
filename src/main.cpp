/*
 * main.cpp
 * CraftOS-PC 2
 * 
 * This file controls the Lua VM, loads the CraftOS BIOS, and sends events back.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include <lauxlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#ifdef WIN32
#include <SDL_main.h>
#pragma warning(1:4431)
#else
#include <SDL2/SDL_main.h>
#endif
#include "Computer.hpp"
#include "config.hpp"

extern void termInit();
extern void termClose();
extern void config_init();
extern void config_save(bool deinit);

int main(int argc, char*argv[]) {
    char * tmpdirname = (char*)malloc(strlen(getBasePath()) + 12);
    strcpy(tmpdirname, getBasePath());
#ifdef _WIN32
    strcat(tmpdirname, "\\computer\\0");
#else
    strcat(tmpdirname, "/computer/0");
#endif
    createDirectory(tmpdirname);
    free(tmpdirname);
    termInit();
    config_init();
    Computer * comp = new Computer(0);
    comp->run();
    delete comp;
    termClose();
    platformFree();
    config_save(true);
    return 0;
}
