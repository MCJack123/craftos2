/*
 * peripheral/drive.cpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the drive peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#include <dirent.h>
#include <sys/stat.h>
#include "drive.hpp"
#include "../platform.hpp"
#include "../runtime.hpp"
#include "../terminal/SDLTerminal.hpp"

int drive::isDiskPresent(lua_State *L) {
    lastCFunction = __func__;
    lua_pushboolean(L, diskType != disk_type::DISK_TYPE_NONE);
    return 1;
}

int drive::getDiskLabel(lua_State *L) {
    lastCFunction = __func__;
    if (diskType == disk_type::DISK_TYPE_AUDIO) return getAudioTitle(L);
    else if (diskType == disk_type::DISK_TYPE_MOUNT) {
        lua_pushstring(L, astr(path.substr((path.find_last_of('\\') == std::string::npos ? path.find_last_of('/') : path.find_last_of('\\')) + 1)).c_str());
        return 1;
    }
    return 0;
}

int drive::setDiskLabel(lua_State *L) {
    // unimplemented
    if (diskType == disk_type::DISK_TYPE_AUDIO) luaL_error(L, "Disk label cannot be changed");
    return 0;
}

int drive::hasData(lua_State *L) {
    lastCFunction = __func__;
    lua_pushboolean(L, diskType == disk_type::DISK_TYPE_MOUNT || diskType == disk_type::DISK_TYPE_DISK);
    return 1;
}

int drive::getMountPath(lua_State *L) {
    lastCFunction = __func__;
    if (diskType == disk_type::DISK_TYPE_NONE) return 0;
    lua_pushstring(L, mount_path.c_str());
    return 1;
}

int drive::hasAudio(lua_State *L) {
    lastCFunction = __func__;
    lua_pushboolean(L, diskType == disk_type::DISK_TYPE_AUDIO);
    return 1;
}

int drive::getAudioTitle(lua_State *L) {
    lastCFunction = __func__;
    if (diskType != disk_type::DISK_TYPE_AUDIO) {
        if (diskType == disk_type::DISK_TYPE_NONE) lua_pushboolean(L, false);
        else lua_pushnil(L);
        return 1;
    }
    const int lastdot = (int)path.find_last_of('.');
    const int start = path.find('\\') != std::string::npos ? (int)path.find_last_of('\\') + 1 : (int)path.find_last_of('/') + 1;
    lua_pushstring(L, astr(path.substr(start, lastdot > start ? lastdot - start : UINT32_MAX)).c_str());
    return 1;
}

int drive::playAudio(lua_State *L) {
    lastCFunction = __func__;
#ifndef NO_MIXER
    if (diskType != disk_type::DISK_TYPE_AUDIO) return 0;
    if (music != NULL) stopAudio(L);
    music = Mix_LoadMUS(astr(path).c_str());
    if (music == NULL) fprintf(stderr, "Could not load audio: %s\n", Mix_GetError());
    if (Mix_PlayMusic(music, 1) == -1) fprintf(stderr, "Could not play audio: %s\n", Mix_GetError());
#endif
    return 0;
}

int drive::stopAudio(lua_State *L) {
    lastCFunction = __func__;
#ifndef NO_MIXER
    if (diskType != disk_type::DISK_TYPE_AUDIO || music == NULL) return 0;
    if (Mix_PlayingMusic()) Mix_HaltMusic();
    Mix_FreeMusic(music);
    music = NULL;
#endif
    return 0;
}

int drive::ejectDisk(lua_State *L) {
    lastCFunction = __func__;
    if (diskType == disk_type::DISK_TYPE_NONE) return 0;
    else if (diskType == disk_type::DISK_TYPE_AUDIO) stopAudio(L);
    else {
        Computer * computer = get_comp(L);
        for (auto it = computer->mounts.begin(); it != computer->mounts.end(); ++it) {
            if (1 == std::get<0>(*it).size() && std::get<0>(*it).front() == mount_path) {
                computer->mounts.erase(it);
                if (mount_path == "disk") computer->usedDriveMounts.erase(0);
                else {
                    const int n = std::stoi(mount_path.substr(4)) - 1;
                    computer->usedDriveMounts.erase(n);
                }
                break;
            }
        }
    }
    diskType = disk_type::DISK_TYPE_NONE;
    return 0;
}

int drive::getDiskID(lua_State *L) {
    lastCFunction = __func__;
    if (diskType != disk_type::DISK_TYPE_DISK) return 0;
    lua_pushinteger(L, id);
    return 1;
}

// thanks Windows
#ifdef _MSC_VER
#pragma optimize("", off)
#endif
int drive::insertDisk(lua_State *L, bool init) {
    lastCFunction = __func__;
    Computer * comp = get_comp(L);
    const int arg = init * 2 + 1;
    const char * error, *errparam;
    if (diskType != disk_type::DISK_TYPE_NONE) lua_pop(L, ejectDisk(L));
    if (lua_isnumber(L, arg)) {
        id = lua_tointeger(L, arg);
        diskType = disk_type::DISK_TYPE_DISK;
        int i;
        for (i = 0; comp->usedDriveMounts.find(i) != comp->usedDriveMounts.end(); i++) {}
        comp->usedDriveMounts.insert(i);
        mount_path = "disk" + (i == 0 ? "" : std::to_string(i + 1));
        comp->mounter_initializing = true;
#ifdef _WIN32
        createDirectory(computerDir + WS("\\disk\\") + to_path_t(id));
        addMount(comp, computerDir + WS("\\disk\\") + to_path_t(id), mount_path.c_str(), false);
#else
        createDirectory((computerDir + "/disk/" + std::to_string(id)).c_str());
        addMount(comp, (computerDir + "/disk/" + std::to_string(id)).c_str(), mount_path.c_str(), false);
#endif
        comp->mounter_initializing = false;
    } else if (lua_isstring(L, arg)) {
        path = wstr(lua_tostring(L, arg));
        struct_stat st;
#ifndef STANDALONE_ROM
        if (path.substr(0, 9) == WS("treasure:")) {
#ifdef _WIN32
            for (size_t i = 9; i < path.size(); i++) if (path[i] == '/') path[i] = '\\';
            path = getROMPath() + WS("\\treasure\\") + path.substr(9);
#else
            path = getROMPath() + WS("/treasure/") + path.substr(9);
#endif
        } else if (path.substr(0, 7) == WS("record:")) {
#ifdef _WIN32
            path = getROMPath() + WS("\\sounds\\minecraft\\sounds\\records\\") + path.substr(7) + WS(".ogg");
#else
            path = getROMPath() + WS("/sounds/minecraft/sounds/records/") + path.substr(7) + WS(".ogg");
#endif
        } else
#endif
        if (path.substr(0, 9) == WS("computer:")) {
            try {std::stoi(path.substr(9));}
            catch (std::invalid_argument &e) {
                if (init) throw std::invalid_argument("Could not mount: Invalid computer ID");
                else {
                    error = "Could not mount: Invalid computer ID";
                    goto throwError;
                }
            }
#ifdef _WIN32
            path = getBasePath() + WS("\\computer\\") + path.substr(9);
#else
            path = getBasePath() + WS("/computer/") + path.substr(9);
#endif
        }
#ifdef NO_MOUNTER
        else {
            if (init) throw std::invalid_argument("Could not mount: Access denied");
            else {
                error = "Could not mount: Permission denied";
                goto throwError;
            }
        }
#endif
        if (platform_stat(path.c_str(), &st) != 0) {
            if (init) throw std::system_error(errno, std::system_category(), "Could not mount: ");
            else {
                error = "Could not mount: %s";
                errparam = strerror(errno);
                goto throwErrorParam;
            }
        }
        if (S_ISDIR(st.st_mode)) {
            diskType = disk_type::DISK_TYPE_MOUNT;
            int i;
            for (i = 0; comp->usedDriveMounts.find(i) != comp->usedDriveMounts.end(); i++) {}
            comp->usedDriveMounts.insert(i);
            mount_path = "disk" + (i == 0 ? "" : std::to_string(i + 1));
            if (!addMount(comp, path, mount_path.c_str(), false)) {
                diskType = disk_type::DISK_TYPE_NONE;
                comp->usedDriveMounts.erase(i);
                mount_path.clear();
                if (init) throw std::runtime_error("Could not mount: Access denied");
                else {
                    error = "Could not mount: Access denied";
                    goto throwError;
                }
            }
        }
#ifndef NO_MIXER
        else {
            diskType = disk_type::DISK_TYPE_AUDIO;
            mount_path.clear();
        }
#else
        else {
            if (init) throw std::invalid_argument("Playing audio is not available in this build");
            else {
                error = "Playing audio is not available in this build";
                goto throwError;
            }
        }
#endif
    } else {
        if (init) throw std::invalid_argument("bad argument (expected string or number)");
        else luaL_typerror(L, arg, "string or number");
    }
    return 0;
    // This dirty hack is because Windows randomly attempts to deallocate a std::wstring
    // in the stack that doesn't exist. (???) The only way to fix it is to make it jump
    // out of scope, and disable optimizations. (TODO: Find if scope escape is required.)
throwError:
    return luaL_error(L, error);
throwErrorParam:
    return luaL_error(L, error, errparam);
}
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

void driveInit() {
#ifndef NO_MIXER
    Mix_Init(MIX_INIT_FLAC | MIX_INIT_MP3 | MIX_INIT_OGG);
    Mix_OpenAudio(48000, AUDIO_S16, 2, 512); // NOTE: If changing the chunk size, update playAudio!
#endif
}

void driveQuit() {
#ifndef NO_MIXER
    Mix_CloseAudio();
    Mix_Quit();
#endif
}

drive::drive(lua_State *L, const char * side) {
    if (lua_isstring(L, 3) || lua_isnumber(L, 3)) insertDisk(L, true);
}

drive::~drive() { 
    stopAudio(NULL);
}

int drive::call(lua_State *L, const char * method) {
    const std::string m(method);
    if (m == "isDiskPresent") return isDiskPresent(L);
    else if (m == "getDiskLabel") return getDiskLabel(L);
    else if (m == "setDiskLabel") return setDiskLabel(L);
    else if (m == "hasData") return hasData(L);
    else if (m == "getMountPath") return getMountPath(L);
    else if (m == "hasAudio") return hasAudio(L);
    else if (m == "getAudioTitle") return getAudioTitle(L);
    else if (m == "playAudio") return playAudio(L);
    else if (m == "stopAudio") return stopAudio(L);
    else if (m == "ejectDisk") return ejectDisk(L);
    else if (m == "getDiskID") return getDiskID(L);
    else if (m == "insertDisk") return insertDisk(L);
    else return luaL_error(L, "No such method");
}

static luaL_Reg drive_reg[] = {
    {"isDiskPresent", NULL},
    {"getDiskLabel", NULL},
    {"setDiskLabel", NULL},
    {"hasData", NULL},
    {"getMountPath", NULL},
    {"hasAudio", NULL},
    {"getAudioTitle", NULL},
    {"playAudio", NULL},
    {"stopAudio", NULL},
    {"ejectDisk", NULL},
    {"getDiskID", NULL},
    {"insertDisk", NULL},
    {NULL, NULL}
};

library_t drive::methods = {"drive", drive_reg, nullptr, nullptr};
