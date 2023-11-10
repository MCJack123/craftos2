/*
 * platform/emscripten.cpp
 * CraftOS-PC 2
 *
 * This file implements functions specific to the Emscripten/WASM platform.
 *
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2023 JackMacWindows.
 */

#ifdef __EMSCRIPTEN__
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../platform.hpp"

void setThreadName(std::thread &t, const std::string& name) {}

void setBasePath(path_t path) {}

void setROMPath(path_t path) {}

path_t getBasePath() {
    return "/user-data";
}

path_t getROMPath() {
    return "/craftos";
}

path_t getPlugInPath() {
    return "/user-data/plugins";
}

path_t getMCSavePath() {
    return "";
}

void updateNow(const std::string& tag_name, const Poco::JSON::Object::Ptr root) {}

void migrateOldData() {}

void copyImage(SDL_Surface* surf, SDL_Window* win) {}

void setupCrashHandler() {}

void setFloating(SDL_Window* win, bool state) {}

void platformExit() {}

void addSystemCertificates(Poco::Net::Context::Ptr context) {}

void unblockInput() {}

#endif