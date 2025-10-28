/*
 * platform.hpp
 * CraftOS-PC 2
 * 
 * This file defines functions that have implementations that differ based on
 * the platform the program is built for.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#ifndef PLATFORM_HPP
#define PLATFORM_HPP
#include <filesystem>
#include <string>
#include <thread>
#include <lib.hpp>
#include <SDL2/SDL.h>
#include <Poco/JSON/Object.h>
#include <Poco/Net/Context.h>

using path_t = std::filesystem::path;

#ifdef __IPHONEOS__
extern void iOS_SetWindowTitle(SDL_Window * win, const char * title);
#define SDL_SetWindowTitle iOS_SetWindowTitle
#endif

extern void setThreadName(std::thread &t, const std::string& name);
extern void setBasePath(path_t path);
extern void setROMPath(path_t path);
extern path_t getBasePath();
extern path_t getROMPath();
extern path_t getPlugInPath();
extern path_t getMCSavePath();
extern void updateNow(const std::string& tag_name, const Poco::JSON::Object::Ptr root);
extern void migrateOldData();
extern void copyImage(SDL_Surface* surf, SDL_Window* win);
extern void setupCrashHandler();
extern void setFloating(SDL_Window* win, bool state);
extern void platformExit();
extern void addSystemCertificates(Poco::Net::Context::Ptr context);
extern void unblockInput();
extern bool winFolderIsReadOnly(path_t path);
#endif
