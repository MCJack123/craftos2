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
#include <string>
struct configuration {
    bool http_enable;
    bool debug_enable;
    //String[] http_whitelist;
    //String[] http_blacklist;
    bool disable_lua51_features;
    std::string default_computer_settings;
    bool logErrors;
    bool showFPS;
    int computerSpaceLimit;
    int maximumFilesOpen;
    int abortTimeout;
    int maxNotesPerTick;
    int clockSpeed;
    bool ignoreHotkeys;
    bool checkUpdates;
    bool romReadOnly;
    std::string customFontPath;
    int customFontScale;
    int customCharScale;
    std::string skipUpdate;
};
struct computer_configuration {
    std::string label;
    bool isColor;
};
#include "lib.hpp"
extern library_t config_lib;
extern struct configuration config;
extern struct computer_configuration getComputerConfig(int id);
extern void setComputerConfig(int id, struct computer_configuration cfg);
#endif