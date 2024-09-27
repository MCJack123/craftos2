/*
 * configuration.cpp
 * CraftOS-PC 2
 *
 * This file implements functions for interacting with the configuration.
 *
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#include <fstream>
#include <unordered_map>
#include <configuration.hpp>
#include "platform.hpp"
#include "runtime.hpp"
#include "terminal/SDLTerminal.hpp"
#include "terminal/RawTerminal.hpp"
#include "terminal/TRoRTerminal.hpp"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

struct configuration config;
int onboardingMode = 0;

#ifdef __EMSCRIPTEN__
extern "C" {extern void syncfs(); }
#endif

static void showMessage(const std::string& message) {
    if (selectedRenderer == 0 || selectedRenderer == 5) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str(), NULL);
    else if (selectedRenderer == 3) RawTerminal::showGlobalMessage(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str());
    else if (selectedRenderer == 4) TRoRTerminal::showGlobalMessage(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str());
    else fprintf(stderr, "%s\n", message.c_str());
}

struct computer_configuration getComputerConfig(int id) {
    struct computer_configuration cfg = {"", true, false, false, 0, 0};
    std::ifstream in(getBasePath() / "config" / (std::to_string(id) + ".json"));
    if (!in.is_open()) return cfg;
    if (in.peek() == std::ifstream::traits_type::eof()) { in.close(); return cfg; } // treat an empty file as if it didn't exist in the first place
    Value root;
    Poco::JSON::Object::Ptr p;
    try { p = root.parse(in); } catch (Poco::JSON::JSONException &e) {
        cfg.loadFailure = true;
        showMessage("An error occurred while parsing the per-computer configuration file for computer " + std::to_string(id) + ": " + e.message() + ". The current session's config will be reset to default, and any changes made will not be saved.");
        in.close();
        return cfg;
    } catch (Poco::Exception &e) {
        cfg.loadFailure = true;
        showMessage("An error occurred while parsing the per-computer configuration file for computer " + std::to_string(id) + ": " + e.message() + ". The current session's config will be reset to default, and any changes made will not be saved.");
        in.close();
        return cfg;
    }catch (std::exception &e) {
        cfg.loadFailure = true;
        showMessage("An error occurred while parsing the per-computer configuration file for computer " + std::to_string(id) + ": " + e.what() + ". The current session's config will be reset to default, and any changes made will not be saved.");
        in.close();
        return cfg;
    } catch (...) {
        cfg.loadFailure = true;
        showMessage("An error occurred while parsing the per-computer configuration file for computer " + std::to_string(id) + ": unknown. The current session's config will be reset to default, and any changes made will not be saved.");
        in.close();
        return cfg;
    }
    in.close();
    if (root.isMember("isColor")) cfg.isColor = root["isColor"].asBool();
    if (root.isMember("label")) {
        if (root.isMember("base64")) cfg.label = b64decode(root["label"].asString());
        else cfg.label = std::string(root["label"].asString());
    }
#if !defined(__IPHONEOS__)
    if (root.isMember("startFullscreen")) cfg.startFullscreen = root["startFullscreen"].asBool();
#endif
    if (root.isMember("computerWidth")) cfg.computerWidth = root["computerWidth"].asInt();
    if (root.isMember("computerHeight")) cfg.computerHeight = root["computerHeight"].asInt();
    return cfg;
}

void setComputerConfig(int id, const computer_configuration& cfg) {
    if (cfg.loadFailure) return;
    Value root;
    if (!cfg.label.empty()) root["label"] = b64encode(cfg.label);
    root["isColor"] = cfg.isColor;
    root["base64"] = true;
    root["startFullscreen"] = cfg.startFullscreen;
    root["computerWidth"] = cfg.computerWidth;
    root["computerHeight"] = cfg.computerHeight;
    std::ofstream out(getBasePath() / "config" / (std::to_string(id) + ".json"));
    out << root;
    out.close();
#ifdef __EMSCRIPTEN__
    queueTask([](void*)->void* {syncfs(); return NULL; }, NULL, true);
#endif
}

#define readConfigSetting(name, type) if (root.isMember(#name)) config.name = root[#name].as##type()

bool configLoadError = false;

// first: 0 = immediate, 1 = reboot, 2 = relaunch
// second: 0 = boolean, 1 = number, 2 = string, 3 = string array
std::unordered_map<std::string, std::pair<int, int> > configSettings = {
    {"http_enable", {1, 0}},
    {"mount_mode", {0, 1}},
    {"http_whitelist", {0, 3}},
    {"http_blacklist", {0, 3}},
    {"disable_lua51_features", {1, 0}},
    {"default_computer_settings", {1, 2}},
    {"logErrors", {0, 0}},
    {"showFPS", {0, 0}},
    {"computerSpaceLimit", {0, 1}},
    {"maximumFilesOpen", {0, 1}},
    {"abortTimeout", {0, 1}},
    {"maxNotesPerTick", {2, 1}},
    {"clockSpeed", {0, 1}},
    {"ignoreHotkeys", {0, 0}},
    {"checkUpdates", {2, 0}},
    {"useHDFont", {2, 0}},
    {"configReadOnly", {0, 0}},
    {"vanilla", {1, 0}},
    {"initialComputer", {2, 1}},
    {"maxRecordingTime", {0, 1}},
    {"recordingFPS", {0, 1}},
    {"showMountPrompt", {0, 0}},
    {"maxOpenPorts", {0, 1}},
    {"mouse_move_throttle", {0, 1}},
    {"monitorsUseMouseEvents", {0, 0}},
    {"defaultWidth", {2, 1}},
    {"defaultHeight", {2, 1}},
    {"standardsMode", {0, 0}},
    {"isColor", {0, 0}},
    {"startFullscreen", {2, 0}},
    {"useHardwareRenderer", {2, 0}},
    {"preferredHardwareDriver", {2, 2}},
    {"useVsync", {2, 0}},
    {"http_websocket_enabled", {1, 0}},
    {"http_max_websockets", {0, 1}},
    {"http_max_websocket_message", {0, 1}},
    {"http_max_requests", {0, 1}},
    {"http_max_upload", {0, 1}},
    {"http_max_download", {0, 1}},
    {"http_timeout", {0, 1}},
    {"extendMargins", {0, 0}},
    {"snapToSize", {0, 0}},
    {"snooperEnabled", {2, 0}},
    {"computerWidth", {2, 1}},
    {"computerHeight", {2, 1}},
    {"keepOpenOnShutdown", {0, 0}},
    {"useWebP", {0, 0}},
    {"dropFilePath", {0, 0}},
    {"useDFPWM", {0, 0}},
};

const std::string hiddenOptions[] = {"customFontPath", "customFontScale", "customCharScale", "skipUpdate", "lastVersion", "pluginData", "http_proxy_server", "http_proxy_port", "cliControlKeyMode", "serverMode", "romReadOnly"};

std::unordered_map<std::string, Poco::Dynamic::Var> unknownOptions;

void config_init() {
    std::error_code e;
    fs::create_directories(getBasePath() / "config", e);
    config = {
        true,
        true,
        MOUNT_MODE_RO_STRICT,
        {"*"},
        {},
        {
            "C:\\Users\\*",
            "/Users/*",
            "/home/*"
        },
        {
            "*"
        },
        {},
        false,
        "",
        false,
        false,
        1000000,
        128,
        17000,
        8,
        20,
        false,
        true,
        true,
        "",
        0,
        0,
        "",
        false,
        false,
        0,
        15,
        10,
        0,
        true,
        128,
        -1,
        false,
        51,
        19,
        false,
#ifdef __EMSCRIPTEN__
        EM_ASM_INT(return navigator.platform == "MacIntel" ? 1 : 0) != 0, // will Apple decide to use "MacARM" in the future? if so, this will break
#else
        false,
#endif
        "",
        false,
        false,
        false,
        {},
        true,
        4,
        131072,
        16,
        4194304,
        16777216,
        30000,
        "",
        0,
        false,
        true,
        false,
#if defined(__IPHONEOS__) || defined(__ANDROID__)
        true,
#else
        false,
#endif
        true,
        false,
        false
    };
    if (e) {
        configLoadError = true;
        showMessage("An error occurred while parsing the global configuration file: Could not create config directory: " + std::string(e.message()) + ". The current session's config will be reset to default, and any changes made will not be saved.");
        return;
    }
    std::ifstream in(getBasePath() / "config" / "global.json");
    if (!in.is_open()) { onboardingMode = 1; return; }
    Value root;
    Poco::JSON::Object::Ptr p;
    try {
        p = root.parse(in);
    } catch (Poco::JSON::JSONException &e) {
        configLoadError = true;
        showMessage("An error occurred while parsing the global configuration file: " + e.message() + ". The current session's config will be reset to default, and any changes made will not be saved.");
        in.close();
        return;
    } catch (Poco::Exception &e) {
        configLoadError = true;
        showMessage("An error occurred while parsing the global configuration file: " + e.message() + ". The current session's config will be reset to default, and any changes made will not be saved.");
        in.close();
        return;
    } catch (std::exception &e) {
        configLoadError = true;
        showMessage("An error occurred while parsing the global configuration file: " + std::string(e.what()) + ". The current session's config will be reset to default, and any changes made will not be saved.");
        in.close();
        return;
    } catch (...) {
        configLoadError = true;
        showMessage("An error occurred while parsing the global configuration file: unknown. The current session's config will be reset to default, and any changes made will not be saved.");
        in.close();
        return;
    }
    in.close();
    try {
        readConfigSetting(http_enable, Bool);
        readConfigSetting(mount_mode, Int);
        if (root.isMember("http_whitelist")) {
            config.http_whitelist.clear();
            for (auto it = root["http_whitelist"].arrayBegin(); it != root["http_whitelist"].arrayEnd(); ++it)
                config.http_whitelist.push_back(it->toString());
        }
        if (root.isMember("http_blacklist")) {
            config.http_blacklist.clear();
            for (auto it = root["http_blacklist"].arrayBegin(); it != root["http_blacklist"].arrayEnd(); ++it)
                config.http_blacklist.push_back(it->toString());
        }
        if (root.isMember("mounter_whitelist")) {
            config.mounter_whitelist.clear();
            for (auto it = root["mounter_whitelist"].arrayBegin(); it != root["mounter_whitelist"].arrayEnd(); ++it)
                config.mounter_whitelist.push_back(it->toString());
        }
        if (root.isMember("mounter_blacklist")) {
            config.mounter_blacklist.clear();
            for (auto it = root["mounter_blacklist"].arrayBegin(); it != root["mounter_blacklist"].arrayEnd(); ++it)
                config.mounter_blacklist.push_back(it->toString());
        }
        if (root.isMember("mounter_no_ask")) {
            config.mounter_no_ask.clear();
            for (auto it = root["mounter_no_ask"].arrayBegin(); it != root["mounter_no_ask"].arrayEnd(); ++it)
                config.mounter_no_ask.push_back(it->toString());
        }
        readConfigSetting(disable_lua51_features, Bool);
        readConfigSetting(default_computer_settings, String);
        readConfigSetting(logErrors, Bool);
        readConfigSetting(showFPS, Bool);
        readConfigSetting(computerSpaceLimit, Int);
        readConfigSetting(maximumFilesOpen, Int);
        readConfigSetting(abortTimeout, Int);
        readConfigSetting(maxNotesPerTick, Int);
        readConfigSetting(clockSpeed, Int);
        readConfigSetting(ignoreHotkeys, Bool);
        readConfigSetting(checkUpdates, Bool);
        readConfigSetting(romReadOnly, Bool);
        readConfigSetting(customFontPath, String);
        readConfigSetting(customFontScale, Int);
        readConfigSetting(customCharScale, Int);
        readConfigSetting(skipUpdate, String);
        readConfigSetting(configReadOnly, Bool);
        readConfigSetting(vanilla, Bool);
        readConfigSetting(initialComputer, Int);
        readConfigSetting(maxRecordingTime, Int);
        readConfigSetting(recordingFPS, Int);
        readConfigSetting(cliControlKeyMode, Int);
        readConfigSetting(showMountPrompt, Bool);
        readConfigSetting(maxOpenPorts, Int);
        readConfigSetting(mouse_move_throttle, Int);
        readConfigSetting(monitorsUseMouseEvents, Bool);
        readConfigSetting(defaultWidth, Int);
        readConfigSetting(defaultHeight, Int);
        readConfigSetting(standardsMode, Bool);
#if !(defined(__IPHONEOS__) || defined(__ANDROID__))
        readConfigSetting(useHardwareRenderer, Bool);
#endif
        readConfigSetting(preferredHardwareDriver, String);
        readConfigSetting(useVsync, Bool);
        readConfigSetting(serverMode, Bool);
        readConfigSetting(http_websocket_enabled, Bool);
        readConfigSetting(http_max_websockets, Int);
        readConfigSetting(http_max_websocket_message, Int);
        readConfigSetting(http_max_requests, Int);
        readConfigSetting(http_max_upload, Int);
        readConfigSetting(http_max_download, Int);
        readConfigSetting(http_timeout, Int);
        readConfigSetting(http_proxy_server, String);
        readConfigSetting(http_proxy_port, Int);
        readConfigSetting(extendMargins, Bool);
        readConfigSetting(snapToSize, Bool);
        readConfigSetting(snooperEnabled, Bool);
#if !(defined(__IPHONEOS__) || defined(__ANDROID__))
        readConfigSetting(keepOpenOnShutdown, Bool);
#endif
        readConfigSetting(useWebP, Bool);
        readConfigSetting(dropFilePath, Bool);
        readConfigSetting(useDFPWM, Bool);
        // for JIT: substr until the position of the first '-' in CRAFTOSPC_VERSION (todo: find a static way to determine this)
        if (onboardingMode == 0 && (!root.isMember("lastVersion") || root["lastVersion"].asString().substr(0, sizeof(CRAFTOSPC_VERSION) - 1) != CRAFTOSPC_VERSION)) { onboardingMode = 2; config_save(); }
#ifndef __EMSCRIPTEN__
        if (root.isMember("pluginData")) for (const auto& e : root["pluginData"]) config.pluginData[e.first] = e.second.extract<std::string>();
        for (const auto& e : root)
            if (configSettings.find(e.first) == configSettings.end() && std::find(hiddenOptions, hiddenOptions + (sizeof(hiddenOptions) / sizeof(std::string)), e.first) == hiddenOptions + (sizeof(hiddenOptions) / sizeof(std::string)))
                unknownOptions.insert(e);
#endif
    } catch (Poco::Exception &e) {
        configLoadError = true;
        showMessage("An error occurred while reading the global configuration file: " + e.message() + ". The current session's config will be partially loaded, and any changes made will not be saved.");
        in.close();
        return;
    } catch (std::exception &e) {
        configLoadError = true;
        showMessage("An error occurred while reading the global configuration file: " + std::string(e.what()) + ". The current session's config will be partially loaded, and any changes made will not be saved.");
        in.close();
        return;
    } catch (...) {
        configLoadError = true;
        showMessage("An error occurred while reading the global configuration file: unknown. The current session's config will be partially loaded, and any changes made will not be saved.");
        in.close();
        return;
    }
}

void config_save() {
    if (configLoadError) return;
    Value root;
    root["http_enable"] = config.http_enable;
    root["mount_mode"] = config.mount_mode;
    root["http_whitelist"] = config.http_whitelist;
    root["http_blacklist"] = config.http_blacklist;
    root["mounter_whitelist"] = config.mounter_whitelist;
    root["mounter_blacklist"] = config.mounter_blacklist;
    root["mounter_no_ask"] = config.mounter_no_ask;
    root["disable_lua51_features"] = config.disable_lua51_features;
    root["default_computer_settings"] = config.default_computer_settings;
    root["logErrors"] = config.logErrors;
    root["showFPS"] = config.showFPS;
    root["computerSpaceLimit"] = config.computerSpaceLimit;
    root["maximumFilesOpen"] = config.maximumFilesOpen;
    root["abortTimeout"] = config.abortTimeout;
    root["maxNotesPerTick"] = config.maxNotesPerTick;
    root["clockSpeed"] = config.clockSpeed;
    root["ignoreHotkeys"] = config.ignoreHotkeys;
    root["checkUpdates"] = config.checkUpdates;
    root["romReadOnly"] = config.romReadOnly;
    root["customFontPath"] = config.customFontPath;
    root["customFontScale"] = config.customFontScale;
    root["customCharScale"] = config.customCharScale;
    root["skipUpdate"] = config.skipUpdate;
    root["configReadOnly"] = config.configReadOnly;
    root["vanilla"] = config.vanilla;
    root["initialComputer"] = config.initialComputer;
    root["maxRecordingTime"] = config.maxRecordingTime;
    root["recordingFPS"] = config.recordingFPS;
    root["cliControlKeyMode"] = config.cliControlKeyMode;
    root["showMountPrompt"] = config.showMountPrompt;
    root["maxOpenPorts"] = config.maxOpenPorts;
    root["mouse_move_throttle"] = config.mouse_move_throttle;
    root["monitorsUseMouseEvents"] = config.monitorsUseMouseEvents;
    root["defaultWidth"] = config.defaultWidth;
    root["defaultHeight"] = config.defaultHeight;
    root["standardsMode"] = config.standardsMode;
    root["useHardwareRenderer"] = config.useHardwareRenderer;
    root["preferredHardwareDriver"] = config.preferredHardwareDriver;
    root["useVsync"] = config.useVsync;
    root["serverMode"] = config.serverMode;
    root["http_websocket_enabled"] = config.http_websocket_enabled;
    root["http_max_websockets"] = config.http_max_websockets;
    root["http_max_websocket_message"] = config.http_max_websocket_message;
    root["http_max_requests"] = config.http_max_requests;
    root["http_max_upload"] = config.http_max_upload;
    root["http_max_download"] = config.http_max_download;
    root["http_timeout"] = config.http_timeout;
    root["http_proxy_server"] = config.http_proxy_server;
    root["http_proxy_port"] = config.http_proxy_port;
    root["extendMargins"] = config.extendMargins;
    root["snapToSize"] = config.snapToSize;
    root["snooperEnabled"] = config.snooperEnabled;
    root["keepOpenOnShutdown"] = config.keepOpenOnShutdown;
    root["useWebP"] = config.useWebP;
    root["dropFilePath"] = config.dropFilePath;
    root["useDFPWM"] = config.useDFPWM;
    root["lastVersion"] = CRAFTOSPC_VERSION;
    Value pluginRoot;
    for (const auto& e : config.pluginData) pluginRoot[e.first] = e.second;
    root["pluginData"] = pluginRoot;
    for (const auto& opt : unknownOptions) root[opt.first] = opt.second;
    std::ofstream out(getBasePath() / "config"/"global.json");
    if (out.is_open()) {
        out << root;
        out.close();
#ifdef __EMSCRIPTEN__
        queueTask([](void*)->void* {syncfs(); return NULL; }, NULL, true);
#endif
    } else {
        showMessage("An error occurred while writing the global configuration file. The current session's config will not be saved.");
    }
}
