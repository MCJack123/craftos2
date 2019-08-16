/*
 * config.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the config API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

extern "C" {
#include "config.h"
#include "platform.h"
}
#include <string.h>
#include <json/json.h>
#include <fstream>
#include <string>
#include <unordered_map>

extern "C" {extern int computerID;}

struct configuration config;

struct computer_configuration getComputerConfig(int id) {
    struct computer_configuration cfg = {NULL, true};
    std::ifstream in(std::string(getBasePath()) + "/config/" + std::to_string(id) + ".json");
    if (!in.is_open()) return cfg; 
    Json::Value root;
    in >> root;
    in.close();
    cfg.isColor = root["isColor"].asBool();
    if (root.isMember("label") && root["label"].asString() != "") {
        cfg.label = (char*)malloc(root["label"].asString().size() + 1);
        strcpy(cfg.label, root["label"].asCString());
    }
    return cfg;
}

void setComputerConfig(int id, struct computer_configuration cfg) {
    Json::Value root(Json::objectValue);
    if (cfg.label != NULL) root["label"] = Json::Value(cfg.label);
    root["isColor"] = Json::Value(cfg.isColor);
    std::ofstream out(std::string(getBasePath()) + "/config/" + std::to_string(id) + ".json");
    out << root;
    out.close();
}

void freeComputerConfig(struct computer_configuration cfg) {
    if (cfg.label != NULL) free(cfg.label);
}

void config_init(void) {
    createDirectory((std::string(getBasePath()) + "/config").c_str());
    config = {
        true,
        false,
        false,
        NULL,
        false,
        false,
        false,
        1000000,
        128,
        17000,
        8,
        20,
        false
    };
    std::ifstream in(std::string(getBasePath()) + "/config/global.json");
    if (!in.is_open()) {config.readFail = true; return;}
    Json::Value root;
    in >> root;
    in.close();
    if (root.isMember("http_enable")) config.http_enable = root["http_enable"].asBool();
    if (root.isMember("debug_enable")) config.debug_enable = root["debug_enable"].asBool();
    if (root.isMember("disable_lua51_features")) config.disable_lua51_features = root["disable_lua51_features"].asBool();
    if (root.isMember("default_computer_settings")) {
        config.default_computer_settings = (char*)malloc(strlen(root["default_computer_settings"].asCString())+1);
        strcpy(config.default_computer_settings, root["default_computer_settings"].asCString());
    }
    if (root.isMember("logPeripheralErrors")) config.logPeripheralErrors = root["logPeripheralErrors"].asBool();
    if (root.isMember("showFPS")) config.showFPS = root["showFPS"].asBool();
    config.readFail = false;
    if (root.isMember("computerSpaceLimit")) config.computerSpaceLimit = root["computerSpaceLimit"].asInt();
    if (root.isMember("maximumFilesOpen")) config.maximumFilesOpen = root["maximumFilesOpen"].asInt();
    if (root.isMember("abortTimeout")) config.abortTimeout = root["abortTimeout"].asInt();
    if (root.isMember("maxNotesPerTick")) config.maxNotesPerTick = root["maxNotesPerTick"].asInt();
    if (root.isMember("clockSpeed")) config.clockSpeed = root["clockSpeed"].asInt();
    if (root.isMember("ignoreHotkeys")) config.ignoreHotkeys = root["ignoreHotkeys"].asBool();
}

void config_save(bool deinit) {
    Json::Value root(Json::objectValue);
    root["http_enable"] = Json::Value(config.http_enable);
    root["debug_enable"] = Json::Value(config.debug_enable);
    root["disable_lua51_features"] = Json::Value(config.disable_lua51_features);
    if (config.default_computer_settings != NULL) root["default_computer_settings"] = Json::Value(config.default_computer_settings);
    root["logPeripheralErrors"] = Json::Value(config.logPeripheralErrors);
    root["showFPS"] = Json::Value(config.showFPS);
    root["computerSpaceLimit"] = Json::Value(config.computerSpaceLimit);
    root["maximumFilesOpen"] = Json::Value(config.maximumFilesOpen);
    root["abortTimeout"] = Json::Value(config.abortTimeout);
    root["maxNotesPerTick"] = Json::Value(config.maxNotesPerTick);
    root["clockSpeed"] = Json::Value(config.clockSpeed);
    std::ofstream out(std::string(getBasePath()) + "/config/global.json");
    out << root;
    out.close();
    if (deinit) {
        if (config.default_computer_settings != NULL) free(config.default_computer_settings);
        config.default_computer_settings = NULL;
    }
}

void config_deinit() { config_save(true); }

int config_get(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * name = lua_tostring(L, 1);
    if (strcmp(name, "http_enable") == 0)
        lua_pushboolean(L, config.http_enable);
    else if (strcmp(name, "debug_enable") == 0)
        lua_pushboolean(L, config.debug_enable);
    else if (strcmp(name, "disable_lua51_features") == 0)
        lua_pushboolean(L, config.disable_lua51_features);
    else if (strcmp(name, "default_computer_settings") == 0 && config.default_computer_settings != NULL)
        lua_pushstring(L, config.default_computer_settings);
    else if (strcmp(name, "logPeripheralErrors") == 0)
        lua_pushboolean(L, config.logPeripheralErrors);
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
    else if (strcmp(name, "readFail") == 0)
        lua_pushboolean(L, config.readFail);
    else if (strcmp(name, "abortTimeout") == 0)
        lua_pushinteger(L, config.abortTimeout);
    else if (strcmp(name, "ignoreHotkeys") == 0)
        lua_pushboolean(L, config.ignoreHotkeys);
    else if (strcmp(name, "isColor") == 0) {
        struct computer_configuration cfg = getComputerConfig(computerID);
        lua_pushboolean(L, cfg.isColor);
        freeComputerConfig(cfg);
    }
    else return 0;
    return 1;
}

int config_set(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * name = lua_tostring(L, 1);
    if (strcmp(name, "http_enable") == 0)
        config.http_enable = lua_toboolean(L, 2);
    else if (strcmp(name, "debug_enable") == 0)
        config.debug_enable = lua_toboolean(L, 2);
    else if (strcmp(name, "disable_lua51_features") == 0)
        config.disable_lua51_features = lua_toboolean(L, 2);
    else if (strcmp(name, "default_computer_settings") == 0) {
        if (config.default_computer_settings != NULL) free(config.default_computer_settings);
        config.default_computer_settings = (char*)malloc(lua_strlen(L, 2));
        strcpy(config.default_computer_settings, lua_tostring(L, 2));
    } else if (strcmp(name, "logPeripheralErrors") == 0)
        config.logPeripheralErrors = lua_toboolean(L, 2);
    else if (strcmp(name, "computerSpaceLimit") == 0)
        config.computerSpaceLimit = lua_tointeger(L, 2);
    else if (strcmp(name, "maximumFilesOpen") == 0)
        config.maximumFilesOpen = lua_tointeger(L, 2);
    else if (strcmp(name, "maxNotesPerTick") == 0)
        config.maxNotesPerTick = lua_tointeger(L, 2);
    else if (strcmp(name, "clockSpeed") == 0)
        config.clockSpeed = lua_tointeger(L, 2);
    /*else if (strcmp(name, "http_whitelist") == 0)
        config.http_whitelist = lua_to(L, 2);
    else if (strcmp(name, "http_blacklist") == 0)
        config.http_blacklist = lua_to(L, 2);*/
    else if (strcmp(name, "showFPS") == 0)
        config.showFPS = lua_tointeger(L, 2);
    else if (strcmp(name, "abortTimeout") == 0)
        config.abortTimeout = lua_tointeger(L, 2);
    else if (strcmp(name, "ignoreHotkeys") == 0)
        config.ignoreHotkeys = lua_toboolean(L, 2);
    else if (strcmp(name, "isColor") == 0) {
        struct computer_configuration cfg = getComputerConfig(computerID);
        cfg.isColor = lua_toboolean(L, 2);
        setComputerConfig(computerID, cfg);
    }
    config_save(false);
    return 0;
}

