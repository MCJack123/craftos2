/*
 * CraftOS-PC.hpp
 * CraftOS-PC 2
 *
 * This file is the main header for plugins to import CraftOS-PC's API.
 *
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef CRAFTOS_PC_HPP
#define CRAFTOS_PC_HPP

#include "Computer.hpp"
#include "FileEntry.hpp"

/**
 * The PluginFunctions structure is used to hold all of the functions that a
 * plugin may use to interact with CraftOS-PC. This structure is passed (as a
 * constant pointer) to the `plugin_init` function.
 *
 * There are two different version fields in this structure. The first,
 * abi_version, is used to determine the version of the ABI. Changes to this
 * version indicate a fundamental incompatibility, and you should not continue
 * loading if this version doesn't match the expected version.
 *
 * The second field, structure_version, is used to determine what functions are
 * exported in both the Computer and PluginFunctions structure. This version is
 * bumped when new fields are added to one of the structure definitions. Check
 * this field before using any fields added after structure version 0. It is
 * not required that this field exactly match - as long as the version is
 * greater than or equal to the minimum that your plugin requires to function,
 * you may continue loading.
 *
 * Do NOT rely on any non-version-0 fields to exist without checking the
 * structure version. If you do this, users using your plugin on an old version
 * of CraftOS-PC will likely experience a segmentation fault/crash when the
 * plugin attempts to load the non-existing function. Instead, you should check
 * that the structure version is compatible, and warn or error that the plugin
 * is incompatible with the current version (without crashing).
 *
 * Do note that if your plugin doesn't require any CraftOS-PC structures during
 * initialization, you can let CraftOS-PC handle version checks by returning an
 * info structure with the required versions filled in (see below). If the
 * version numbers don't match, CraftOS-PC will stop loading your plugin.
 * However, this will not suffice if you need to access the structures in your
 * plugin_init.
 *
 * You may see references to version numbers in the form xx.yy - this is a
 * shorthand form to represent <abi_version>.<structure_version>.
 */
struct PluginFunctions {
    unsigned abi_version; // The plugin ABI version that is supported by this copy of CraftOS-PC. This version must **exactly** match your plugin's API version. You should check this version before doing anything else.
    unsigned structure_version; // The version of the PluginFunctions and Computer structures. Check this version before using any field that isn't available in version 0. This version must be equal to or greater than your plugin's minimum structure version.

    // The following fields are available in API version 10.0. No structure version check is required to use these.
    int& selectedRenderer;
    path_t (*getBasePath)();
    path_t (*getROMPath)();
    library_t * (*getLibrary)(std::string name);
    Computer * (*getComputerById)(int id);
    void (*registerPeripheral)(std::string name, peripheral_init initializer);
    void (*registerSDLEvent)(SDL_EventType type, sdl_event_handler handler, void* userdata);
    bool (*addMount)(Computer * comp, path_t real_path, const char * comp_path, bool read_only);
    bool (*addVirtualMount)(Computer * comp, FileEntry& vfs, const char * comp_path);
    Computer * (*startComputer)(int id);
    void (*queueEvent)(Computer * comp, event_provider p, void* userdata);
    void* (*queueTask)(std::function<void*(void*)> func, void* userdata, bool async);
};

/**
 * The PluginInfo structure is used to hold information about a plugin. This
 * structure is returned by plugin_init to indicate some properties about the
 * plugin. The default values in this structure will not change any
 * functionality - feel free to leave them at their default values, or change
 * them to configure your plugin.
 */
struct PluginInfo {
    unsigned abi_version = PLUGIN_VERSION;  // The required ABI version for the plugin. Defaults to the version being built for.
    unsigned minimum_structure_version = 0; // The minumum required structure version. Defaults to 0 (works with any version).
    std::string luaopenName = "";           // The name of the `luaopen` function. This may be useful to be able to rename the plugin file without breaking `luaopen`.
    std::string failureReason = "";         // This can be used to trigger a load failure without throwing an exception. Set this field to any non-blank value to stop loading.
    std::string apiName = "";               // The name of the API. This can be used to override the default, which is determined by filename. This will also affect luaopenName if that's not set.
};

#endif
