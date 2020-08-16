/*
 * config.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the config API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#define CRAFTOSPC_INTERNAL
#include "config.hpp"
#include "platform.hpp"
#include "os.hpp"
#include "terminal/SDLTerminal.hpp"
#include "terminal/RawTerminal.hpp"
#include "terminal/TRoRTerminal.hpp"
#include <string.h>
#include <fstream>
#include <string>
#include <unordered_map>
#include <Poco/Base64Encoder.h>
#include <Poco/Base64Decoder.h>

struct configuration config;
extern int selectedRenderer;
int onboardingMode = 0;

struct computer_configuration getComputerConfig(int id) {
    struct computer_configuration cfg = {"", true, false, false};
    std::ifstream in(std::string(getBasePath()) + "/config/" + std::to_string(id) + ".json");
    if (!in.is_open()) return cfg; 
    if (in.peek() == std::ifstream::traits_type::eof()) {in.close(); return cfg;} // treat an empty file as if it didn't exist in the first place
    Value root;
    Poco::JSON::Object::Ptr p;
    try {p = root.parse(in);} catch (Poco::JSON::JSONException &e) {
        cfg.loadFailure = true;
        std::string message = "An error occurred while parsing the per-computer configuration file for computer " + std::to_string(id) + ": " + e.message() + ". The current session's config will be reset to default, and any changes made will not be saved.";
        if (selectedRenderer == 0 || selectedRenderer == 5) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str(), NULL);
        else if (selectedRenderer == 3) RawTerminal::showGlobalMessage(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str());
        else if (selectedRenderer == 4) TRoRTerminal::showGlobalMessage(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str());
        else printf("%s\n", message.c_str());
        in.close();
        return cfg;
    }
    in.close();
    cfg.isColor = root["isColor"].asBool();
    if (root.isMember("label")) {
        if (root.isMember("base64")) cfg.label = b64decode(root["label"].asString());
        else cfg.label = std::string(root["label"].asString());
    }
    if (root.isMember("startFullscreen")) cfg.startFullscreen = root["startFullscreen"].asBool();
    return cfg;
}

void setComputerConfig(int id, struct computer_configuration cfg) {
    if (cfg.loadFailure) return;
    Value root;
    if (!cfg.label.empty()) root["label"] = b64encode(cfg.label);
    root["isColor"] = cfg.isColor;
    root["base64"] = true;
    root["startFullscreen"] = cfg.startFullscreen;
    std::ofstream out(std::string(getBasePath()) + "/config/" + std::to_string(id) + ".json");
    out << root;
    out.close();
}

#define readConfigSetting(name, type) if (root.isMember(#name)) config.name = root[#name].as##type()

bool configLoadError = false;

void config_save();

void config_init() {
    createDirectory((std::string(getBasePath()) + "/config").c_str());
    config = {
        true,
        false,
        MOUNT_MODE_RO_STRICT,
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
        false,
        "",
        false
    };
    std::ifstream in(std::string(getBasePath()) + "/config/global.json");
    if (!in.is_open()) { onboardingMode = 1;  return; }
    Value root;
    Poco::JSON::Object::Ptr p;
    try {
        p = root.parse(in);
    } catch (Poco::JSON::JSONException &e) {
        configLoadError = true;
        std::string message = "An error occurred while parsing the global configuration file: " + e.message() + ". The current session's config will be reset to default, and any changes made will not be saved.";
        if (selectedRenderer == 0 || selectedRenderer == 5) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str(), NULL);
        else if (selectedRenderer == 3) RawTerminal::showGlobalMessage(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str());
        else if (selectedRenderer == 4) TRoRTerminal::showGlobalMessage(SDL_MESSAGEBOX_WARNING, "Error parsing JSON", message.c_str());
        else printf("%s\n", message.c_str());
        in.close();
        return;
    }
    in.close();
    readConfigSetting(http_enable, Bool);
    readConfigSetting(debug_enable, Bool);
    readConfigSetting(mount_mode, Int);
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
    readConfigSetting(useHardwareRenderer, Bool);
    readConfigSetting(preferredHardwareDriver, String);
    readConfigSetting(useVsync, Bool);
    if (onboardingMode == 0 && (!root.isMember("lastVersion") || root["lastVersion"].asString() != CRAFTOSPC_VERSION)) { onboardingMode = 2; config_save(); }
    if (config.standardsMode) config.abortTimeout = 7000;
}

void config_save() {
    if (configLoadError) return;
    Value root;
    root["http_enable"] = config.http_enable;
    root["debug_enable"] = config.debug_enable;
    root["mount_mode"] = config.mount_mode;
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
    root["lastVersion"] = CRAFTOSPC_VERSION;
    std::ofstream out(std::string(getBasePath()) + "/config/global.json");
    out << root;
    out.close();
}

void config_deinit(Computer *comp) { config_save(); }

#define getConfigSetting(n, type) else if (strcmp(name, #n) == 0) lua_push##type(L, config.n)

int config_get(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
    const char * name = lua_tostring(L, 1);
    if (strcmp(name, "http_enable") == 0)
        lua_pushboolean(L, config.http_enable);
    getConfigSetting(debug_enable, boolean);
    getConfigSetting(mount_mode, integer);
    getConfigSetting(disable_lua51_features, boolean);
    else if (strcmp(name, "default_computer_settings") == 0)
        lua_pushstring(L, config.default_computer_settings.c_str());
    getConfigSetting(logErrors, boolean);
    getConfigSetting(computerSpaceLimit, integer);
    getConfigSetting(maximumFilesOpen, integer);
    getConfigSetting(maxNotesPerTick, integer);
    getConfigSetting(clockSpeed, integer);
    /*else if (strcmp(name, "http_whitelist") == 0)
        lua_push(L, config.http_whitelist);
    else if (strcmp(name, "http_blacklist") == 0)
        lua_push(L, config.http_blacklist);*/
    getConfigSetting(showFPS, boolean);
    getConfigSetting(abortTimeout, integer);
    getConfigSetting(ignoreHotkeys, boolean);
    else if (strcmp(name, "isColor") == 0)
        lua_pushboolean(L, computer->config.isColor);
    else if (strcmp(name, "startFullscreen") == 0)
        lua_pushboolean(L, computer->config.startFullscreen);
    getConfigSetting(checkUpdates, boolean);
    getConfigSetting(romReadOnly, boolean);
    getConfigSetting(configReadOnly, boolean);
    getConfigSetting(vanilla, boolean);
    getConfigSetting(initialComputer, integer);
    getConfigSetting(maxRecordingTime, integer);
    getConfigSetting(recordingFPS, integer);
    getConfigSetting(showMountPrompt, boolean);
    getConfigSetting(maxOpenPorts, integer);
    getConfigSetting(mouse_move_throttle, number);
    getConfigSetting(monitorsUseMouseEvents, boolean);
    getConfigSetting(defaultWidth, integer);
    getConfigSetting(defaultHeight, integer);
    getConfigSetting(standardsMode, boolean);
    getConfigSetting(useHardwareRenderer, boolean);
    else if (strcmp(name, "preferredHardwareDriver") == 0)
        lua_pushstring(L, config.preferredHardwareDriver.c_str());
    getConfigSetting(useVsync, boolean);
    else if (strcmp(name, "useHDFont") == 0) {
        if (config.customFontPath == "") lua_pushboolean(L, false);
        else if (config.customFontPath == "hdfont") lua_pushboolean(L, true);
        else lua_pushnil(L);
    } else lua_pushnil(L);
    return 1;
}

