/*
 * ccemux.cpp
 * CraftOS-PC 2
 * 
 * This file creates a new CCEmuX API for backwards-compatibility with CCEmuX
 * programs when run in CraftOS-PC.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2020 JackMacWindows.
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
#define libFunc(lib, name) getLibraryFunction(functions->getLibrary(lib), name)

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
            print(\"Opened computer ID \" .. ccemux.openEmu(tonumber(args[2])))\n\
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

static lua_CFunction getLibraryFunction(library_t * lib, const char * name) {
    for (int i = 0; lib->functions[i].name; i++) if (std::string(lib->functions[i].name) == std::string(name)) return lib->functions[i].func;
    return NULL;
}

static int ccemux_getVersion(lua_State *L) {
    libFunc("os", "about")(L);
    lua_getglobal(L, "string");
    lua_pushstring(L, "match");
    lua_gettable(L, -2);
    lua_pushvalue(L, -3);
    lua_pushstring(L, "^CraftOS%-PC A-c-c-e-l-e-r-a-t-e-d- -v([%d%l%.%-]+)\n");
    lua_call(L, 2, 1);
    return 1;
}

static int ccemux_openEmu(lua_State *L) {
    int id = 1;
    if (lua_isnumber(L, 1)) id = (int)lua_tointeger(L, 1);
    else {
        library_t * plib = functions->getLibrary("peripheral");
        for (; id < 256; id++) { // don't search forever
            lua_pushcfunction(L, getLibraryFunction(plib, "isPresent"));
            lua_pushfstring(L, "computer_%d", id);
            lua_call(L, 1, 1);
            if (!lua_toboolean(L, -1)) {lua_pop(L, 1); break;}
            lua_pop(L, 1);
        }
    }
    lua_pushcfunction(L, libFunc("periphemu", "create"));
    lua_pushinteger(L, id);
    lua_pushstring(L, "computer");
    if (lua_pcall(L, 2, 1, 0) != 0) lua_error(L);
    if (lua_toboolean(L, -1)) lua_pushinteger(L, id);
    else lua_pushnil(L);
    return 1;
}

static int ccemux_closeEmu(lua_State *L) {
    return libFunc("os", "shutdown")(L);
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
    if (lua_isstring(L, 2) && std::string(lua_tostring(L, 2)) == "disk_drive") {
        lua_pushstring(L, "drive");
        lua_replace(L, 2);
    }
    if (lua_isstring(L, 2) && std::string(lua_tostring(L, 2)) == "wireless_modem") {
        lua_pushstring(L, "modem");
        lua_replace(L, 2);
    }
    return libFunc("periphemu", "create")(L);
}

static int ccemux_detach(lua_State *L) {
    return libFunc("periphemu", "remove")(L);
}

static struct luaL_Reg M[] = {
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

static PluginInfo info("ccemux");

extern "C" {
#ifdef _WIN32
_declspec(dllexport)
#endif
int luaopen_ccemux(lua_State *L) {
    luaL_register(L, lua_tostring(L, 1), M);
    functions->addVirtualMount(get_comp(L), emuROM, "/rom");
    return 1;
}

#ifdef _WIN32
_declspec(dllexport)
#endif
PluginInfo * plugin_init(const PluginFunctions * func, const path_t& path) {
    functions = func;
    return &info;
}
}