/*
 * plugin.cpp
 * CraftOS-PC 2
 * 
 * This file implements various functions relating to plugin loading.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include <unordered_map>
#include <CraftOS-PC.hpp>
#include <dirent.h>
#include <sys/stat.h>
#include "apis.hpp"
#include "platform.hpp"
#include "runtime.hpp"
#include "util.hpp"
#ifdef _WIN32
#define PATH_SEP L"\\"
#else
#define PATH_SEP "/"
#endif

static std::unordered_map<path_t, std::pair<void*, PluginInfo> > loadedPlugins;

static library_t * getLibrary(const std::string& name) {
    if (name == "config") return &config_lib;
    else if (name == "fs") return &fs_lib; 
    else if (name == "mounter") return &mounter_lib; 
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

static PluginFunctions function_map = {
    PLUGIN_VERSION,
    0,
    selectedRenderer,
    &getBasePath,
    &getROMPath,
    &getLibrary,
    &getComputerById,
    &registerPeripheral,
    &registerSDLEvent,
    &addMount,
    &addVirtualMount,
    &startComputer,
    &queueEvent,
    &queueTask
};

std::unordered_map<path_t, std::string> initializePlugins() {
    std::unordered_map<path_t, std::string> failures;
#ifndef STANDALONE_ROM
    const path_t plugin_path = getPlugInPath();
    platform_DIR * d = platform_opendir(plugin_path.c_str());
    struct_stat st;
    if (d) {
        struct_dirent *dir;
        for (int i = 0; (dir = platform_readdir(d)) != NULL; i++) {
            if (platform_stat((plugin_path + PATH_SEP + path_t(dir->d_name)).c_str(), &st) == 0 && S_ISDIR(st.st_mode)) continue;
            if (path_t(dir->d_name) == WS(".DS_Store") || path_t(dir->d_name) == WS("desktop.ini")) continue;
            path_t path = plugin_path + PATH_SEP + dir->d_name;
            void* handle = SDL_LoadObject(astr(path).c_str());
            if (handle == NULL) {
                failures[path] = "File could not be loaded";
                printf("Failed to load plugin at %s: File is not a dynamic library\n", astr(plugin_path + PATH_SEP + dir->d_name).c_str());
                continue;
            }
            const auto plugin_init = (PluginInfo(*)(const PluginFunctions*, const path_t&))SDL_LoadFunction(handle, "plugin_init");
            if (plugin_init != NULL) {
                PluginInfo info;
                try {
                    info = plugin_init(const_cast<const PluginFunctions*>(&function_map), path);
                } catch (std::exception &e) {
                    failures[path] = e.what();
                    printf("Failed to load plugin at %s: %s\n", astr(plugin_path + PATH_SEP + dir->d_name).c_str(), e.what());
                    SDL_UnloadObject(handle);
                    continue;
                }
                if (!info.failureReason.empty()) {
                    failures[path] = info.failureReason;
                    printf("Failed to load plugin at %s: %s\n", astr(plugin_path + PATH_SEP + dir->d_name).c_str(), info.failureReason.c_str());
                    SDL_UnloadObject(handle);
                    continue;
                }
                if (info.abi_version != PLUGIN_VERSION || info.minimum_structure_version > function_map.structure_version) {
                    failures[path] = "CraftOS-PC version too old";
                    printf("Failed to load plugin at %s: This plugin requires a newer version of CraftOS-PC\n", astr(plugin_path + PATH_SEP + dir->d_name).c_str());
                    SDL_UnloadObject(handle);
                    continue;
                }
                loadedPlugins[path] = std::make_pair(handle, info);
            } else if (SDL_LoadFunction(handle, "plugin_info") != NULL) {
                failures[path] = "Plugin version too old";
                printf("Failed to load plugin at %s: This plugin needs to be updated for newer versions of CraftOS-PC\n", astr(plugin_path + PATH_SEP + dir->d_name).c_str());
                SDL_UnloadObject(handle);
                continue;
            } else loadedPlugins[path] = std::make_pair(handle, PluginInfo());
        }
        platform_closedir(d);
    }
#endif
    for (const path_t& path : customPlugins) {
        void* handle = SDL_LoadObject(astr(path).c_str());
        if (handle == NULL) {
            failures[path] = "File could not be loaded";
            printf("Failed to load plugin at %s: File is not a dynamic library\n", astr(path).c_str());
            continue;
        }
        const auto plugin_init = (PluginInfo(*)(const PluginFunctions*, path_t))SDL_LoadFunction(handle, "plugin_init");
        if (plugin_init != NULL) {
            PluginInfo info;
            try {
                info = plugin_init(const_cast<const PluginFunctions*>(&function_map), path);
            } catch (std::exception &e) {
                failures[path] = e.what();
                printf("Failed to load plugin at %s: %s\n", astr(path).c_str(), e.what());
                SDL_UnloadObject(handle);
                continue;
            }
            if (!info.failureReason.empty()) {
                failures[path] = info.failureReason;
                printf("Failed to load plugin at %s: %s\n", astr(path).c_str(), info.failureReason.c_str());
                SDL_UnloadObject(handle);
                continue;
            }
            if (info.abi_version != PLUGIN_VERSION || info.minimum_structure_version > function_map.structure_version) {
                failures[path] = "CraftOS-PC version too old";
                printf("Failed to load plugin at %s: This plugin requires a newer version of CraftOS-PC\n", astr(path).c_str());
                SDL_UnloadObject(handle);
                continue;
            }
            loadedPlugins[path] = std::make_pair(handle, info);
        } else loadedPlugins[path] = std::make_pair(handle, PluginInfo());
    }
    return failures;
}

void loadPlugins(Computer * comp) {
    for (const auto& p : loadedPlugins) {
        lua_CFunction luaopen;
        std::string api_name;
        if (!p.second.second.apiName.empty()) api_name = p.second.second.apiName;
        else {
            size_t pos = p.first.find_last_of(WS("/\\"));
            if (pos == std::string::npos) pos = 0; else pos++;
            api_name = astr(p.first.substr(pos).substr(0, p.first.substr(pos).find_first_of('.')));
        }
        if (!p.second.second.luaopenName.empty()) luaopen = (lua_CFunction)SDL_LoadFunction(p.second.first, p.second.second.luaopenName.c_str());
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
        // todo: pcall this
        lua_call(comp->L, 1, 1);
        lua_setglobal(comp->L, api_name.c_str());
    }
}

void deinitializePlugins() {
    for (const auto& p : loadedPlugins) {
        const auto plugin_deinit = (void(*)())SDL_LoadFunction(p.second.first, "plugin_deinit");
        if (plugin_deinit != NULL) plugin_deinit();
        SDL_UnloadObject(p.second.first);
    }
    loadedPlugins.clear();
}