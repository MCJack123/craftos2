/*
 * http.hpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for the http API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2020 JackMacWindows.
 */

#ifndef HTTP_HPP
#define HTTP_HPP
#include "lib.hpp"
#include <functional>
extern void HTTPDownload(std::string url, std::function<void(std::istream&)> callback);
extern library_t http_lib;
#endif