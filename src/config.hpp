/*
 * config.hpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the config API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef CONFIG_HPP
#define CONFIG_HPP
#define MOUNT_MODE_NONE      0
#define MOUNT_MODE_RO_STRICT 1
#define MOUNT_MODE_RO        2
#define MOUNT_MODE_RW        3
#include <string>
struct configuration {
    bool http_enable;
    bool debug_enable;
    int mount_mode;
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
    bool configReadOnly;
    bool vanilla;
    int initialComputer;
    int maxRecordingTime;
    int recordingFPS; // should be an even divisor of clockSpeed
    int cliControlKeyMode; // 0: home = ctrl, s+home = home; 1: home = home, s+home = ctrl; 2: home = home, esc-c = control; 3: home = home, ctrl-c = control
    bool showMountPrompt;
    int maxOpenPorts;
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