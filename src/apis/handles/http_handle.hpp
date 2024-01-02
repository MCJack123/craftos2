/*
 * apis/handles/http_handle.hpp
 * CraftOS-PC 2
 * 
 * This file defines the methods for HTTP handles.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#ifndef HTTP_HANDLE_HPP
#define HTTP_HANDLE_HPP
extern "C" {
#include <lua.h>
}
struct http_handle_t {
    std::string url;
    std::string failureReason;
    Poco::Net::HTTPClientSession * session;
    Poco::Net::HTTPResponse * handle;
    std::istream * stream;
    http_handle_t(std::istream * s) : stream(s) {}
};
extern int http_handle_free(lua_State *L);
extern int http_handle_close(lua_State *L);
extern int http_handle_readAll(lua_State *L);
extern int http_handle_readLine(lua_State *L);
extern int http_handle_readChar(lua_State *L);
extern int http_handle_readByte(lua_State *L);
extern int http_handle_readAllByte(lua_State *L);
extern int http_handle_getResponseCode(lua_State *L);
extern int http_handle_getResponseHeaders(lua_State *L);
extern int http_handle_seek(lua_State *L);
extern int req_read(lua_State *L);
extern int req_readLine(lua_State *L);
extern int req_readAll(lua_State *L);
extern int req_close(lua_State *L);
extern int req_free(lua_State *L);
extern int req_getURL(lua_State *L);
extern int req_getMethod(lua_State *L);
extern int req_getRequestHeaders(lua_State *L);
extern int res_write(lua_State *L);
extern int res_writeLine(lua_State *L);
extern int res_close(lua_State *L);
extern int res_setStatusCode(lua_State *L);
extern int res_setResponseHeader(lua_State *L);
#endif