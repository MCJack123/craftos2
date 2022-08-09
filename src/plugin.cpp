/*
 * plugin.cpp
 * CraftOS-PC 2
 * 
 * This file implements various functions relating to plugin loading.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

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

std::string loadingPlugin;
static std::map<path_t, std::pair<void*, PluginInfo*> > loadedPlugins;
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
    &pumpTaskQueue
};

void preloadPlugins() {
    #ifndef STANDALONE_ROM
    const path_t plugin_path = getPlugInPath();
    if (fs::is_directory(plugin_path)) {
        for (const auto& dir : fs::directory_iterator(plugin_path)) {
            if (dir.is_directory()) continue;
            if (dir.path().filename() == ".DS_Store" || dir.path().filename() == "desktop.ini") continue;
            loadingPlugin = dir.path().filename().string();
            void* handle = SDL_LoadObject(dir.path().string().c_str());
            if (handle == NULL) {
                fprintf(stderr, "Failed to load plugin at %s: %s\n", dir.path().string().c_str(), SDL_GetError());
                continue;
            }
            const auto plugin_load = (void(*)(const PluginFunctions*,const path_t&))SDL_LoadFunction(handle, "plugin_load");
            if (plugin_load != NULL) plugin_load(&function_map, dir.path().native());
            loadedPlugins[dir.path()] = std::make_pair(handle, (PluginInfo*)NULL);
        }
    }
#endif
    for (const path_t& path : customPlugins) {
        loadingPlugin = path.filename().string();
        void* handle = SDL_LoadObject(path.string().c_str());
        if (handle == NULL) {
            fprintf(stderr, "Failed to load plugin at %s: %s\n", path.string().c_str(), SDL_GetError());
            continue;
        }
        const auto plugin_load = (void(*)(const PluginFunctions*, const path_t&))SDL_LoadFunction(handle, "plugin_load");
        if (plugin_load != NULL) plugin_load(&function_map, path);
        loadedPlugins[path] = std::make_pair(handle, (PluginInfo*)NULL);
    }
    loadingPlugin = "";
}

std::unordered_map<path_t, std::string> initializePlugins() {
    std::unordered_map<path_t, std::string> failures;
    std::vector<path_t> remove;
    for (auto& p : loadedPlugins) {
        path_t path = p.first;
        loadingPlugin = p.first.string();
        const auto plugin_init = (PluginInfo*(*)(const PluginFunctions*, const path_t&))SDL_LoadFunction(p.second.first, "plugin_init");
        if (plugin_init != NULL) {
            PluginInfo * info = NULL;
            try {
                info = plugin_init(const_cast<const PluginFunctions*>(&function_map), path);
            } catch (std::exception &e) {
                failures[path] = e.what();
                fprintf(stderr, "Failed to load plugin at %s: %s\n", path.string().c_str(), e.what());
                continue;
            }
            if (!info->failureReason.empty()) {
                failures[path] = info->failureReason;
                fprintf(stderr, "Failed to load plugin at %s: %s\n", path.string().c_str(), info->failureReason.c_str());
                const auto plugin_deinit = (void(*)(PluginInfo *))SDL_LoadFunction(p.second.first, "plugin_deinit");
                if (plugin_deinit != NULL) plugin_deinit(info);
                continue;
            }
            if (info->abi_version != PLUGIN_VERSION || info->minimum_structure_version > function_map.structure_version) {
                failures[path] = "CraftOS-PC version too old";
                fprintf(stderr, "Failed to load plugin at %s: This plugin requires a newer version of CraftOS-PC\n", path.string().c_str());
                const auto plugin_deinit = (void(*)(PluginInfo *))SDL_LoadFunction(p.second.first, "plugin_deinit");
                if (plugin_deinit != NULL) plugin_deinit(info);
                continue;
            }
            p.second.second = info;
        } else if (SDL_LoadFunction(p.second.first, "plugin_info") != NULL) {
            failures[path] = "Plugin version too old";
            fprintf(stderr, "Failed to load plugin at %s: This plugin needs to be updated for newer versions of CraftOS-PC\n", path.string().c_str());
            continue;
        } else p.second.second = &defaultInfo;
    }
    loadingPlugin = "";
    return failures;
}

void loadPlugins(Computer * comp) {
    for (const auto& p : loadedPlugins) { if (p.second.second != NULL) {
        lua_CFunction luaopen;
        std::string api_name;
        if (!p.second.second->apiName.empty()) api_name = p.second.second->apiName;
        else api_name = p.first.stem().string();
        loadingPlugin = api_name;
        if (!p.second.second->luaopenName.empty()) luaopen = (lua_CFunction)SDL_LoadFunction(p.second.first, p.second.second->luaopenName.c_str());
        else luaopen = (lua_CFunction)SDL_LoadFunction(p.second.first, ("luaopen_" + api_name).c_str());
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
    }}
    loadingPlugin = "";
}

void deinitializePlugins() {
    userConfig.clear();
    for (auto& p : loadedPlugins) { if (p.second.second != NULL) {
        loadingPlugin = p.first.filename().string();
        const auto plugin_deinit = (void(*)(PluginInfo*))SDL_LoadFunction(p.second.first, "plugin_deinit");
        if (plugin_deinit != NULL) plugin_deinit(p.second.second);
        p.second.second = NULL;
    }}
    loadingPlugin = "";
}

void unloadPlugins() {
    for (const auto& p : loadedPlugins) {
        loadingPlugin = p.first.filename().string();
        const auto plugin_unload = (void(*)())SDL_LoadFunction(p.second.first, "plugin_unload");
        if (plugin_unload != NULL) plugin_unload();
        SDL_UnloadObject(p.second.first);
    }
    loadedPlugins.clear();
}
