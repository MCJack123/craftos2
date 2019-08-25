/*
 * http_handle.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for HTTP handles.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "http_handle.hpp"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPClientSession.h>

using Poco::Net::HTTPResponse;
using Poco::Net::HTTPClientSession;

typedef struct {
    const char * key;
    const char * value;
} dict_val_t;

typedef struct {
    size_t size;
    size_t offset;
    char * data;
} buffer_t;

typedef struct {
    bool closed;
    char * url;
    HTTPClientSession * session;
    HTTPResponse * handle;
    std::istream& stream;
} http_handle_t;

int http_handle_free(lua_State *L) {
    delete (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    return 0;
}

int http_handle_close(lua_State *L) {
    http_handle_t* handle = (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (handle->closed) return 0;
    handle->closed = true;
    delete handle->handle;
    delete handle->session;
    free(handle->url);
    return 0;
}

extern char checkChar(char c);

int http_handle_readAll(lua_State *L) {
    http_handle_t * handle = (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (handle->closed || !handle->stream.good()) return 0;
    std::string ret;
    char buffer[4096];
    while (handle->stream.read(buffer, sizeof(buffer)))
        ret.append(buffer, sizeof(buffer));
    ret.append(buffer, handle->stream.gcount());
    lua_pushstring(L, ret.c_str());
    return 1;
}

int http_handle_readLine(lua_State *L) {
    http_handle_t * handle = (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (handle->closed || !handle->stream.good()) return 0;
    std::string line;
    std::getline(handle->stream, line);
    lua_pushstring(L, line.c_str());
    return 1;
}

int http_handle_read(lua_State *L) {
    http_handle_t * handle = (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (handle->closed || !handle->stream.good()) return 0;
    char retval[2];
    retval[0] = checkChar(handle->stream.get());
    lua_pushstring(L, retval);
    return 1;
}

int http_handle_getResponseCode(lua_State *L) {
    http_handle_t * handle = (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (handle->closed) return 0;
    lua_pushinteger(L, handle->handle->getStatus());
    return 1;
}

int http_handle_getResponseHeaders(lua_State *L) {
    http_handle_t * handle = (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (handle->closed) return 0;
    lua_newtable(L);
    for (auto it = handle->handle->begin(); it != handle->handle->end(); it++) {
        lua_pushstring(L, it->first.c_str());
        lua_pushstring(L, it->second.c_str());
        lua_settable(L, -2);
    }
    return 1;
}