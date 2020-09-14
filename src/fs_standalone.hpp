/*
 * fs_standalone.hpp
 * CraftOS-PC 2
 * 
 * This file defines a FileEntry class that holds data for a standalone ROM. It
 * also provides variables with the ROM contents if enabled.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef FS_STANDALONE_HPP
#define FS_STANDALONE_HPP
#include <map>
#include <sstream>
#include <string>
#include <algorithm>
#include <codecvt>
#include <locale>

struct FileEntry {
    bool isDir;
    bool error = false;
    std::string data;
    std::map<std::string, FileEntry> dir;
    FileEntry(std::string d): isDir(false), data(d) {}
    FileEntry(const char * d): isDir(false), data(d) {}
    FileEntry(std::map<std::string, FileEntry> d): isDir(true), dir(d) {}
    FileEntry(std::initializer_list<std::map<std::string, FileEntry>::value_type > il): isDir(true), dir(il) {}
    FileEntry(const FileEntry &f) {isDir = f.isDir; if (isDir) dir = f.dir; else data = f.data;}
    FileEntry& operator=(const FileEntry& rhs) {isDir = rhs.isDir; if (isDir) dir = rhs.dir; else data = rhs.data; return *this;}
    FileEntry& operator[](std::string key) {if (!isDir) throw std::runtime_error("Attempted to index a file"); return this->dir.at(key);}
    FileEntry& path(std::string path) { // throws
        std::replace(path.begin(), path.end(), '\\', '/');
        std::stringstream ss(path);
        std::string item;
        FileEntry * retval = this;
        while (std::getline(ss, item, '/')) if (!item.empty() && item != "rom:" && item != "debug:") retval = &(*retval)[item];
        return *retval;
    }
    FileEntry& path(std::wstring path) { // throws
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return this->path(converter.to_bytes(path));
    }
};

#ifdef STANDALONE_ROM
extern FileEntry standaloneROM;
extern FileEntry standaloneDebug;
extern std::string standaloneBIOS;
#endif

#endif