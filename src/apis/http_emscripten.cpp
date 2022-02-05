/*
 * apis/http_emscripten.cpp
 * CraftOS-PC 2
 * 
 * This file implements the http API for the Emscripten platform, which does not
 * support the standard HTTP implementation using Poco.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#define CRAFTOSPC_INTERNAL
#ifdef __EMSCRIPTEN__
#include "../util.hpp"
#include "../runtime.hpp"
#include "../platform.hpp"
#include <unordered_map>
#include <thread>
#include <emscripten/emscripten.h>
#include <emscripten/fetch.h>


struct http_check_t {
    std::string url;
    std::string status;
};

struct http_data_t {
    bool isBinary;
    Computer * comp;
    char** headers;
};

struct http_param_t {
    Computer *comp;
    std::string url;
    std::string postData;
    std::unordered_map<std::string, std::string> headers;
    std::string method;
    std::string old_url;
    bool isBinary;
    bool redirect;
};

void HTTPDownload(std::string url, std::function<void(std::istream&)> callback) {

}

int http_handle_free(lua_State *L) {
    delete (emscripten_fetch_t**)lua_touserdata(L, lua_upvalueindex(1));
    return 0;
}

int http_handle_close(lua_State *L) {
    emscripten_fetch_t ** handle = (emscripten_fetch_t**)lua_touserdata(L, lua_upvalueindex(1));
    emscripten_fetch_free_unpacked_response_headers(((http_data_t*)(*handle)->userData)->headers);
    delete (http_data_t*)(*handle)->userData;
    emscripten_fetch_close(*handle);
    *handle = NULL;
    return 0;
}

#define checkChar(c) c

int http_handle_readAll(lua_State *L) {
    emscripten_fetch_t ** handle = (emscripten_fetch_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (*handle == NULL || (*handle)->dataOffset >= (*handle)->numBytes) return 0;
    lua_pushlstring(L, (*handle)->data, (*handle)->numBytes);
    (*handle)->dataOffset = (*handle)->numBytes;
    return 1;
}

int http_handle_readLine(lua_State *L) {
    emscripten_fetch_t ** handle = (emscripten_fetch_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (*handle == NULL || (*handle)->dataOffset >= (*handle)->numBytes) return 0;
    std::string line;
    for (; (*handle)->dataOffset < (*handle)->numBytes && (*handle)->data[(*handle)->dataOffset] != '\n'; (*handle)->dataOffset++) line.push_back((*handle)->data[(*handle)->dataOffset]);
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    lua_pushstring(L, line.c_str());
    return 1;
}

int http_handle_readChar(lua_State *L) {
    emscripten_fetch_t ** handle = (emscripten_fetch_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (*handle == NULL || (*handle)->dataOffset >= (*handle)->numBytes) return 0;
    lua_pushlstring(L, &(*handle)->data[(*handle)->dataOffset++], 1);
    return 1;
}

int http_handle_readByte(lua_State *L) {
    emscripten_fetch_t ** handle = (emscripten_fetch_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (*handle == NULL || (*handle)->dataOffset >= (*handle)->numBytes) return 0;
    if (!lua_isnumber(L, 1)) {
        lua_pushinteger(L, (*handle)->data[(*handle)->dataOffset++]);
    } else {
        int c = lua_tointeger(L, 1);
        if ((*handle)->numBytes - (*handle)->dataOffset > c) c = (*handle)->numBytes - (*handle)->dataOffset;
        char * retval = new char[c];
        memcpy(retval, &(*handle)->data[(*handle)->dataOffset], c);
        (*handle)->dataOffset += c;
        lua_pushlstring(L, retval, c);
    }
    return 1;
}

int http_handle_getResponseCode(lua_State *L) {
    emscripten_fetch_t ** handle = (emscripten_fetch_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (*handle == NULL) return 0;
    lua_pushinteger(L, (*handle)->status);
    return 1;
}

int http_handle_getResponseHeaders(lua_State *L) {
    emscripten_fetch_t ** handle = (emscripten_fetch_t**)lua_touserdata(L, lua_upvalueindex(1));
    if (*handle == NULL) return 0;
    char** headers = ((http_data_t*)(*handle)->userData)->headers;
    lua_newtable(L);
    for (int i = 0; headers[i] != NULL; i+=2) {
        for (int j = 0, last_c = '-'; headers[i][j]; j++) {
            if (last_c == '-' && headers[i][j] >= 'a' && headers[i][j] <= 'z') headers[i][j] += ('A' - 'a');
            last_c = headers[i][j];
        }
        for (int j = 0; headers[i+1][j]; j++) if (headers[i+1][j] == '\r' || headers[i+1][j] == '\n') headers[i+1][j] = 0;
        lua_pushstring(L, headers[i]);
        if (headers[i+1][0] == ' ') lua_pushstring(L, &headers[i+1][1]);
        else lua_pushstring(L, headers[i+1]);
        lua_settable(L, -3);
    }
    return 1;
}

std::string http_success(lua_State *L, void* data) {
    emscripten_fetch_t ** handle = new emscripten_fetch_t*;
    *handle = (emscripten_fetch_t*)data;
    luaL_checkstack(L, 30, "Unable to allocate HTTP handle");
    lua_pushstring(L, &(*handle)->url[strlen("https://4eqvr0r8wh.execute-api.us-east-2.amazonaws.com/")]);
    lua_newtable(L);

    lua_pushstring(L, "close");
    lua_pushlightuserdata(L, handle);
    lua_newtable(L);
    lua_pushstring(L, "__gc");
    lua_pushlightuserdata(L, handle);
    lua_pushcclosure(L, http_handle_free, 1);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);
    lua_pushcclosure(L, http_handle_close, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "readAll");
    lua_pushlightuserdata(L, handle);
    lua_pushcclosure(L, http_handle_readAll, 1);
    lua_settable(L, -3);

    if (!((http_data_t*)(*handle)->userData)->isBinary) {
        lua_pushstring(L, "readLine");
        lua_pushlightuserdata(L, handle);
        lua_pushcclosure(L, http_handle_readLine, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "read");
        lua_pushlightuserdata(L, handle);
        lua_pushcclosure(L, http_handle_readChar, 1);
        lua_settable(L, -3);
    } else {
        lua_pushstring(L, "read");
        lua_pushlightuserdata(L, handle);
        lua_pushcclosure(L, http_handle_readByte, 1);
        lua_settable(L, -3);
    }

    lua_pushstring(L, "getResponseCode");
    lua_pushlightuserdata(L, handle);
    lua_pushcclosure(L, http_handle_getResponseCode, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "getResponseHeaders");
    lua_pushlightuserdata(L, handle);
    lua_pushcclosure(L, http_handle_getResponseHeaders, 1);
    lua_settable(L, -3);
    return "http_success";
}

std::string http_failure(lua_State *L, void* data) {
    lua_pushstring(L, (char*)data);
    delete[] (char*)data;
    return "http_failure";
}

void downloadSucceeded(emscripten_fetch_t *fetch) {
    size_t headers_length = emscripten_fetch_get_response_headers_length(fetch);
    char * headers_str = new char[headers_length+1];
    emscripten_fetch_get_response_headers(fetch, headers_str, headers_length);
    ((http_data_t*)fetch->userData)->headers = emscripten_fetch_unpack_response_headers(headers_str);
    delete[] headers_str;
    fetch->dataOffset = 0;
    queueEvent(((http_data_t*)fetch->userData)->comp, http_success, fetch);
}

void downloadFailed(emscripten_fetch_t *fetch) {
    if (fetch->userData == NULL) return;
    char * url = new char[strlen(fetch->url)-strlen("https://4eqvr0r8wh.execute-api.us-east-2.amazonaws.com/")+1];
    strcpy(url, &fetch->url[strlen("https://4eqvr0r8wh.execute-api.us-east-2.amazonaws.com/")]);
    queueEvent(((http_data_t*)fetch->userData)->comp, http_failure, url);
    delete (http_data_t*)fetch->userData;
    fetch->userData = NULL;
    emscripten_fetch_close(fetch);
}

int http_request(lua_State *L) {
    if (!config.http_enable) {
        lua_pushboolean(L, false);
        return 1;
    }
    luaL_checkstring(L, 1);
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    if (lua_isstring(L, 5) && strlen(lua_tostring(L, 5)) > 31) luaL_error(L, "invalid method '%s'", lua_tostring(L, 5));
    strcpy(attr.requestMethod, lua_isstring(L, 5) ? lua_tostring(L, 5) : (lua_isstring(L, 2) ? "POST" : "GET"));
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
    attr.onsuccess = downloadSucceeded;
    attr.onerror = downloadFailed;
    http_data_t * data = new http_data_t;
    data->isBinary = lua_isboolean(L, 4) && lua_toboolean(L, 4);
    data->comp = get_comp(L);
    attr.userData = data;
    if (lua_isstring(L, 2)) attr.requestData = lua_tolstring(L, 2, &attr.requestDataSize);
    std::unordered_map<std::string, std::string> headers;
    if (lua_istable(L, 3)) {
        lua_pushvalue(L, 3);
        lua_pushnil(L);
        for (int i = 0; lua_next(L, -2); i++) {
            lua_pushvalue(L, -2);
            headers[lua_tostring(L, -1)] = lua_tostring(L, -2);
            lua_pop(L, 2);
        }
        lua_pop(L, 1);
    }
    const char ** header_str = new const char*[headers.size()*2+1];
    int i = 0;
    for (auto p : headers) {
        header_str[i] = p.first.c_str();
        header_str[i+1] = p.second.c_str();
        i+=2;
    }
    header_str[i] = NULL;
    attr.requestHeaders = header_str;
    lua_pushstring(L, "https://4eqvr0r8wh.execute-api.us-east-2.amazonaws.com/");
    lua_pushvalue(L, 1);
    lua_concat(L, 2);
    queueTask([L](void* attr)->void*{return emscripten_fetch((emscripten_fetch_attr_t*)attr, lua_tostring(L, -1));}, &attr);
    delete[] header_str;
    lua_pushboolean(L, true);
    return 1;
}

std::string http_check(lua_State *L, void* data) {
    http_check_t * res = (http_check_t*)data;
    lua_pushlstring(L, res->url.c_str(), res->url.size());
    lua_pushboolean(L, res->status.empty());
    if (res->status.empty()) lua_pushnil(L);
    else lua_pushstring(L, res->status.c_str());
    delete res;
    return "http_check";
}

void* checkThread(void* arg) {
#ifdef __APPLE__
    pthread_setname_np("HTTP Check Thread");
#endif
    http_param_t * param = (http_param_t*)arg;
    std::string status;
    if (param->url.find(':') == std::string::npos) status = "Must specify http or https";
    else if (param->url.find("://") == std::string::npos) status = "URL malformed";
    else if (param->url.substr(0, 7) != "http://" && param->url.substr(0, 8) != "https://") status = "Invalid protocol '" + param->url.substr(0, param->url.find("://")) + "'";
    // Replace this when implementing the HTTP white/blacklist
    else if (param->url.find("192.168.") != std::string::npos || param->url.find("10.0.") != std::string::npos || param->url.find("127.") != std::string::npos || param->url.find("localhost") != std::string::npos) status = "Domain not permitted";
    http_check_t * res = new http_check_t;
    res->url = param->url;
    res->status = status;
    queueEvent(param->comp, http_check, res);
    delete param;
    return NULL;
}

int http_checkURL(lua_State *L) {
    if (!config.http_enable) {
        lua_pushboolean(L, false);
        return 1;
    }
    luaL_checkstring(L, 1);
    http_param_t * param = new http_param_t;
    param->comp = get_comp(L);
    param->url = std::string(lua_tostring(L, 1), lua_strlen(L, 1));
    std::thread th(checkThread, param);
    setThreadName(th, "HTTP Check Thread");
    th.detach();
    lua_pushboolean(L, true);
    return 1;
}

int http_addListener(lua_State *L) {
    return luaL_error(L, "CraftOS-PC Online does not support running HTTP servers.");
}

int http_removeListener(lua_State *L) {
    return luaL_error(L, "CraftOS-PC Online does not support running HTTP servers.");
}

int http_websocket(lua_State *L) {
    return 0;
}

void stopWebsocket(void*n){}

luaL_Reg http_reg[] = {
    {"request", http_request},
    {"checkURL", http_checkURL},
    {"addListener", http_addListener},
    {"removeListener", http_removeListener},
    {"websocket", http_websocket},
    {NULL, NULL}
};

library_t http_lib = {"http", http_reg, nullptr, nullptr};

#endif