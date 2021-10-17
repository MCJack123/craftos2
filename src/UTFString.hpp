/*
 * UTFString.hpp
 * CraftOS-PC 2
 * 
 * This file defines some functions related to UTFStrings.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#include <string>
#include "util.hpp"

#define toUTFString(L, arg) (**(std::u32string**)lua_touserdata(L, arg))

extern std::u32string ansiToUnicode(const std::string& str);
extern std::string unicodeToAnsi(const std::u32string& str);
extern std::u32string UTF8ToUnicode(const std::string& str);
extern std::string unicodeToUTF8(const std::u32string& str);
extern std::u32string& createUTFString(lua_State *L, const std::u32string &str = U"");
extern bool isUTFString(lua_State *L, int idx);
extern int luaopen_UTFString(lua_State *L);
