// Copyright (c) 2019-2024 JackMacWindows.
// SPDX-FileCopyrightText: 2019-2024 JackMacWindows
//
// SPDX-License-Identifier: MIT

#include <unordered_map>
#define get_comp __get_comp_unused
#include <CraftOS-PC.hpp>
#undef path_t
#undef get_comp
#include <dirent.h>
#include <sys/stat.h>
#include "apis.hpp"
#include "main.hpp"
#include "platform.hpp"
#include "runtime.hpp"
#include "termsupport.hpp"
#include "util.hpp"

typedef PluginInfo * (*plugin_init_t)(const PluginFunctions *, const path_t&);
typedef void (*plugin_deinit_t)(PluginInfo *);

std::string loadingPlugin;
std::unordered_map<std::string, std::tuple<int, std::function<int(const std::string&, void*)>, void*> > userConfig;
static PluginInfo defaultInfo;

static library_t * getLibrary(const std::string& name) {
    if (name == "config") return &config_lib;
    else if (name == "fs") return &fs_lib;
#ifndef NO_MOUNTER
    else if (name == "mounter") return &mounter_lib;
#endif
    else if (name == "os") return &os_lib;
    else if (name == "peripheral") return &peripheral_lib;
    else if (name == "periphemu") return &periphemu_lib;
    else if (name == "redstone" || name == "rs") return &rs_lib;
    else if (name == "term") return &term_lib;
    else return NULL;
}

static Computer * getComputerById(int id) {
    LockGuard lock(computers);
    for (Computer * c : *computers) if (c->id == id) return c;
    return NULL;
}

static std::string getConfigSetting(const std::string& name) {return config.pluginData.at(name);}
static int getConfigSettingInt(const std::string& name) {return std::stoi(config.pluginData.at(name));}
static bool getConfigSettingBool(const std::string& name) {
    const std::string val = config.pluginData.at(name);
    if (val == "true") return true;
    else if (val == "false") return false;
    else throw std::invalid_argument("Not a boolean value");
}
static void setConfigSetting(const std::string& name, const std::string& value) {config.pluginData[name] = value;}
static void setConfigSettingInt(const std::string& name, int value) {config.pluginData[name] = std::to_string(value);}
static void setConfigSettingBool(const std::string& name, bool value) {config.pluginData[name] = value ? "true" : "false";}
static void registerConfigSetting(const std::string& name, int type, const std::function<int(const std::string&, void*)>& callback, void* userdata) {userConfig[name] = std::make_tuple(type, callback, userdata);}
extern void setDistanceProvider(const std::function<double(const Computer *, const Computer *)>& func);
static void registerPeripheral_ptr(const std::string& name, const peripheral_init& fn) {return registerPeripheral(name, fn);}
static int registerTerminalFactory(TerminalFactory * factory) {terminalFactories.push_back(factory); return terminalFactories.size() - 1;}
static void setListenerMode(bool mode) {listenerMode = mode; if (!mode) queueTask([](void*)->void*{return NULL;}, NULL);}
static _path_t _getBasePath() {return getBasePath().native();}
static _path_t _getROMPath() {return getROMPath().native();}
static bool _addMount(Computer *comp, const _path_t& real_path, const char * comp_path, bool read_only) {return addMount(comp, real_path, comp_path, read_only);}
static bool _addVirtualMount(Computer * comp, const FileEntry& vfs, const char * comp_path) {return addVirtualMount(comp, vfs, comp_path);}
extern bool checkIAPEligibility(const char * identifier);

static const PluginFunctions function_map = {
    PLUGIN_VERSION,
    8,
    CRAFTOSPC_VERSION,
    selectedRenderer,
    &config,
    &_getBasePath,
    &_getROMPath,
    &getLibrary,
    &getComputerById,
    &registerPeripheral_ptr,
    &registerSDLEvent,
    &_addMount,
    &_addVirtualMount,
    &startComputer,
    &queueEvent,
    &queueTask,
    &getConfigSetting,
    &getConfigSettingInt,
    &getConfigSettingBool,
    &setConfigSetting,
    &setConfigSettingInt,
    &setConfigSettingBool,
    &registerConfigSetting,
    &attachPeripheral,
    &detachPeripheral,
    &addEventHook,
    &setDistanceProvider,
    &registerPeripheral,
    &registerTerminalFactory,
    &parseArguments,
    &setListenerMode,
    &checkIAPEligibility,
    &pumpTaskQueue
};

