/*
 * mounter.h
 * CraftOS-PC 2
 * 
 * This file defines the methods for the mounter API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#ifndef MOUNTER_H
#define MOUNTER_H
#ifdef __cplusplus
extern "C" {
#endif
#include "lib.h"
#include <stdbool.h>
extern library_t mounter_lib;
extern char * fixpath(const char * path);
extern char * fixpath_Ex(const char * path, bool addExt);
extern bool addMount(const char * real_path, const char * comp_path, bool read_only);
extern bool fixpath_ro(const char * path);
#ifdef __cplusplus
}
#endif
#endif
