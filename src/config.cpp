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
#include <string.h>
#include <fstream>
#include <string>
#include <unordered_map>
#include <Poco/Base64Encoder.h>
#include <Poco/Base64Decoder.h>

struct configuration config;

struct computer_configuration getComputerConfig(int id) {
    struct computer_configuration cfg = {"", true};
    std::ifstream in(std::string(getBasePath()) + "/config/" + std::to_string(id) + ".json");
    if (!in.is_open()) return cfg; 
    if (in.peek() == std::ifstream::traits_type::eof()) {in.close(); return cfg;} // treat an empty file as if it didn't exist in the first place
    Value root;
    Poco::JSON::Object::Ptr p;
    try {p = root.parse(in);} catch (Poco::JSON::JSONException &e) {throw std::runtime_error("Error parsing per-computer config: " + e.message());}
    in.close();
    cfg.isColor = root["isColor"].asBool();
    if (root.isMember("label")) {
        if (root.isMember("base64")) cfg.label = b64decode(root["label"].asString());
        else cfg.label = std::string(root["label"].asString());
    }
    return cfg;
}

void setComputerConfig(int id, struct computer_configuration cfg) {
    Value root;
    if (!cfg.label.empty()) root["label"] = b64encode(cfg.label);
    root["isColor"] = cfg.isColor;
    root["base64"] = true;
    std::ofstream out(std::string(getBasePath()) + "/config/" + std::to_string(id) + ".json");
    out << root;
    out.close();
}

#define readConfigSetting(name, type) if (root.isMember(#name)) config.name = root[#name].as##type()

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
        50
    };
    std::ifstream in(std::string(getBasePath()) + "/config/global.json");
    if (!in.is_open()) {return;}
    Value root;
    Poco::JSON::Object::Ptr p = root.parse(in);
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
}

void config_save(bool deinit) {
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
    std::ofstream out(std::string(getBasePath()) + "/config/global.json");
    out << root;
    out.close();
}

void config_deinit(Computer *comp) { config_save(false); }

int config_get(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
    const char * name = lua_tostring(L, 1);
    if (strcmp(name, "http_enable") == 0)
        lua_pushboolean(L, config.http_enable);
    else if (strcmp(name, "debug_enable") == 0)
        lua_pushboolean(L, config.debug_enable);
    else if (strcmp(name, "mount_mode") == 0)
        lua_pushinteger(L, config.mount_mode);
    else if (strcmp(name, "disable_lua51_features") == 0)
        lua_pushboolean(L, config.disable_lua51_features);
    else if (strcmp(name, "default_computer_settings") == 0)
        lua_pushstring(L, config.default_computer_settings.c_str());
    else if (strcmp(name, "logErrors") == 0)
        lua_pushboolean(L, config.logErrors);
    else if (strcmp(name, "computerSpaceLimit") == 0)
        lua_pushinteger(L, config.computerSpaceLimit);
    else if (strcmp(name, "maximumFilesOpen") == 0)
        lua_pushinteger(L, config.maximumFilesOpen);
    else if (strcmp(name, "maxNotesPerTick") == 0)
        lua_pushinteger(L, config.maxNotesPerTick);
    else if (strcmp(name, "clockSpeed") == 0)
        lua_pushinteger(L, config.clockSpeed);
    /*else if (strcmp(name, "http_whitelist") == 0)
        lua_push(L, config.http_whitelist);
    else if (strcmp(name, "http_blacklist") == 0)
        lua_push(L, config.http_blacklist);*/
    else if (strcmp(name, "showFPS") == 0)
        lua_pushboolean(L, config.showFPS);
    else if (strcmp(name, "abortTimeout") == 0)
        lua_pushinteger(L, config.abortTimeout);
    else if (strcmp(name, "ignoreHotkeys") == 0)
        lua_pushboolean(L, config.ignoreHotkeys);
    else if (strcmp(name, "isColor") == 0)
        lua_pushboolean(L, computer->config.isColor);
    else if (strcmp(name, "checkUpdates") == 0)
        lua_pushboolean(L, config.checkUpdates);
    else if (strcmp(name, "romReadOnly") == 0)
        lua_pushboolean(L, config.romReadOnly);
    else if (strcmp(name, "configReadOnly") == 0)
        lua_pushboolean(L, config.configReadOnly);
    else if (strcmp(name, "vanilla") == 0)
        lua_pushboolean(L, config.vanilla);
    else if (strcmp(name, "initialComputer") == 0)
        lua_pushinteger(L, config.initialComputer);
    else if (strcmp(name, "maxRecordingTime") == 0)
        lua_pushinteger(L, config.maxRecordingTime);
    else if (strcmp(name, "recordingFPS") == 0)
        lua_pushinteger(L, config.recordingFPS);
    else if (strcmp(name, "showMountPrompt") == 0)
        lua_pushboolean(L, config.showMountPrompt);
    else if (strcmp(name, "maxOpenPorts") == 0)
        lua_pushinteger(L, config.maxOpenPorts);
    else if (strcmp(name, "mouse_move_throttle") == 0)
        lua_pushboolean(L, config.mouse_move_throttle);
    else if (strcmp(name, "useHDFont") == 0) {
        if (config.customFontPath == "") lua_pushboolean(L, false);
        else if (config.customFontPath == "hdfont") lua_pushboolean(L, true);
        else lua_pushnil(L);
    } else lua_pushnil(L);
    return 1;
}

