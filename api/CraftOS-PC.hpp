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
    unsigned abi_version; ///< The plugin ABI version that is supported by this copy of CraftOS-PC. This version must **exactly** match your plugin's API version. You should check this version before doing anything else.
    unsigned structure_version; ///< The version of the PluginFunctions and Computer structures. Check this version before using any field that isn't available in version 0. This version must be equal to or greater than your plugin's minimum structure version.

    // The following fields are available in API version 10.0. No structure version check is required to use these.

    /**
     * A reference to the variable holding the current renderer.
     */
    const int& selectedRenderer;

    /**
     * Returns the path to the CraftOS-PC data root.
     * @return The path to the CraftOS-PC data root.
     */
    path_t (*getBasePath)();

    /**
     * Returns the path to the ROM.
     * @return The path to the ROM.
     */
    path_t (*getROMPath)();

    /**
     * Returns the library structure for a built-in API.
     * @param name The name of the API to get
     * @return A pointer to the library structure for the selected API
     */
    library_t * (*getLibrary)(const std::string& name);

    /**
     * Returns the computer object for a specific ID.
     * @param id The ID of the computer to get
     * @return The computer object, or NULL if a computer with that ID isn't running
     */
    Computer * (*getComputerById)(int id);

    /**
     * Registers a peripheral with the specified name.
     * @param name The name of the peripheral to register.
     * @param initializer The initialization function that creates the peripheral object.
     * @see peripheral_init The prototype for a peripheral initializer
     */
    void (*registerPeripheral)(const std::string& name, const peripheral_init& initializer);

    /**
     * Registers an SDL event hook to call a function when the specified event occurs.
     * @param type The type of event to listen for
     * @param handler The function to call when the event occurs
     * @param userdata An opaque pointer to pass to the handler function
     * @see sdl_event_handler The prototype for an event handler
     */
    void (*registerSDLEvent)(SDL_EventType type, const sdl_event_handler& handler, void* userdata);

    /**
     * Adds a directory mount to a computer.
     * @param comp The computer to mount on
     * @param real_path The path to the directory to mount
     * @param comp_path The path inside the computer to mount on
     * @param read_only Whether the mount should be read-only
     * @return Whether the mount succeeded
     */
    bool (*addMount)(Computer * comp, const path_t& real_path, const char * comp_path, bool read_only);

    /**
     * Adds a virtual mount to a computer.
     * @param comp The computer to mount on
     * @param vfs The virtual filesystem file entry to mount
     * @param comp_path The path inside the computer to mount on
     * @return Whether the mount succeeded
     */
    bool (*addVirtualMount)(Computer * comp, const FileEntry& vfs, const char * comp_path);

    /**
     * Starts up a computer with the specified ID.
     * @param id The ID of the computer to start
     * @return The Computer object for the new computer
     */
    Computer * (*startComputer)(int id);

    /**
     * Queues a Lua event to be sent to a computer.
     * @param comp The computer to send the event to
     * @param event The event provider function to queue
     * @param userdata An opaque pointer storing any user data for the provider
     * @see event_provider The prototype for the event provider
     */
    void (*queueEvent)(Computer * comp, const event_provider& event, void* userdata);

    /**
     * Runs a function on the main thread, and returns the result from the function.
     * @param func The function to call
     * @param userdata An opaque pointer to pass to the function
     * @param async Whether to run the function asynchronously (if true, returns NULL immediately)
     * @return The value returned from the function, or NULL if async is true
     */
    void* (*queueTask)(const std::function<void*(void*)>& func, void* userdata, bool async);
};

/**
 * The PluginInfo structure is used to hold information about a plugin. This
 * structure is returned by plugin_init to indicate some properties about the
 * plugin. The default values in this structure will not change any
 * functionality - feel free to leave them at their default values, or change
 * them to configure your plugin.
 */
struct PluginInfo {
    unsigned abi_version = PLUGIN_VERSION;  ///< The required ABI version for the plugin. Defaults to the version being built for.
    unsigned minimum_structure_version = 0; ///< The minumum required structure version. Defaults to 0 (works with any version).
    std::string luaopenName = "";           ///< The name of the `luaopen` function. This may be useful to be able to rename the plugin file without breaking `luaopen`.
    std::string failureReason = "";         ///< This can be used to trigger a load failure without throwing an exception. Set this field to any non-blank value to stop loading.
    std::string apiName = "";               ///< The name of the API. This can be used to override the default, which is determined by filename. This will also affect luaopenName if that's not set.
    PluginInfo() = default;
    PluginInfo(std::string api): apiName(api) {}
    static PluginInfo * error(const std::string& err) {
        PluginInfo * info = new PluginInfo;
        info->failureReason = err;
        return info;
    }
};

#endif
