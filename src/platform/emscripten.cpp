/*
 * platform/emscripten.cpp
 * CraftOS-PC 2
 *
 * This file implements functions specific to the Emscripten/WASM platform.
 *
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#ifdef __EMSCRIPTEN__
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../platform.hpp"

void setThreadName(std::thread &t, const std::string& name) {}

unsigned long long getFreeSpace(const std::string& path) {
    return 1000000;
}

unsigned long long getCapacity(const std::string& path) {
    return 1000000;
}

int createDirectory(const std::string& path) {
    if (mkdir(path.c_str(), 0777) != 0) {
        if (errno == ENOENT && path != "/" && !path.empty()) {
            if (createDirectory(path.substr(0, path.find_last_of('/')).c_str())) return 1;
            mkdir(path.c_str(), 0777);
        } else if (errno != EEXIST) return 1;
    }
    return 0;
}

int removeDirectory(const std::string& path) {
    struct stat statbuf;
    if (!stat(path.c_str(), &statbuf)) {
        if (S_ISDIR(statbuf.st_mode)) {
            DIR *d = opendir(path.c_str());
            int r = -1;
            if (d) {
                struct dirent *p;
                r = 0;
                while (!r && (p=readdir(d))) {
                    /* Skip the names "." and ".." as we don't want to recurse on them. */
                    if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;
                    r = removeDirectory(path + "/" + std::string(p->d_name));
                }
                closedir(d);
            }
            if (!r) r = rmdir(path.c_str());
            return r;
        } else return unlink(path.c_str());
    } else return -1;
}

void setBasePath(const char * path) {}

void setROMPath(const char * path) {}

std::string getBasePath() {
    return "/user-data";
}

std::string getROMPath() {
    return "/craftos";
}

std::string getPlugInPath() {
    return "/user-data/plugins";
}

std::string getMCSavePath() {
    return "";
}

void updateNow(const std::string& tag_name, const Poco::JSON::Object::Ptr root) {}

void migrateOldData() {}

void copyImage(SDL_Surface* surf, SDL_Window* win) {}

void setupCrashHandler() {}

void setFloating(SDL_Window* win, bool state) {}

void platformExit() {}

#endif