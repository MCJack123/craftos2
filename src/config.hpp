/*
 * config.hpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the config API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#ifndef CONFIG_HPP
#define CONFIG_HPP
#include "lib.hpp"
extern library_t config_lib;
struct configuration {
    bool http_enable;
    bool debug_enable;
    //String[] http_whitelist;
    //String[] http_blacklist;
    bool disable_lua51_features;
    char * default_computer_settings;
    bool logPeripheralErrors;
    bool showFPS;
    bool readFail;
    int computerSpaceLimit;
    int maximumFilesOpen;
    int abortTimeout;
    int maxNotesPerTick;
    int clockSpeed;
    bool ignoreHotkeys;
};
struct computer_configuration {
    char * label;
    bool isColor;
};
extern struct configuration config;
extern struct computer_configuration getComputerConfig(int id);
extern void setComputerConfig(int id, struct computer_configuration cfg);
extern void freeComputerConfig(struct computer_configuration cfg);
#endif