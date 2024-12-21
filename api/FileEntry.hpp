// Copyright (c) 2019-2024 JackMacWindows.
// SPDX-FileCopyrightText: 2019-2024 JackMacWindows
//
// SPDX-License-Identifier: MIT

#ifndef CRAFTOS_PC_FILEENTRY_HPP
#define CRAFTOS_PC_FILEENTRY_HPP
#include <algorithm>
#include <codecvt>
#include <filesystem>
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
    FileEntry& path(std::filesystem::path path) noexcept(false) {
        FileEntry * retval = this;
        for (const auto& item : path) if (item.string() != "." && !std::regex_match(item.native(), std::basic_regex<std::filesystem::path::value_type>(std::filesystem::path("\\d+:").native()))) retval = &(*retval)[item.string()];
        return *retval;
    }
    FileEntry& path(std::string path) noexcept(false) {
        return this->path(std::filesystem::path(path));
    }
    FileEntry& path(std::wstring path) noexcept(false) {
        return this->path(std::filesystem::path(path));
    }
    const FileEntry& path(std::filesystem::path path) const noexcept(false) {
        const FileEntry * retval = this;
        for (const auto& item : path) if (item.string() != "." && !std::regex_match(item.native(), std::basic_regex<std::filesystem::path::value_type>(std::filesystem::path("\\d+:").native()))) retval = &(*retval)[item.string()];
        return *retval;
    }
    const FileEntry& path(std::string path) const noexcept(false) {
        return this->path(std::filesystem::path(path));
    }
    const FileEntry& path(std::wstring path) const noexcept(false) {
        return this->path(std::filesystem::path(path));
    }
};

#endif