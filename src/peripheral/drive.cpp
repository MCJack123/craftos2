/*
 * peripheral/drive.cpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the drive peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "drive.hpp"
#include <sys/stat.h>
#include <dirent.h>
extern "C" {
#include <lauxlib.h>
}

int drive::isDiskPresent(lua_State *L) {
    lua_pushboolean(L, diskType != DISK_TYPE_NONE);
    return 1;
}

int drive::getDiskLabel(lua_State *L) {
    if (diskType == DISK_TYPE_AUDIO) return getAudioTitle(L);
    else if (diskType == DISK_TYPE_MOUNT) {
        lua_pushstring(L, path.substr((path.find_last_of('\\') == std::string::npos ? path.find_last_of('/') : path.find_last_of('\\')) + 1).c_str());
        return 1;
    }
    return 0;
}

int drive::setDiskLabel(lua_State *L) {
    // unimplemented
    return 0;
}

int drive::hasData(lua_State *L) {
    lua_pushboolean(L, diskType == DISK_TYPE_MOUNT || diskType == DISK_TYPE_DISK);
    return 1;
}

int drive::getMountPath(lua_State *L) {
    if (diskType == DISK_TYPE_NONE) return 0;
    lua_pushstring(L, mount_path.c_str());
    return 1;
}

int drive::hasAudio(lua_State *L) {
    lua_pushboolean(L, diskType == DISK_TYPE_AUDIO);
    return 1;
}

int drive::getAudioTitle(lua_State *L) {
    if (diskType != DISK_TYPE_AUDIO) return 0;
    int lastdot = path.find_last_of('.');
    int start = path.find('\\') != std::string::npos ? path.find_last_of('\\') + 1 : path.find_last_of('/') + 1;
    lua_pushstring(L, path.substr(start, lastdot > start ? lastdot - start : INT64_MAX).c_str());
    return 1;
}

int drive::playAudio(lua_State *L) {
    if (diskType != DISK_TYPE_AUDIO) return 0;
    if (music != NULL) stopAudio(L);
    music = Mix_LoadMUS(path.c_str());
    if (music == NULL) printf("Could not load audio: %s\n", Mix_GetError());
    if (Mix_PlayMusic(music, 1) == -1) printf("Could not play audio: %s\n", Mix_GetError());
    return 0;
}

int drive::stopAudio(lua_State *L) {
    if (diskType != DISK_TYPE_AUDIO || music == NULL) return 0;
    if (Mix_PlayingMusic()) Mix_HaltMusic();
    Mix_FreeMusic(music);
    music = NULL;
    return 0;
}

int drive::ejectDisk(lua_State *L) {
    if (diskType == DISK_TYPE_NONE) return 0;
    else if (diskType == DISK_TYPE_AUDIO) stopAudio(L);
    else {
        Computer * computer = get_comp(L);
        for (auto it = computer->mounts.begin(); it != computer->mounts.end(); it++) {
            if (1 == std::get<0>(*it).size() && std::get<0>(*it).front() == mount_path) {
                computer->mounts.erase(it);
                if (mount_path == "disk") usedMounts.erase(0);
                else {
                    int n = std::stoi(mount_path.substr(4)) - 1;
                    for (std::unordered_set<int>::iterator it = usedMounts.begin(); it != usedMounts.end(); it++) {
                        if (*it == n) { usedMounts.erase(*it); break; }
                    }
                }
                break;
            }
        }
    }
    diskType = DISK_TYPE_NONE;
    return 0;
}

int drive::getDiskID(lua_State *L) {
    if (diskType != DISK_TYPE_DISK) return 0;
    lua_pushinteger(L, id);
    return 1;
}

#include <cassert>

int drive::insertDisk(lua_State *L, bool init) {
    int arg = init * 2 + 1;
    if (diskType != DISK_TYPE_NONE) lua_pop(L, ejectDisk(L));
    if (lua_isnumber(L, arg)) {
        id = lua_tointeger(L, arg);
        diskType = DISK_TYPE_DISK;
        int i;
        for (i = 0; usedMounts.find(i) != usedMounts.end(); i++);
        usedMounts.insert(i);
        mount_path = "disk" + (i == 0 ? "" : std::to_string(i + 1));
#ifdef WIN32
        createDirectory((std::string(getBasePath()) + "\\computer\\disk\\" + std::to_string(id)).c_str());
        addMount(get_comp(L), (std::string(getBasePath()) + "\\computer\\disk\\" + std::to_string(id)).c_str(), mount_path.c_str(), false);
#else
        assert(createDirectory((std::string(getBasePath()) + "/computer/disk/" + std::to_string(id)).c_str()) == 0);
        addMount(get_comp(L), (std::string(getBasePath()) + "/computer/disk/" + std::to_string(id)).c_str(), mount_path.c_str(), false);
#endif
    } else if (lua_isstring(L, arg)) {
        path = lua_tostring(L, arg);
        struct stat st;
        if (path.substr(0, 9) == "treasure:") {
#ifdef WIN32
            for (int i = 9; i < path.size(); i++) if (path[i] == '/') path[i] = '\\';
#endif
            path = std::string(getROMPath()) + "\\treasure\\" + path.substr(9);
        }
        if (stat(path.c_str(), &st) != 0) {
            lua_pushfstring(L, "Could not mount: %s", strerror(errno));
            lua_error(L);
        }
        if (S_ISDIR(st.st_mode)) {
            diskType = DISK_TYPE_MOUNT;
            int i;
            for (i = 0; usedMounts.find(i) != usedMounts.end(); i++);
            usedMounts.insert(i);
            mount_path = "disk" + (i == 0 ? "" : std::to_string(i + 1));
            addMount(get_comp(L), path.c_str(), mount_path.c_str(), false);
        } else {
            diskType = DISK_TYPE_AUDIO;
            mount_path.clear();
        }
    } else bad_argument(L, "string or number", arg);
    return 0;
}

void driveInit() {
    Mix_Init(MIX_INIT_FLAC | MIX_INIT_MP3 | MIX_INIT_OGG);
    Mix_OpenAudio(44100, AUDIO_S16, 2, 2048);
}

void driveQuit() {
    Mix_CloseAudio();
    Mix_Quit();
}

drive::drive(lua_State *L, const char * side) {
    if (lua_isstring(L, 3) || lua_isnumber(L, 3)) insertDisk(L, true);
}

drive::~drive() { 
    //?
}

int drive::call(lua_State *L, const char * method) {
    std::string m(method);
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
    else return 0;
}

const char * drive_keys[12] = {
    "isDiskPresent",
    "getDiskLabel",
    "setDiskLabel",
    "hasData",
    "getMountPath",
    "hasAudio",
    "getAudioTitle",
    "playAudio",
    "stopAudio",
    "ejectDisk",
    "getDiskID",
    "insertDisk"
};

std::unordered_set<int> drive::usedMounts;
library_t drive::methods = {"drive", 12, drive_keys, NULL, nullptr, nullptr};