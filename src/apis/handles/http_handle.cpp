/*
 * apis/handles/http_handle.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for HTTP handles.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef __EMSCRIPTEN__
#include <cstdlib>
#include <codecvt>
#include <locale>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPServerRequest.h>
#include "http_handle.hpp"
#include "../../util.hpp"

using namespace Poco::Net;

struct http_handle_t {
    bool closed;
    std::string url;
    HTTPClientSession * session;
    HTTPResponse * handle;
    std::istream * stream;
    bool isBinary;
    std::string failureReason;
    http_handle_t(std::istream * s) : stream(s) {}
};

struct http_res {
    std::string body;
    HTTPServerResponse * res;
};

int http_handle_free(lua_State *L) {
    lastCFunction = __func__;
    delete (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    return 0;
}

int http_handle_close(lua_State *L) {
    lastCFunction = __func__;
    http_handle_t* handle = (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (handle->closed) return luaL_error(L, "attempt to use a closed file");
    handle->closed = true;
    if (handle->failureReason.empty()) get_comp(L)->requests_open--;
    delete handle->handle;
    delete handle->session;
    return 0;
}

int http_handle_readAll(lua_State *L) {
    lastCFunction = __func__;
    http_handle_t * handle = (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (handle->closed || !handle->stream->good()) return luaL_error(L, "attempt to use a closed file");
    std::string ret;
    char buffer[4096];
    while (handle->stream->read(buffer, sizeof(buffer)))
        ret.append(buffer, sizeof(buffer));
    ret.append(buffer, handle->stream->gcount());
    if (!lua_toboolean(L, lua_upvalueindex(2))) ret.erase(std::remove(ret.begin(), ret.end(), '\r'), ret.end());
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring wstr;
    try {wstr = converter.from_bytes(ret.c_str(), ret.c_str() + ret.length());}
    catch (std::exception & e) {
        fprintf(stderr, "http_handle_readAll: Error decoding UTF-8: %s\n", e.what());
        lua_pushlstring(L, ret.c_str(), ret.length());
        return 1;
    }
    std::string out;
    for (wchar_t c : wstr) {if (c < 256) out += (char)c; else out += '?';}
    lua_pushlstring(L, out.c_str(), out.length());
    return 1;
}

int http_handle_readLine(lua_State *L) {
    lastCFunction = __func__;
    http_handle_t * handle = (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (handle->closed) return luaL_error(L, "attempt to use a closed file");
    if (!handle->stream->good()) return 0;
    std::string line;
    std::getline(*handle->stream, line, '\n');
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring wstr;
    try {wstr = converter.from_bytes(line.c_str(), line.c_str() + line.length());}
    catch (std::exception & e) {
        fprintf(stderr, "http_handle_readLine: Error decoding UTF-8: %s\n", e.what());
        lua_pushlstring(L, line.c_str(), line.length());
        return 1;
    }
    std::string out;
    for (wchar_t c : wstr) {if (c < 256) out += (char)c; else out += '?';}
    lua_pushlstring(L, out.c_str(), out.length());
    return 1;
}

int http_handle_readChar(lua_State *L) {
    lastCFunction = __func__;
    http_handle_t * handle = (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (handle->closed) return luaL_error(L, "attempt to use a closed file");
    if (!handle->stream->good()) return 0;
    uint32_t codepoint;
    const char c = (char)handle->stream->get();
    if (c < 0) {
        if (c & 64) {
            const char c2 = (char)handle->stream->get();
            if (c2 >= 0 || c2 & 64) codepoint = 1U<<31;
            else if (c & 32) {
                const char c3 = (char)handle->stream->get();
                if (c3 >= 0 || c3 & 64) codepoint = 1U<<31;
                else if (c & 16) {
                    if (c & 8) codepoint = 1U<<31;
                    else {
                        const char c4 = (char)handle->stream->get();
                        if (c4 >= 0 || c4 & 64) codepoint = 1U<<31;
                        else codepoint = ((c & 0x7) << 18) | ((c2 & 0x3F) << 12) | ((c3 & 0x3F) << 6) | (c4 & 0x3F);
                    }
                } else {
                    codepoint = ((c & 0xF) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                }
            } else codepoint = ((c & 0x1F) << 6) | (c2 & 0x3F);
        } else codepoint = 1U<<31;
    } else codepoint = (unsigned char)c;
    if (codepoint > 255) {
        lua_pushstring(L, "?");
        return 1;
    }
    char retval[2];
    retval[0] = (char)codepoint;
    lua_pushstring(L, retval);
    return 1;
}

int http_handle_readByte(lua_State *L) {
    lastCFunction = __func__;
    http_handle_t * handle = (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (handle->closed) return luaL_error(L, "attempt to use a closed file");
    if (!handle->stream->good()) return 0;
    if (!lua_isnumber(L, 1)) {
        lua_pushinteger(L, handle->stream->get());
    } else {
        const size_t c = lua_tointeger(L, 1);
        char * retval = new char[c];
        handle->stream->read(retval, c);
        lua_pushlstring(L, retval, c);
    }
    return 1;
}

int http_handle_getResponseCode(lua_State *L) {
    lastCFunction = __func__;
    http_handle_t * handle = (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (handle->closed) return luaL_error(L, "attempt to use a closed file");
    lua_pushinteger(L, handle->handle->getStatus());
    return 1;
}

int http_handle_getResponseHeaders(lua_State *L) {
    lastCFunction = __func__;
    http_handle_t * handle = (http_handle_t*)lua_touserdata(L, lua_upvalueindex(1));
    if (handle->closed) return luaL_error(L, "attempt to use a closed file");
    lua_createtable(L, 0, handle->handle->size());
    for (const auto& h : *handle->handle) {
        lua_pushstring(L, h.first.c_str());
        lua_pushstring(L, h.second.c_str());
        lua_settable(L, -3);
    }
    return 1;
}

int req_read(lua_State *L) {
    lastCFunction = __func__;
    HTTPServerRequest * req = (HTTPServerRequest*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || !req->stream().good()) return luaL_error(L, "attempt to use a closed file");
    char tmp[2];
    tmp[0] = (char)req->stream().get();
    lua_pushstring(L, tmp);
    return 1;
}

int req_readLine(lua_State *L) {
    lastCFunction = __func__;
    HTTPServerRequest * req = (HTTPServerRequest*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || !req->stream().good()) return luaL_error(L, "attempt to use a closed file");
    std::string line;
    std::getline(req->stream(), line);
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    lua_pushstring(L, line.c_str());
    return 1;
}

int req_readAll(lua_State *L) {
    lastCFunction = __func__;
    HTTPServerRequest * req = (HTTPServerRequest*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || !req->stream().good()) return luaL_error(L, "attempt to use a closed file");
    std::string ret;
    char buffer[4096];
    while (req->stream().read(buffer, sizeof(buffer)))
        ret.append(buffer, sizeof(buffer));
    ret.append(buffer, req->stream().gcount());
    ret.erase(std::remove(ret.begin(), ret.end(), '\r'), ret.end());
    lua_pushstring(L, ret.c_str());
    return 1;
}

int req_close(lua_State *L) {
    lastCFunction = __func__;
    return 0;
}

int req_free(lua_State *L) {
    lastCFunction = __func__;
    delete (bool*)lua_touserdata(L, lua_upvalueindex(1));
    return 0;
}

int req_getURL(lua_State *L) {
    lastCFunction = __func__;
    HTTPServerRequest * req = (HTTPServerRequest*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2))) return luaL_error(L, "attempt to use a closed file");
    lua_pushstring(L, req->getURI().c_str());
    return 1;
}

int req_getMethod(lua_State *L) {
    lastCFunction = __func__;
    HTTPServerRequest * req = (HTTPServerRequest*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2))) return luaL_error(L, "attempt to use a closed file");
    lua_pushstring(L, req->getMethod().c_str());
    return 1;
}

int req_getRequestHeaders(lua_State *L) {
    lastCFunction = __func__;
    HTTPServerRequest * req = (HTTPServerRequest*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2))) return luaL_error(L, "attempt to use a closed file");
    lua_createtable(L, 0, req->size());
    for (const auto& h : *req) {
        lua_pushstring(L, h.first.c_str());
        lua_pushstring(L, h.second.c_str());
        lua_settable(L, -3);
    }
    return 1;
}

int res_write(lua_State *L) {
    lastCFunction = __func__;
    struct http_res * res = (http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || res->res->sent()) return luaL_error(L, "attempt to use a closed file");
    size_t len = 0;
    const char * buf = luaL_checklstring(L, 1, &len);
    res->body += std::string(buf, len);
    return 0;
}

int res_writeLine(lua_State *L) {
    lastCFunction = __func__;
    struct http_res * res = (http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || res->res->sent()) return luaL_error(L, "attempt to use a closed file");
    size_t len = 0;
    const char * buf = luaL_checklstring(L, 1, &len);
    res->body += std::string(buf, len);
    res->body += "\n";
    return 0;
}

int res_close(lua_State *L) {
    lastCFunction = __func__;
    struct http_res * res = (http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || res->res->sent()) return luaL_error(L, "attempt to use a closed file");
    const std::string body((const std::string)res->body);
    try {
        res->res->setContentLength(body.size());
        res->res->send().write(body.c_str(), body.size());
    } catch (std::exception &e) {
        *(bool*)lua_touserdata(L, lua_upvalueindex(2)) = true;
        luaL_error(L, "Could not send data: %s", e.what());
    }
    *(bool*)lua_touserdata(L, lua_upvalueindex(2)) = true;
    return 0;
}

int res_setStatusCode(lua_State *L) {
    lastCFunction = __func__;
    struct http_res * res = (http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || res->res->sent()) return luaL_error(L, "attempt to use a closed file");
    res->res->setStatus((HTTPResponse::HTTPStatus)luaL_checkinteger(L, 1));
    return 0;
}

int res_setResponseHeader(lua_State *L) {
    lastCFunction = __func__;
    struct http_res * res = (http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || res->res->sent()) return luaL_error(L, "attempt to use a closed file");
    res->res->set(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
    return 0;
}

#endif // __EMSCRIPTEN__