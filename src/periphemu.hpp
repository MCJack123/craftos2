/*
 * periphemu.hpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the periphemu API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2020 JackMacWindows.
 */

#ifndef PERIPHEMU_HPP
#define PERIPHEMU_HPP
#include "lib.hpp"
extern void registerPeripheral(std::string name, peripheral_init initializer);
extern library_t periphemu_lib;
#endif