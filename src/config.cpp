/*
 * config.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the config API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "config.hpp"
#include "platform.hpp"
#include <string.h>
#include <fstream>
#include <string>
#include <unordered_map>
#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Parser.h>

using namespace Poco::JSON;
struct configuration config;

class Value {
    Poco::Dynamic::Var obj;
    Value* parent = NULL;
    std::string key;
    Value(Poco::Dynamic::Var o, Value* p, std::string k): obj(o), parent(p), key(k) {}
    void updateParent() {
        if (parent == NULL) return;
        Object o(parent->obj.extract<Object>());
        o.set(key, obj);
        parent->obj = o;
    }
public:
    Value() {obj = Object();}
    Value(Poco::Dynamic::Var o): obj(o) {}
    Value operator[](std::string key) { return Value(obj.extract<Object>().get(key), this, key); }
    void operator=(int v) { obj = v; updateParent(); }
    void operator=(bool v) { obj = v; updateParent(); }
    void operator=(std::string v) { obj = v; updateParent(); }
    bool asBool() { return obj.convert<bool>(); }
    int asInt() { return obj.convert<int>(); }
    std::string asString() { return obj.toString(); }
    const char * asCString() { return obj.toString().c_str(); }
    bool isMember(std::string key) { return obj.extract<Object>().has(key); }
    Object::Ptr parse(std::istream& in) { Object::Ptr p = Parser().parse(in).extract<Object::Ptr>(); obj = *p; return p; }
    friend std::ostream& operator<<(std::ostream &out, Value &v) { Stringifier().stringify(v.obj.extract<Object>(), out, 4); return out; }
    //friend std::istream& operator>>(std::istream &in, Value &v) {v.obj = Parser().parse(in).extract<Object::Ptr>(); return in; }
};

struct computer_configuration getComputerConfig(int id) {
    struct computer_configuration cfg = {NULL, true};
    std::ifstream in(std::string(getBasePath()) + "/config/" + std::to_string(id) + ".json");
    if (!in.is_open()) return cfg; 
    Value root;
    Object::Ptr p = root.parse(in);
    in.close();
    cfg.isColor = root["isColor"].asBool();
    if (root.isMember("label") && root["label"].asString() != "") {
        cfg.label = (char*)malloc(root["label"].asString().size() + 1);
        strcpy(cfg.label, root["label"].asCString());
    }
    return cfg;
}

void setComputerConfig(int id, struct computer_configuration cfg) {
    Value root;
    if (cfg.label != NULL) root["label"] = std::string(cfg.label);
    root["isColor"] = cfg.isColor;
    std::ofstream out(std::string(getBasePath()) + "/config/" + std::to_string(id) + ".json");
    Stringifier s;
    out << root;
    out.close();
}

void freeComputerConfig(struct computer_configuration cfg) {
    if (cfg.label != NULL) free(cfg.label);
}

void config_init() {
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
    Value root;
    Object::Ptr p = root.parse(in);
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
    Value root;
    root["http_enable"] = config.http_enable;
    root["debug_enable"] = config.debug_enable;
    root["disable_lua51_features"] = config.disable_lua51_features;
    if (config.default_computer_settings != NULL) root["default_computer_settings"] = config.default_computer_settings;
    root["logPeripheralErrors"] = config.logPeripheralErrors;
    root["showFPS"] = config.showFPS;
    root["computerSpaceLimit"] = config.computerSpaceLimit;
    root["maximumFilesOpen"] = config.maximumFilesOpen;
    root["abortTimeout"] = config.abortTimeout;
    root["maxNotesPerTick"] = config.maxNotesPerTick;
    root["clockSpeed"] = config.clockSpeed;
    std::ofstream out(std::string(getBasePath()) + "/config/global.json");
    out << root;
    out.close();
    if (deinit) {
        if (config.default_computer_settings != NULL) free(config.default_computer_settings);
        config.default_computer_settings = NULL;
    }
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
    else if (strcmp(name, "isColor") == 0) 
        lua_pushboolean(L, computer->config.isColor);
    else return 0;
    return 1;
}

int config_set(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    Computer * computer = get_comp(L);
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
        computer->config.isColor = lua_toboolean(L, 2);
        setComputerConfig(computer->id, computer->config);
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

library_t config_lib = {"config", 4, config_keys, config_values, NULL, config_deinit};