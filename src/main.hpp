/*
 * main.hpp
 * CraftOS-PC 2
 *
 * This file defines variables used on the command line that may be used by
 * other CraftOS-PC components.
 *
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#ifndef MAIN_HPP
#define MAIN_HPP

#include <map>
#include <unordered_map>
#include <Terminal.hpp>
#include "platform.hpp"

extern bool rawClient;
extern std::string overrideHardwareDriver;
extern std::map<uint8_t, Terminal*> rawClientTerminals;
extern std::unordered_map<unsigned, uint8_t> rawClientTerminalIDs;
extern std::string script_file;
extern std::string script_args;
extern int returnValue;
extern std::unordered_map<path_t, std::string> globalPluginErrors;
extern int parseArguments(const std::vector<std::string>& argv);

// These functions are required by main for full initialization of Lua.
extern "C" {
    extern int db_debug(lua_State *L);
    extern int db_breakpoint(lua_State *L);
    extern int db_unsetbreakpoint(lua_State *L);
    extern void setcompmask_(lua_State *L, int mask);
    extern FILE* mounter_fopen_(lua_State *L, const char * filename, const char * mode);
    extern int mounter_fclose_(lua_State *L, FILE * stream);
}

#endif