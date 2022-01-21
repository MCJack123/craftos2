/*
 * apis.hpp
 * CraftOS-PC 2
 *
 * This file defines the library structures for all CraftOS APIs.
 *
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#ifndef APIS_HPP
#define APIS_HPP
#include "util.hpp"
extern library_t config_lib;
extern library_t fs_lib;
extern library_t http_lib;
#ifndef NO_MOUNTER
extern library_t mounter_lib;
#endif
extern library_t os_lib;
extern library_t periphemu_lib;
extern library_t peripheral_lib;
extern library_t rs_lib;
extern library_t term_lib;
#endif