// first: 0 = immediate, 1 = reboot, 2 = relaunch
// second: 0 = boolean, 1 = number, 2 = string
std::unordered_map<std::string, std::pair<int, int> > configSettings = {
    {"http_enable", {1, 0}},
    {"debug_enable", {1, 0}},
    {"mount_mode", {0, 1}},
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
    {"romReadOnly", {2, 0}},
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
    {"useVsync", {2, 0}}
};

const char * config_set_action_names[3] = {"", "The changes will take effect after rebooting the computer.", "The changes will take effect after restarting CraftOS-PC."};

#define setConfigSetting(n, type) else if (strcmp(name, #n) == 0) config.n = lua_to##type(L, 2)
#define setConfigSettingI(n) else if (strcmp(name, #n) == 0) config.n = luaL_checkinteger(L, 2)

int config_set(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (config.configReadOnly) luaL_error(L, "Configuration is read-only");
    Computer * computer = get_comp(L);
    const char * name = lua_tostring(L, 1);
    if (strcmp(name, "http_enable") == 0)
        config.http_enable = lua_toboolean(L, 2);
    setConfigSetting(debug_enable, boolean);
    else if (strcmp(name, "mount_mode") == 0) {
        if (!lua_isnumber(L, 2) && !lua_isstring(L, 2)) return 0;
        int selected = 0;
        if (dynamic_cast<SDLTerminal*>(computer->term) != NULL) {
            SDL_MessageBoxData data;
            data.flags = SDL_MESSAGEBOX_WARNING;
            data.window = dynamic_cast<SDLTerminal*>(computer->term)->win;
            data.title = "Mount mode change requested";
            // oh why Windows do you make me need to use pointers and dynamic allocation for A SIMPLE STRING INSIDE A SCOPE :((
            std::string * message = new std::string("A script is attempting to change the default mount mode to " + (lua_isnumber(L, 2) ? std::to_string(lua_tointeger(L, 2)) : std::string(lua_tostring(L, 2))) + ". This will allow any script to access any part of your REAL computer that is not blacklisted. Do you want to allow this change?");
            data.message = message->c_str();
            data.numbuttons = 2;
            SDL_MessageBoxButtonData buttons[2];
            buttons[0].flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
            buttons[0].buttonid = 0;
            buttons[0].text = "Deny";
            buttons[1].flags = 0;
            buttons[1].buttonid = 1;
            buttons[1].text = "Allow";
            data.buttons = buttons;
            data.colorScheme = NULL;
            queueTask([data](void*selected)->void* {SDL_ShowMessageBox(&data, (int*)selected); return NULL; }, &selected);
            delete message;
        }
        if (selected) {
            if (lua_type(L, 2) == LUA_TNUMBER) config.mount_mode = lua_tointeger(L, 2);
            else if (lua_type(L, 2) == LUA_TSTRING) {
                const char * mode = lua_tostring(L, 2);
                if (strcmp(mode, "none") == 0) config.mount_mode = MOUNT_MODE_NONE;
                else if (strcmp(mode, "ro strict") == 0 || strcmp(mode, "ro_strict") == 0) config.mount_mode = MOUNT_MODE_RO_STRICT;
                else if (strcmp(mode, "ro") == 0) config.mount_mode = MOUNT_MODE_RO;
                else if (strcmp(mode, "rw") == 0) config.mount_mode = MOUNT_MODE_RW;
                else luaL_error(L, "Unknown mount mode '%s'", mode);
            }
        } else luaL_error(L, "Configuration option 'mount_mode' is protected");
    } setConfigSetting(disable_lua51_features, boolean);
    else if (strcmp(name, "default_computer_settings") == 0)
        config.default_computer_settings = std::string(luaL_checkstring(L, 2), lua_strlen(L, 2));
    setConfigSetting(logErrors, boolean);
    setConfigSettingI(computerSpaceLimit);
    setConfigSettingI(maximumFilesOpen);
    setConfigSettingI(maxNotesPerTick);
    setConfigSettingI(clockSpeed);
    /*else if (strcmp(name, "http_whitelist") == 0)
        config.http_whitelist = lua_to(L, 2);
    else if (strcmp(name, "http_blacklist") == 0)
        config.http_blacklist = lua_to(L, 2);*/
    setConfigSettingI(showFPS);
    setConfigSettingI(abortTimeout);
    setConfigSetting(ignoreHotkeys, boolean);
    else if (strcmp(name, "isColor") == 0) {
        computer->config.isColor = lua_toboolean(L, 2);
        setComputerConfig(computer->id, computer->config);
    } else if (strcmp(name, "startFullscreen") == 0) {
        computer->config.startFullscreen = lua_toboolean(L, 2);
        setComputerConfig(computer->id, computer->config);
    }
    setConfigSetting(checkUpdates, boolean);
    setConfigSetting(romReadOnly, boolean);
    setConfigSetting(vanilla, boolean);
    setConfigSettingI(initialComputer);
    setConfigSettingI(maxRecordingTime);
    setConfigSettingI(recordingFPS);
    setConfigSettingI(maxOpenPorts);
    setConfigSetting(mouse_move_throttle, number);
    setConfigSetting(monitorsUseMouseEvents, boolean);
    setConfigSettingI(defaultWidth);
    setConfigSettingI(defaultHeight);
    setConfigSetting(standardsMode, boolean);
    setConfigSetting(useHardwareRenderer, boolean);
    else if (strcmp(name, "preferredHardwareDriver") == 0)
        config.preferredHardwareDriver = std::string(luaL_checkstring(L, 2), lua_strlen(L, 2));
    setConfigSetting(useVsync, boolean);
    else if (strcmp(name, "useHDFont") == 0)
        config.customFontPath = lua_toboolean(L, 2) ? "hdfont" : "";
    else luaL_error(L, "Unknown configuration option");
    config_save();
    if (configSettings[std::string(name)].first) lua_pushstring(L, config_set_action_names[configSettings[std::string(name)].first]);
    else lua_pushnil(L);
    return 1;
}

int config_list(lua_State *L) {
    lua_newtable(L);
    int i = 1;
    for (auto it = configSettings.begin(); it != configSettings.end(); it++, i++) {
        lua_pushnumber(L, i);
        lua_pushstring(L, it->first.c_str());
        lua_settable(L, -3);
    }
    return 1;
}

int config_getType(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::string name = lua_tostring(L, 1);
    if (configSettings.find(name) == configSettings.end()) lua_pushnil(L);
    else {
        switch (configSettings[name].second) {
            case 0: lua_pushstring(L, "boolean"); break;
            case 1: lua_pushstring(L, "number"); break;
            case 2: lua_pushstring(L, "string"); break;
            default: lua_pushnil(L);
        }
    }
    return 1;
}

const char * config_keys[4] = {
    "get",
    "set",
    "list",
    "getType"
};

lua_CFunction config_values[4] = {
    config_get,
    config_set,
    config_list,
    config_getType
};

library_t config_lib = {"config", 4, config_keys, config_values, nullptr, config_deinit};