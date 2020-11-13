/*
 * configuration.hpp
 * CraftOS-PC 2
 *
 * This file defines structures used for storing the configuration.
 *
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef CRAFTOS_PC_CONFIGURATION_HPP
#define CRAFTOS_PC_CONFIGURATION_HPP

#include <string>
#include <vector>

// Constants for mount modes
#define MOUNT_MODE_NONE      0 // Disallow all mounting
#define MOUNT_MODE_RO_STRICT 1 // Only allow read-only mounts
#define MOUNT_MODE_RO        2 // Default to read-only mounts
#define MOUNT_MODE_RW        3 // Default to read-write mounts

// This structure holds all available configuration variables. See https://www.craftos-pc.cc/docs/config for information about what each of these means.
struct configuration {
    bool http_enable;
    bool debug_enable;
    int mount_mode;
    std::vector<std::string> http_whitelist;
    std::vector<std::string> http_blacklist;
    std::vector<std::string> mounter_whitelist;
    std::vector<std::string> mounter_blacklist;
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
    int mouse_move_throttle;
    bool monitorsUseMouseEvents;
    int defaultWidth;
    int defaultHeight;
    bool standardsMode;
    bool useHardwareRenderer;
    std::string preferredHardwareDriver;
    bool useVsync;
    bool jit_ffi_enable;
};

// A smaller structure that holds the configuration for a single computer.
struct computer_configuration {
    std::string label;
    bool isColor;
    bool loadFailure;
    bool startFullscreen;
};

#endif