// 0 = immediate, 1 = reboot, 2 = relaunch
std::unordered_map<std::string, int> config_set_actions = {
    {"http_enable", 1},
    {"debug_enable", 1},
    {"mount_mode", 0},
    {"disable_lua51_features", 1},
    {"default_computer_settings", 1},
    {"logErrors", 0},
    {"showFPS", 0},
    {"computerSpaceLimit", 0},
    {"maximumFilesOpen", 0},
    {"abortTimeout", 0},
    {"maxNotesPerTick", 2},
    {"clockSpeed", 0},
    {"ignoreHotkeys", 0},
    {"checkUpdates", 2},
    {"romReadOnly", 2},
    {"useHDFont", 2},
    {"configReadOnly", 0},
    {"vanilla", 1},
    {"initialComputer", 2},
    {"maxRecordingTime", 0},
    {"recordingFPS", 0},
    {"showMountPrompt", 0},
    {"maxOpenPorts", 0},
    {"mouse_move_throttle", 0},
    {"isColor", 0}
};

const char * config_set_action_names[3] = {"", "The changes will take effect after rebooting the computer.", "The changes will take effect after restarting CraftOS-PC."};

int config_set(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (config.configReadOnly) luaL_error(L, "Configuration is read-only");
    Computer * computer = get_comp(L);
    const char * name = lua_tostring(L, 1);
    if (strcmp(name, "http_enable") == 0)
        config.http_enable = lua_toboolean(L, 2);
    else if (strcmp(name, "debug_enable") == 0)
        config.debug_enable = lua_toboolean(L, 2);
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
            queueTask([data](void*selected)->void*{SDL_ShowMessageBox(&data, (int*)selected); return NULL;}, &selected);
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
    }
    else if (strcmp(name, "disable_lua51_features") == 0)
        config.disable_lua51_features = lua_toboolean(L, 2);
    else if (strcmp(name, "default_computer_settings") == 0) 
        config.default_computer_settings = std::string(luaL_checkstring(L, 2), lua_strlen(L, 2));
    else if (strcmp(name, "logErrors") == 0)
        config.logErrors = lua_toboolean(L, 2);
    else if (strcmp(name, "computerSpaceLimit") == 0)
        config.computerSpaceLimit = luaL_checkinteger(L, 2);
    else if (strcmp(name, "maximumFilesOpen") == 0)
        config.maximumFilesOpen = luaL_checkinteger(L, 2);
    else if (strcmp(name, "maxNotesPerTick") == 0)
        config.maxNotesPerTick = luaL_checkinteger(L, 2);
    else if (strcmp(name, "clockSpeed") == 0)
        config.clockSpeed = luaL_checkinteger(L, 2);
    /*else if (strcmp(name, "http_whitelist") == 0)
        config.http_whitelist = lua_to(L, 2);
    else if (strcmp(name, "http_blacklist") == 0)
        config.http_blacklist = lua_to(L, 2);*/
    else if (strcmp(name, "showFPS") == 0)
        config.showFPS = luaL_checkinteger(L, 2);
    else if (strcmp(name, "abortTimeout") == 0)
        config.abortTimeout = luaL_checkinteger(L, 2);
    else if (strcmp(name, "ignoreHotkeys") == 0)
        config.ignoreHotkeys = lua_toboolean(L, 2);
    else if (strcmp(name, "isColor") == 0) {
        computer->config.isColor = lua_toboolean(L, 2);
        setComputerConfig(computer->id, computer->config);
    } else if (strcmp(name, "checkUpdates") == 0)
        config.checkUpdates = lua_toboolean(L, 2);
    else if (strcmp(name, "romReadOnly") == 0)
        config.romReadOnly = lua_toboolean(L, 2);
    else if (strcmp(name, "configReadOnly") == 0)
        config.configReadOnly = lua_toboolean(L, 2);
    else if (strcmp(name, "vanilla") == 0)
        config.vanilla = lua_toboolean(L, 2);
    else if (strcmp(name, "initialComputer") == 0)
        config.initialComputer = luaL_checkinteger(L, 2);
    else if (strcmp(name, "maxRecordingTime") == 0)
        config.maxRecordingTime = luaL_checkinteger(L, 2);
    else if (strcmp(name, "recordingFPS") == 0)
        config.recordingFPS = luaL_checkinteger(L, 2);
    else if (strcmp(name, "maxOpenPorts") == 0)
        config.maxOpenPorts = luaL_checkinteger(L, 2);
    else if (strcmp(name, "mouse_move_throttle") == 0)
        config.mouse_move_throttle = lua_toboolean(L, 2);
    else if (strcmp(name, "useHDFont") == 0)
        config.customFontPath = lua_toboolean(L, 2) ? "hdfont" : "";
    else luaL_error(L, "Unknown configuration option");
    config_save(false);
    if (config_set_actions[std::string(name)]) lua_pushstring(L, config_set_action_names[config_set_actions[std::string(name)]]);
    else lua_pushnil(L);
    return 1;
}