int config_list(lua_State *L) {
    lua_newtable(L);
    lua_pushnumber(L, 1);
    lua_pushstring(L, "http_enable");
    lua_settable(L, -3);

    lua_pushnumber(L, 2);
    lua_pushstring(L, "debug_enable");
    lua_settable(L, -3);

    lua_pushnumber(L, 3);
    lua_pushstring(L, "disable_lua51_features");
    lua_settable(L, -3);

    lua_pushnumber(L, 4);
    lua_pushstring(L, "default_computer_settings");
    lua_settable(L, -3);

    lua_pushnumber(L, 5);
    lua_pushstring(L, "logPeripheralErrors");
    lua_settable(L, -3);

    lua_pushnumber(L, 6);
    lua_pushstring(L, "computerSpaceLimit");
    lua_settable(L, -3);

    lua_pushnumber(L, 7);
    lua_pushstring(L, "maximumFilesOpen");
    lua_settable(L, -3);

    lua_pushnumber(L, 8);
    lua_pushstring(L, "maxNotesPerTick");
    lua_settable(L, -3);

    lua_pushnumber(L, 9);
    lua_pushstring(L, "clockSpeed");
    lua_settable(L, -3);

    lua_pushnumber(L, 10);
    lua_pushstring(L, "showFPS");
    lua_settable(L, -3);

    lua_pushnumber(L, 11);
    lua_pushstring(L, "readFail");
    lua_settable(L, -3);

    lua_pushnumber(L, 12);
    lua_pushstring(L, "abortTimeout");
    lua_settable(L, -3);

    lua_pushnumber(L, 13);
    lua_pushstring(L, "ignoreHotkeys");
    lua_settable(L, -3);

    lua_pushnumber(L, 14);
    lua_pushstring(L, "isColor");
    lua_settable(L, -3);
    return 1;
}

