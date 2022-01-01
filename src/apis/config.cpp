/*
 * apis/config.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the config API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#include <cstring>
#include <Computer.hpp>
#include <configuration.hpp>
#include "../runtime.hpp"
#include "../terminal/RawTerminal.hpp"
#include "../terminal/SDLTerminal.hpp"
#include "../util.hpp"

#define getConfigSetting(n, type) else if (strcmp(name, #n) == 0) lua_push##type(L, config.n)
#define setConfigSetting(n, type) else if (strcmp(name, #n) == 0) config.n = lua_to##type(L, 2)
#define setConfigSettingI(n) else if (strcmp(name, #n) == 0) config.n = luaL_checkinteger(L, 2)

static const char * config_set_action_names[3] = {"", "The changes will take effect after rebooting the computer.", "The changes will take effect after restarting CraftOS-PC."};

static int config_get(lua_State *L) {
    lastCFunction = __func__;
    Computer * computer = get_comp(L);
    const char * name = luaL_checkstring(L, 1);
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
        lua_pushboolean(L, computer->config->isColor);
    else if (strcmp(name, "startFullscreen") == 0)
        lua_pushboolean(L, computer->config->startFullscreen);
    else if (strcmp(name, "computerWidth") == 0)
        lua_pushinteger(L, computer->config->computerWidth);
    else if (strcmp(name, "computerHeight") == 0)
        lua_pushinteger(L, computer->config->computerHeight);
    getConfigSetting(checkUpdates, boolean);
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
    getConfigSetting(http_websocket_enabled, boolean);
    getConfigSetting(http_max_websockets, integer);
    getConfigSetting(http_max_websocket_message, integer);
    getConfigSetting(http_max_requests, integer);
    getConfigSetting(http_max_upload, integer);
    getConfigSetting(http_max_download, integer);
    getConfigSetting(http_timeout, integer);
    getConfigSetting(extendMargins, boolean);
    getConfigSetting(snapToSize, boolean);
    getConfigSetting(snooperEnabled, boolean);
    getConfigSetting(keepOpenOnShutdown, boolean);
    getConfigSetting(useWebP, boolean);
    getConfigSetting(dropFilePath, boolean);
    getConfigSetting(useDFPWM, boolean);
    else if (strcmp(name, "useHDFont") == 0) {
        if (config.customFontPath.empty()) lua_pushboolean(L, false);
        else if (config.customFontPath == "hdfont") lua_pushboolean(L, true);
        else lua_pushnil(L);
    } else if (strcmp(name, "http_whitelist") == 0) {
        lua_createtable(L, config.http_whitelist.size(), 0);
        for (size_t i = 0; i < config.http_whitelist.size(); i++) {
            lua_pushstring(L, config.http_whitelist[i].c_str());
            lua_rawseti(L, -2, i+1);
        }
    } else if (strcmp(name, "http_blacklist") == 0) {
        lua_createtable(L, config.http_blacklist.size(), 0);
        for (size_t i = 0; i < config.http_blacklist.size(); i++) {
            lua_pushstring(L, config.http_blacklist[i].c_str());
            lua_rawseti(L, -2, i+1);
        }
    } else if (userConfig.find(name) != userConfig.end()) {
        try {
            switch (std::get<0>(userConfig[name])) {
                case 0: lua_pushboolean(L, config.pluginData[name] == "true"); break;
                case 1: lua_pushinteger(L, std::stoi(config.pluginData[name])); break;
                case 2: lua_pushlstring(L, config.pluginData[name].c_str(), config.pluginData[name].size()); break;
                case 3: return luaL_error(L, "Invalid type"); // maybe fix this later?
                default: lua_pushnil(L); break;
            }
        } catch (...) {lua_pushnil(L);}
    } else lua_pushnil(L);
    return 1;
}

static int config_set(lua_State *L) {
    lastCFunction = __func__;
    if (config.configReadOnly) luaL_error(L, "Configuration is read-only");
    Computer * computer = get_comp(L);
    const char * name = luaL_checkstring(L, 1);
    bool isUserConfig = false;
    if (strcmp(name, "http_enable") == 0)
        config.http_enable = lua_toboolean(L, 2);
    else if (strcmp(name, "debug_enable") == 0) ; // do nothing
    else if (strcmp(name, "mount_mode") == 0) {
        if (!lua_isnumber(L, 2) && !lua_isstring(L, 2)) return 0;
        int selected = 0;
        if (dynamic_cast<SDLTerminal*>(computer->term) != NULL) {
            SDL_MessageBoxData data;
            data.flags = SDL_MESSAGEBOX_WARNING;
            data.window = dynamic_cast<SDLTerminal*>(computer->term)->win;
            data.title = "Mount mode change requested";
            // If you're wondering why I'm using dynamic allocation for a static string that isn't needed past the end-of-scope,
            // apparently there's some bug in the MSVC compiler's optimization that makes it sometimes try to delete static
            // variables that a) are out of scope, and b) were never in scope. Of course, this results in a nasty crash due to
            // trying to deallocate unallocated/invalid memory. The only workaround I've found is to make the static variables
            // dynamic, thus telling MSVC to keep its grubby hands off allocation/deallocation. It's an unfortunate situation,
            // but there's really no way around it.
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
            queueTask([data](void*s)->void* {SDL_ShowMessageBox(&data, (int*)s); return NULL; }, &selected);
            delete message;
        }
        if (selected) {
            if (lua_type(L, 2) == LUA_TNUMBER) config.mount_mode = (int)lua_tointeger(L, 2);
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
    setConfigSetting(showFPS, boolean);
    setConfigSettingI(abortTimeout);
    setConfigSetting(ignoreHotkeys, boolean);
    else if (strcmp(name, "isColor") == 0) {
        computer->config->isColor = lua_toboolean(L, 2);
        if (computer->term) computer->term->grayscale = !computer->config->isColor;
        setComputerConfig(computer->id, *computer->config);
    } else if (strcmp(name, "startFullscreen") == 0) {
        computer->config->startFullscreen = lua_toboolean(L, 2);
        setComputerConfig(computer->id, *computer->config);
    } else if (strcmp(name, "computerWidth") == 0) {
        computer->config->computerWidth = luaL_checkinteger(L, 2);
        setComputerConfig(computer->id, *computer->config);
    } else if (strcmp(name, "computerHeight") == 0) {
        computer->config->computerHeight = luaL_checkinteger(L, 2);
        setComputerConfig(computer->id, *computer->config);
    }
    setConfigSetting(checkUpdates, boolean);
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
    setConfigSetting(http_websocket_enabled, boolean);
    setConfigSettingI(http_max_websockets);
    setConfigSettingI(http_max_websocket_message);
    setConfigSettingI(http_max_requests);
    setConfigSettingI(http_max_upload);
    setConfigSettingI(http_max_download);
    setConfigSettingI(http_timeout);
    setConfigSetting(extendMargins, boolean);
    setConfigSetting(snapToSize, boolean);
    setConfigSetting(snooperEnabled, boolean);
    setConfigSetting(keepOpenOnShutdown, boolean);
    setConfigSetting(useWebP, boolean);
    setConfigSetting(dropFilePath, boolean);
    setConfigSetting(useDFPWM, boolean);
    else if (strcmp(name, "useHDFont") == 0)
        config.customFontPath = lua_toboolean(L, 2) ? "hdfont" : "";
    else if (strcmp(name, "http_whitelist") == 0) {
        luaL_checktype(L, 2, LUA_TTABLE);
        config.http_whitelist.clear();
        lua_rawgeti(L, 2, 1);
        for (int i = 1; lua_isstring(L, -1); i++) {
            config.http_whitelist.push_back(luaL_tostring(L, -1));
            lua_pop(L, 1);
            lua_rawgeti(L, 2, i+1);
        }
    } else if (strcmp(name, "http_blacklist") == 0) {
        luaL_checktype(L, 2, LUA_TTABLE);
        config.http_blacklist.clear();
        lua_rawgeti(L, 2, 1);
        for (int i = 1; lua_isstring(L, -1); i++) {
            config.http_blacklist.push_back(luaL_tostring(L, i));
            lua_pop(L, 1);
            lua_rawgeti(L, 2, i+1);
        }
    } else if (userConfig.find(name) != userConfig.end()) {
        isUserConfig = true;
        switch (std::get<0>(userConfig[name])) {
            case 0: config.pluginData[name] = lua_toboolean(L, 2) ? "true" : "false"; break;
            case 1: config.pluginData[name] = std::to_string(luaL_checkinteger(L, 2)); break;
            case 2: config.pluginData[name] = std::string(luaL_checkstring(L, 2), lua_strlen(L, 2)); break;
            case 3: return luaL_error(L, "Invalid type"); // maybe fix this later?
        }
        if (std::get<1>(userConfig[name]) != nullptr) {
            const int retval = std::get<1>(userConfig[name])(name, std::get<2>(userConfig[name]));
            if (retval) lua_pushstring(L, config_set_action_names[retval]);
            else lua_pushnil(L);
        } else lua_pushnil(L);
    } else luaL_error(L, "Unknown configuration option '%s'", lua_tostring(L, 1));
    config_save();
    if (!isUserConfig) {
        if (configSettings[std::string(name)].first) lua_pushstring(L, config_set_action_names[configSettings[std::string(name)].first]);
        else lua_pushnil(L);
    }
    return 1;
}

static int config_list(lua_State *L) {
    lastCFunction = __func__;
    lua_createtable(L, configSettings.size(), 0);
    int i = 1;
    for (auto it = configSettings.begin(); it != configSettings.end(); ++it, i++) {
        lua_pushnumber(L, i);
        lua_pushstring(L, it->first.c_str());
        lua_settable(L, -3);
    }
    for (auto it = userConfig.begin(); it != userConfig.end(); ++it, i++) {
        lua_pushnumber(L, i);
        lua_pushstring(L, it->first.c_str());
        lua_settable(L, -3);
    }
    return 1;
}

static int config_getType(lua_State *L) {
    lastCFunction = __func__;
    const std::string name = luaL_checkstring(L, 1);
    if (configSettings.find(name) == configSettings.end()) {
        if (userConfig.find(name) == userConfig.end()) lua_pushnil(L);
        else {
            switch (std::get<0>(userConfig[name])) {
                case 0: lua_pushstring(L, "boolean"); break;
                case 1: lua_pushstring(L, "number"); break;
                case 2: lua_pushstring(L, "string"); break;
                case 3: lua_pushstring(L, "table"); break;
                default: lua_pushnil(L);
            }
        }
    } else {
        switch (configSettings[name].second) {
            case 0: lua_pushstring(L, "boolean"); break;
            case 1: lua_pushstring(L, "number"); break;
            case 2: lua_pushstring(L, "string"); break;
            case 3: lua_pushstring(L, "table"); break;
            default: lua_pushnil(L);
        }
    }
    return 1;
}

static int config_add(lua_State *L) {
    lastCFunction = __func__;
    const std::string name = luaL_checkstring(L, 1);
    const std::string value = luaL_checkstring(L, 2);
    if (configSettings.find(name) == configSettings.end()) return luaL_error(L, "Unknown configuration option %s", name.c_str());
    else if (configSettings[name].second != 3) return luaL_error(L, "Configuration option %s is not an array", name.c_str());
    if (name == "http_whitelist") config.http_whitelist.push_back(value);
    else if (name == "http_blacklist") config.http_blacklist.push_back(value);
    return 0;
}

static int config_remove(lua_State *L) {
    lastCFunction = __func__;
    const std::string name = luaL_checkstring(L, 1);
    const std::string value = luaL_checkstring(L, 2);
    if (configSettings.find(name) == configSettings.end()) return luaL_error(L, "Unknown configuration option %s", name.c_str());
    else if (configSettings[name].second != 3) return luaL_error(L, "Configuration option %s is not an array", name.c_str());
    if (name == "http_whitelist") config.http_whitelist.erase(std::remove(config.http_whitelist.begin(), config.http_whitelist.end(), value), config.http_whitelist.end());
    else if (name == "http_blacklist") config.http_blacklist.erase(std::remove(config.http_blacklist.begin(), config.http_blacklist.end(), value), config.http_blacklist.end());
    return 0;
}

static void config_deinit(Computer *comp) { config_save(); }

static luaL_Reg config_reg[] = {
    {"get", config_get},
    {"set", config_set},
    {"list", config_list},
    {"getType", config_getType},
    {"add", config_add},
    {"remove", config_remove},
    {NULL, NULL}
};

library_t config_lib = {"config", config_reg, nullptr, config_deinit};