extern "C" {
extern PluginInfo * plugin_init_ccemux(const PluginFunctions * func, const path_t& path);
extern PluginInfo * plugin_init_joystick(const PluginFunctions * func, const path_t& path);
extern PluginInfo * plugin_init_sound(const PluginFunctions * func, const path_t& path);

extern int luaopen_ccemux(lua_State *L);
extern int luaopen_joystick(lua_State *L);
extern int luaopen_sound(lua_State *L);

extern void plugin_deinit_joystick(PluginInfo * info);
extern void plugin_deinit_sound(PluginInfo * info);
}

typedef std::tuple<path_t, plugin_init_t, lua_CFunction, plugin_deinit_t> plugin_t;
static const plugin_t plugins[] = {
    std::make_tuple(path_t("ccemux"), plugin_init_ccemux, luaopen_ccemux, (plugin_deinit_t)NULL),
    std::make_tuple(path_t("joystick"), plugin_init_joystick, luaopen_joystick, plugin_deinit_joystick),
    std::make_tuple(path_t("sound"), plugin_init_sound, luaopen_sound, plugin_deinit_sound)
};
static const int pluginCount = 3;

static std::vector<std::pair<PluginInfo *, const plugin_t *> > loadedPlugins;

void preloadPlugins() {
    // do nothing, all plugins are loaded
}

std::unordered_map<path_t, std::string> initializePlugins() {
    std::unordered_map<path_t, std::string> failures;
    for (int i = 0; i < pluginCount; i++) {
        const plugin_t plugin = plugins[i];
        path_t path = std::get<0>(plugin);
        PluginInfo * info = NULL;
        try {
            info = std::get<1>(plugin)(const_cast<const PluginFunctions*>(&function_map), path);
        } catch (std::exception &e) {
            failures[path] = e.what();
            fprintf(stderr, "Failed to load plugin at %s: %s\n", path.string().c_str(), e.what());
            continue;
        }
        if (info->abi_version != PLUGIN_VERSION || info->minimum_structure_version > function_map.structure_version) {
            failures[path] = "CraftOS-PC version too old";
            fprintf(stderr, "Failed to load plugin at %s: This plugin requires a newer version of CraftOS-PC\n", path.string().c_str());
            const auto plugin_deinit = std::get<3>(plugin);
            if (plugin_deinit != NULL) plugin_deinit(info);
            continue;
        }
        if (!info->failureReason.empty()) {
            failures[path] = info->failureReason;
            fprintf(stderr, "Failed to load plugin at %s: %s\n", path.string().c_str(), info->failureReason.c_str());
            const auto plugin_deinit = std::get<3>(plugin);
            if (plugin_deinit != NULL) plugin_deinit(info);
            continue;
        }
        loadedPlugins.push_back(std::make_pair(info, &plugins[i]));
    }
    return failures;
}

void loadPlugins(Computer * comp) {
    for (const auto& p : loadedPlugins) {
        lua_CFunction luaopen;
        std::string api_name;
        if (!p.first->apiName.empty()) api_name = p.first->apiName;
        else api_name = std::get<0>(*p.second).string();
        loadingPlugin = api_name;
        luaopen = std::get<2>(*p.second);
        if (luaopen == NULL) {
            fprintf(stderr, "Error loading plugin %s: Missing API opener\n", api_name.c_str());
            lua_getglobal(comp->L, "_CCPC_PLUGIN_ERRORS");
            if (lua_isnil(comp->L, -1)) {
                lua_newtable(comp->L);
                lua_pushvalue(comp->L, -1);
                lua_setglobal(comp->L, "_CCPC_PLUGIN_ERRORS");
            }
            lua_pushstring(comp->L, api_name.c_str());
            lua_pushstring(comp->L, "Missing API opener");
            lua_settable(comp->L, -3);
            lua_pop(comp->L, 1);
            continue;
        }
        lua_pushcfunction(comp->L, luaopen);
        lua_pushstring(comp->L, api_name.c_str());
        // todo: pcall this?
        lua_call(comp->L, 1, 1);
        lua_setglobal(comp->L, api_name.c_str());
    }
    loadingPlugin = "";
}

void deinitializePlugins() {
    userConfig.clear();
    for (auto& p : loadedPlugins) {
        loadingPlugin = std::get<0>(*p.second).string();
        const auto plugin_deinit = std::get<3>(*p.second);
        if (plugin_deinit != NULL) plugin_deinit(p.first);
    }
    loadingPlugin = "";
}

void unloadPlugins() {
    // do nothing
}