const char * configuration_keys[] = {
    "http_enable",
    "debug_enable",
    "mount_mode",
    "disable_lua51_features",
    "default_computer_settings",
    "logErrors",
    "computerSpaceLimit",
    "maximumFilesOpen",
    "maxNotesPerTick",
    "clockSpeed",
    "showFPS",
    "abortTimeout",
    "ignoreHotkeys",
    "isColor",
    "checkUpdates",
    "romReadOnly",
    "configReadOnly",
    "vanilla",
    "initialComputer",
    "maxRecordingTime",
    "recordingFPS",
    "useHDFont",
    "showMountPrompt",
    "maxOpenPorts",
    "mouse_move_throttle",
    NULL,
};

int config_list(lua_State *L) {
    lua_newtable(L);
    for (int i = 1; configuration_keys[i] != NULL; i++) {
        lua_pushnumber(L, i);
        lua_pushstring(L, configuration_keys[i]);
        lua_settable(L, -3);
    }
    return 1;
}

int config_getType(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    std::string name = lua_tostring(L, 1);
    if (name == "http_enable" || name == "debug_enable" ||
        name == "disable_lua51_features" || name == "logErrors" || 
        name == "showFPS" || name == "ignoreHotkeys" || name == "isColor" ||
        name == "checkUpdates" || name == "romReadOnly" || 
        name == "configReadOnly" || name == "vanilla" || name == "useHDFont" || 
        name == "showMountPrompt")
        lua_pushstring(L, "boolean");
    else if (name == "default_computer_settings")
        lua_pushstring(L, "string");
    else if (name == "computerSpaceLimit" || name == "maximumFilesOpen" || 
             name == "maxNotesPerTick" || name == "clockSpeed" || 
             name == "abortTimeout" || name == "mount_mode" || 
             name == "initialComputer" || name == "maxRecordingTime" || 
             name == "recordingFPS" || name == "maxOpenPorts" || name == "mouse_move_throttle")
        lua_pushstring(L, "number");
    else lua_pushnil(L);
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