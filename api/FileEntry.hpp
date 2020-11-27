/*
 * FileEntry.hpp
 * CraftOS-PC 2
 * 
 * This file defines a FileEntry class that holds data for virtual filesystems,
 * including the standalone ROM.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef CRAFTOS_PC_FILEENTRY_HPP
#define CRAFTOS_PC_FILEENTRY_HPP
#include <algorithm>
#include <codecvt>
#include <locale>
#include <map>
#include <regex>
#include <sstream>
#include <string>

/**
 * The FileEntry structure holds a virtual file or directory of files for use
 * with virtual mounts. A FileEntry can be constructed with either a string
 * value (indicating a file) or a map of strings to FileEntries (indicating a
 * directory).
 *
 * Here's an example of how to construct a FileEntry:
 * @code{.cpp}
FileEntry myVFS = {
    {"dir1", {
        {"file.txt", "This is a test file.\nHello!"},
        {"innerdir", {
            {"innerfile.txt", "This file is inside two directories."}
        }}
    }},
    {"outerfile.txt", "This file is in the root."}
};
 * @endcode
 */
struct FileEntry {
    bool isDir; // Whether the entry is a directory
    bool error = false; // Whether a failure occurred
    std::string data; // If this entry is a file, this contains the file data
    std::map<std::string, FileEntry> dir; // If this entry is a directory, this contains the inner files & directories
    FileEntry(std::string d): isDir(false), data(d) {}  // File constructor
    FileEntry(const char * d): isDir(false), data(d) {} // File constructor
    FileEntry(std::map<std::string, FileEntry> d): isDir(true), dir(d) {} // Directory constructor
    FileEntry(std::initializer_list<std::map<std::string, FileEntry>::value_type > il): isDir(true), dir(il) {} // Directory constructor
    FileEntry(const FileEntry &f) {isDir = f.isDir; if (isDir) dir = f.dir; else data = f.data;} // Copy constructor
    ~FileEntry() = default;
    FileEntry& operator=(const FileEntry& rhs) {isDir = rhs.isDir; if (isDir) dir = rhs.dir; else data = rhs.data; return *this;}
    FileEntry& operator[](std::string key) noexcept(false) {if (!isDir) throw std::runtime_error("Attempted to index a file"); return this->dir.at(key);}
    const FileEntry& operator[](std::string key) const noexcept(false) {if (!isDir) throw std::runtime_error("Attempted to index a file"); return this->dir.at(key);}

    /**
     * Traverses a path string and returns the associated file entry.
     * @param path The path to traverse
     * @return The resulting file entry
     * @throw std::runtime_error If one of the non-final nodes is a file
     * @throw std::out_of_range If one of the nodes doesn't exist
     */
    FileEntry& path(std::string path) noexcept(false) {
        std::replace(path.begin(), path.end(), '\\', '/');
        std::stringstream ss(path);
        std::string item;
        FileEntry * retval = this;
        while (std::getline(ss, item, '/')) if (!item.empty() && !std::regex_match(path, std::regex("\\d+:"))) retval = &(*retval)[item];
        return *retval;
    }
    FileEntry& path(std::wstring path) noexcept(false) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return this->path(converter.to_bytes(path));
    }
    const FileEntry& path(std::string path) const noexcept(false) {
        std::replace(path.begin(), path.end(), '\\', '/');
        std::stringstream ss(path);
        std::string item;
        const FileEntry * retval = this;
        while (std::getline(ss, item, '/')) if (!item.empty() && !std::regex_match(item, std::regex("\\d+:"))) retval = &(*retval)[item];
        return *retval;
    }
    const FileEntry& path(std::wstring path) const noexcept(false) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        return this->path(converter.to_bytes(path));
    }
};

#endif