int config_getType(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    const char * name = lua_tostring(L, 1);
    if (strcmp(name, "http_enable") == 0)
        lua_pushinteger(L, 0);
    else if (strcmp(name, "debug_enable") == 0)
        lua_pushinteger(L, 0);
    else if (strcmp(name, "disable_lua51_features") == 0)
        lua_pushinteger(L, 0);
    else if (strcmp(name, "default_computer_settings") == 0)
        lua_pushinteger(L, 1);
    else if (strcmp(name, "logPeripheralErrors") == 0)
        lua_pushinteger(L, 0);
    else if (strcmp(name, "computerSpaceLimit") == 0)
        lua_pushinteger(L, 2);
    else if (strcmp(name, "maximumFilesOpen") == 0)
        lua_pushinteger(L, 2);
    else if (strcmp(name, "maxNotesPerTick") == 0)
        lua_pushinteger(L, 2);
    else if (strcmp(name, "clockSpeed") == 0)
        lua_pushinteger(L, 2);
    /*else if (strcmp(name, "http_whitelist") == 0)
        lua_pushinteger(L, 3);
    else if (strcmp(name, "http_blacklist") == 0)
        lua_pushinteger(L, 3);*/
    else if (strcmp(name, "showFPS") == 0)
        lua_pushinteger(L, 0);
    else if (strcmp(name, "abortTimeout") == 0)
        lua_pushinteger(L, 2);
    else if (strcmp(name, "ignoreHotkeys") == 0)
        lua_pushboolean(L, 0);
    else if (strcmp(name, "isColor") == 0)
        lua_pushinteger(L, 0);
    else lua_pushinteger(L, -1);
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

library_t config_lib = {"config", 4, config_keys, config_values, config_init, config_deinit};