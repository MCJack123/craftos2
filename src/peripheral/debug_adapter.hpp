/*
 * peripheral/debug_adapter.hpp
 * CraftOS-PC 2
 * 
 * This file defines the class for the VS Code debug adapter.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2023 JackMacWindows. 
 */

#ifndef PERIPHERAL_DEBUG_ADAPTER_HPP
#define PERIPHERAL_DEBUG_ADAPTER_HPP
#include "debugger.hpp"
#include <Poco/Net/TCPServer.h>
#include <sstream>

class debug_adapter: public debugger {
    friend class DAPConnection;
    Poco::Net::TCPServer server;
    Poco::Net::StreamSocket * socket = NULL;
public:
    std::stringstream data;
    bool running = true;
    static peripheral * _init(lua_State *L, const char * side) {return new debug_adapter(L, side);}
    static void deinit(peripheral * p) {delete (debug_adapter*)p;}
    virtual destructor getDestructor() const override {return deinit;}
    debug_adapter(lua_State *L, const char * side);
    ~debug_adapter();
    void sendData(const std::string& data);
};

#endif