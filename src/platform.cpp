/*
 * platform.cpp
 * CraftOS-PC 2
 * 
 * This file controls which platform implementation will be compiled.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#include "platform.hpp"
#ifdef WIN32
#include "platform/win.cpp"
#else
#ifdef __APPLE__
#include "platform/darwin.cpp"
#else
#ifdef __ANDROID__
#include "platform/android.cpp"
#else
#ifdef __linux__
#include "platform/linux.cpp"
#else
#ifdef __EMSCRIPTEN__
#include "platform/emscripten.cpp"
#else
#error Unknown platform
#endif
#endif
#endif
#endif
#endif