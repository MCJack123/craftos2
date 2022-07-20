/*
 * ccemux.cpp
 * CraftOS-PC 2
 * 
 * This file creates a new CCEmuX API for backwards-compatibility with CCEmuX
 * programs when run in CraftOS-PC.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <CraftOS-PC.hpp>
#include <chrono>
#include <string>
#include <SDL2/SDL.h>
#ifdef _WIN32
#include <windows.h>
#define COMP_DIR L"\\computer\\"
#else
#include <stdlib.h>
#define COMP_DIR "/computer/"
#endif

static const PluginFunctions * functions;

/*
 * The following files were borrowed from CCEmuX, which is licensed under the MIT license.
 *
 * MIT License
 * 
 * Copyright (c) 2018 CLGD
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

static FileEntry emuROM = {{"programs", {{"emu.lua", "local args = { ... }\n\
\n\
if ccemux then\n\
    local function help()\n\
        print(\"Usages:\")\n\
        print(\"emu close - close this computer\")\n\
        print(\"emu open [id] - open another computer\")\n\
        print(\"emu data [id] - opens the data folder\")\n\
        print(\"emu config - opens the config editor\")\n\
        print(\"Run 'help emu' for additional information\")\n\
    end\n\
\n\
    if #args == 0 then\n\
        help()\n\
    else\n\
        if args[1] == \"close\" then\n\
            ccemux.closeEmu()\n\
        elseif args[1] == \"open\" then\n\
            local id = ccemux.openEmu(tonumber(args[2]))\n\
            if id then print(\"Opened computer ID \" .. id)\n\
            else print(\"Could not open computer\")\n\
        elseif args[1] == \"data\" then\n\
            local id = nil\n\
            if args[2] ~= nil then\n\
                id = tonumber(args[2])\n\
                if id == nil then\n\
                    printError(\"Expected a computer ID\")\n\
                    return\n\
                end\n\
            end\n\
            if ccemux.openDataDir(id) then\n\
                print(\"Opened data folder\")\n\
            else\n\
                print(\"Unable to open data folder\")\n\
            end\n\
        elseif args[1] == \"config\" then\n\
            local ok, err = ccemux.openConfig()\n\
            if ok then\n\
                print(\"Opened config editor\")\n\
            else\n\
                print(err)\n\
            end\n\
        else\n\
            printError(\"Unrecognized subcommand: \" .. args[1])\n\
            help()\n\
        end\n\
    end\n\
else\n\
    printError(\"CCEmuX API is disabled or unavailable.\")\n\
end"}}}, {"help", {{"emu.txt", "USAGE\n\
\n\
* emu close\n\
Closes the current emulated computer, without affecting the others. Can be called from within programs via ccemux.closeEmu().\n\
\n\
* emu open [id]\n\
Opens a new emulated computer, with the given ID (if specified) or with the next ID. Can be called from within programs via ccemux.openEmu() or ccemux.openEmu(id)\n\
\n\
* emu data\n\
This will open the CraftOS-PC data dir (where config files and computer save folders are stored) in your default file browser. Can be called from within programs via ccemux.openDataDir(). Note that it may fail on some Linux systems (i.e. if you don't have a DE installed)\n\
\n\
* emu config\n\
Opens an interface to edit the CraftOS-PC configuration. Note that not all rendering backends support this."}}}, {"autorun", {{"emu.lua", "-- Setup completion functions\n\
local function completeMultipleChoice(text, options, addSpaces)\n\
    local tResults = {}\n\
    for n = 1, #options do\n\
        local sOption = options[n]\n\
        if #sOption + (addSpaces and 1 or 0) > #text and sOption:sub(1, #text) == text then\n\
            local sResult = sOption:sub(#text + 1)\n\
            if addSpaces then\n\
                table.insert(tResults, sResult .. \" \")\n\
            else\n\
                table.insert(tResults, sResult)\n\
            end\n\
        end\n\
    end\n\
    return tResults\n\
end\n\
\n\
local commands = { \"close\", \"open\", \"data\", \"config\" }\n\
shell.setCompletionFunction(\"rom/programs/emu.lua\", function(shell, index, text, previous)\n\
    if index == 1 then\n\
        return completeMultipleChoice(text, commands, true)\n\
    end\n\
end)"}}}};

static int ccemux_getVersion(lua_State *L) {
    lua_pushstring(L, functions->craftos_pc_version);
    return 1;
}

static int ccemux_openEmu(lua_State *L) {
    Computer * comp = get_comp(L);
    int id = 0;
    if (lua_isnumber(L, 1)) id = (int)lua_tointeger(L, 1);
    else if (!lua_isnoneornil(L, 1)) luaL_typerror(L, 1, "number");
    else {
        std::lock_guard<std::mutex> lock(comp->peripherals_mutex);
        while (functions->getComputerById(id) != NULL) id++;
    }
    if (functions->attachPeripheral(comp, "computer_" + std::to_string(id), "computer", NULL, "") == NULL) lua_pushnil(L);
    else lua_pushinteger(L, id);
    return 1;
}

static int ccemux_closeEmu(lua_State *L) {
    get_comp(L)->running = 0;
    return 0;
}

static int ccemux_openDataDir(lua_State *L) {
    Computer *comp = get_comp(L);
    const path_t p = luaL_optinteger(L, 1, comp->id) == comp->id ? comp->dataDir : functions->getBasePath() + COMP_DIR + to_path_t(lua_tointeger(L, 1));
#ifdef WIN32
    ShellExecuteW(NULL, L"explore", p.c_str(), NULL, NULL, SW_SHOW);
    lua_pushboolean(L, true);
#elif defined(__APPLE__)
    system(("open '" + p + "'").c_str());
    lua_pushboolean(L, true);
#elif defined(__linux__)
    system(("xdg-open '" + p + "'").c_str());
    lua_pushboolean(L, true);
#else
    lua_pushboolean(L, false);
#endif
    return 1;
}

static int ccemux_openConfig(lua_State *L) {
#ifdef WIN32
    ShellExecuteW(NULL, L"open", (functions->getBasePath() + L"/config/global.json").c_str(), NULL, NULL, SW_SHOW);
    lua_pushboolean(L, true);
#elif defined(__APPLE__)
    system(("open '" + functions->getBasePath() + "/config/global.json'").c_str());
    lua_pushboolean(L, true);
#elif defined(__linux__)
    system(("xdg-open '" + functions->getBasePath() + "/config/global.json'").c_str());
    lua_pushboolean(L, true);
#else
    lua_pushboolean(L, false);
#endif
    return 1;
}

static int ccemux_milliTime(lua_State *L) {
    lua_pushinteger(L, std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count());
    return 1;
}

static int ccemux_nanoTime(lua_State *L) {
    lua_pushinteger(L, std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()).time_since_epoch().count());
    return 1;
}

static int ccemux_echo(lua_State *L) {
    printf("%s\n", luaL_checkstring(L, 1));
    return 0;
}

static int ccemux_setClipboard(lua_State *L) {
    SDL_SetClipboardText(luaL_checkstring(L, 1));
    return 0;
}

static int ccemux_attach(lua_State *L) {
    std::string side = luaL_checkstring(L, 1);
    std::string type = luaL_checkstring(L, 2);
    if (type == "disk_drive") type = "drive";
    else if (type == "wireless_modem") type = "modem"; 
    lua_remove(L, 1); lua_remove(L, 1);
    std::string err;
    functions->attachPeripheral(get_comp(L), side, type, &err, "L", L);
    if (!err.empty()) luaL_error(L, "%s", err.c_str());
    return 0;
}

static int ccemux_detach(lua_State *L) {
    functions->detachPeripheral(get_comp(L), luaL_checkstring(L, 1));
    return 0;
}

static struct luaL_reg M[] = {
    {"getVersion", ccemux_getVersion},
    {"openEmu", ccemux_openEmu},
    {"closeEmu", ccemux_closeEmu},
    {"openDataDir", ccemux_openDataDir},
    {"openConfig", ccemux_openConfig},
    {"milliTime", ccemux_milliTime},
    {"nanoTime", ccemux_nanoTime},
    {"echo", ccemux_echo},
    {"setClipboard", ccemux_setClipboard},
    {"attach", ccemux_attach},
    {"detach", ccemux_detach},
    {NULL, NULL}
};

static PluginInfo info("ccemux", 3);

extern "C" {
DLLEXPORT int luaopen_ccemux(lua_State *L) {
    luaL_register(L, lua_tostring(L, 1), M);
    functions->addVirtualMount(get_comp(L), emuROM, "/rom");
    return 1;
}

DLLEXPORT PluginInfo * plugin_init(const PluginFunctions * func, const path_t& path) {
    functions = func;
    return &info;
}
}
