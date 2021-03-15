/*
 * http.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the http API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#ifdef __EMSCRIPTEN__
#include "http_emscripten.cpp"
#else
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <functional>
#include <Computer.hpp>
#include <configuration.hpp>
#include <Poco/URI.h>
#include <Poco/Version.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPMessage.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPServer.h>
#include "handles/http_handle.hpp"
#include "../platform.hpp"
#include "../runtime.hpp"
#include "../util.hpp"

using namespace Poco::Net;

#ifdef __INTELLISENSE__
#pragma region Client
#endif

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

struct http_handle_t {
    bool closed;
    std::string url;
    HTTPClientSession * session;
    HTTPResponse * handle;
    std::istream * stream;
    bool isBinary;
    std::string failureReason;
    http_handle_t(std::istream * s): stream(s) {}
};

struct http_check_t{
    std::string url;
    std::string status;
};

static std::string http_success(lua_State *L, void* data) {
    http_handle_t * handle = (http_handle_t*)data;
    luaL_checkstack(L, 30, "Unable to allocate HTTP handle");
    lua_pushlstring(L, handle->url.c_str(), handle->url.size());
    lua_createtable(L, 0, 5);

    lua_pushstring(L, "close");
    lua_pushlightuserdata(L, handle);
    lua_createtable(L, 0, 1);
    lua_pushstring(L, "__gc");
    lua_pushlightuserdata(L, handle);
    lua_pushcclosure(L, http_handle_free, 1);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);
    lua_pushcclosure(L, http_handle_close, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "readAll");
    lua_pushlightuserdata(L, handle);
    lua_pushboolean(L, handle->isBinary);
    lua_pushcclosure(L, http_handle_readAll, 2);
    lua_settable(L, -3);

    if (!handle->isBinary) {
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

static std::string http_failure(lua_State *L, void* data) {
    http_handle_t * handle = (http_handle_t*)data;
    luaL_checkstack(L, 30, "Unable to allocate HTTP handle");
    lua_pushlstring(L, handle->url.c_str(), handle->url.size());
    if (!handle->failureReason.empty()) lua_pushstring(L, handle->failureReason.c_str());
    if (handle->stream != NULL) {
        lua_createtable(L, 0, 5);

        lua_pushstring(L, "close");
        lua_pushlightuserdata(L, handle);
        lua_createtable(L, 0, 1);
        lua_pushstring(L, "__gc");
        lua_pushlightuserdata(L, handle);
        lua_pushcclosure(L, http_handle_free, 1);
        lua_settable(L, -3);
        lua_setmetatable(L, -2);
        lua_pushcclosure(L, http_handle_close, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "readAll");
        lua_pushlightuserdata(L, handle);
        lua_pushboolean(L, handle->isBinary);
        lua_pushcclosure(L, http_handle_readAll, 2);
        lua_settable(L, -3);

        if (!handle->isBinary) {
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
    } else {
        delete handle;
    }
    return "http_failure";
}

static std::string http_check(lua_State *L, void* data) {
    http_check_t * res = (http_check_t*)data;
    lua_pushlstring(L, res->url.c_str(), res->url.size());
    lua_pushboolean(L, res->status.empty());
    if (res->status.empty()) lua_pushnil(L);
    else lua_pushstring(L, res->status.c_str());
    delete res;
    return "http_check";
}

static void downloadThread(void* arg) {
#ifdef __APPLE__
    pthread_setname_np("HTTP Download Thread");
#endif
    http_param_t* param = (http_param_t*)arg;
    std::string status;
    if (param->url.find(':') == std::string::npos) status = "Must specify http or https";
    else if (param->url.find("://") == std::string::npos) status = "URL malformed";
    else if (param->url.substr(0, 7) != "http://" && param->url.substr(0, 8) != "https://") status = "Invalid protocol '" + param->url.substr(0, param->url.find("://")) + "'";
    if (!status.empty()) {
        http_handle_t * err = new http_handle_t(NULL);
        err->url = param->url;
        err->failureReason = status;
        queueEvent(param->comp, http_failure, err);
        delete param;
        return;
    }
    Poco::URI uri(param->url);
    if (uri.getHost() == "localhost") uri.setHost("127.0.0.1");
    HTTPClientSession * session;
    if (uri.getScheme() == "http") {
        session = new HTTPClientSession(uri.getHost(), uri.getPort());
    } else if (uri.getScheme() == "https") {
        const Context::Ptr context = new Context(Context::CLIENT_USE, "", Context::VERIFY_NONE, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
        session = new HTTPSClientSession(uri.getHost(), uri.getPort(), context);
    } else {
        http_handle_t * err = new http_handle_t(NULL);
        err->url = param->url;
        err->failureReason = "Invalid protocol '" + uri.getScheme() + "'";
        queueEvent(param->comp, http_failure, err);
        delete param;
        return;
    }
    if (!config.http_proxy_server.empty()) session->setProxy(config.http_proxy_server, config.http_proxy_port);
    HTTPRequest request(!param->method.empty() ? param->method : (!param->postData.empty() ? "POST" : "GET"), uri.getPathAndQuery(), HTTPMessage::HTTP_1_1);
    HTTPResponse * response = new HTTPResponse();
    if (config.http_timeout > 0) session->setTimeout(Poco::Timespan(config.http_timeout * 1000));
    size_t requestSize = param->postData.size();
    for (const auto& h : param->headers) {request.add(h.first, h.second); requestSize += h.first.size() + h.second.size() + 1;}
    if (!request.has("User-Agent")) request.add("User-Agent", "computercraft/" CRAFTOSPC_CC_VERSION " CraftOS-PC/" CRAFTOSPC_VERSION);
    if (!request.has("Accept-Charset")) request.add("Accept-Charset", "UTF-8");
    if (!param->postData.empty()) {
        if (request.getContentLength() == HTTPRequest::UNKNOWN_CONTENT_LENGTH) request.setContentLength(param->postData.size());
        if (request.getContentType() == HTTPRequest::UNKNOWN_CONTENT_TYPE) request.setContentType("application/x-www-form-urlencoded; charset=utf-8");
    }
    if (config.http_max_upload > 0 && requestSize > (unsigned)config.http_max_upload) {
        http_handle_t * err = new http_handle_t(NULL);
        err->url = param->url;
        err->failureReason = "Request body is too large";
        queueEvent(param->comp, http_failure, err);
        delete param;
        delete response;
        delete session;
        return;
    }
    try {
        std::ostream& reqs = session->sendRequest(request);
        if (!param->postData.empty()) reqs.write(param->postData.c_str(), param->postData.size());
        if (reqs.bad() || reqs.fail()) {
            http_handle_t * err = new http_handle_t(NULL);
            err->url = param->url;
            err->failureReason = "Failed to send request";
            queueEvent(param->comp, http_failure, err);
            delete param;
            delete response;
            delete session;
            return;
        }
    } catch (Poco::TimeoutException &e) {
        http_handle_t * err = new http_handle_t(NULL);
        err->url = param->url;
        err->failureReason = "Timed out";
        queueEvent(param->comp, http_failure, err);
        delete param;
        delete response;
        delete session;
        return;
    } catch (Poco::Exception &e) {
        fprintf(stderr, "Error while downloading %s: %s\n", param->url.c_str(), e.message().c_str());
        http_handle_t * err = new http_handle_t(NULL);
        err->url = param->url;
        err->failureReason = e.message();
        queueEvent(param->comp, http_failure, err);
        delete param;
        delete response;
        delete session;
        return;
    }
    http_handle_t * handle;
    try {
        handle = new http_handle_t(&session->receiveResponse(*response));
    } catch (Poco::TimeoutException &e) {
        http_handle_t * err = new http_handle_t(NULL);
        err->url = param->url;
        err->failureReason = "Timed out";
        queueEvent(param->comp, http_failure, err);
        delete param;
        delete response;
        delete session;
        return;
    } catch (Poco::Exception &e) {
        fprintf(stderr, "Error while downloading %s: %s\n", param->url.c_str(), e.message().c_str());
        http_handle_t * err = new http_handle_t(NULL);
        err->url = param->url;
        err->failureReason = e.message();
        queueEvent(param->comp, http_failure, err);
        delete param;
        delete response;
        delete session;
        return;
    }
    if (config.http_max_download > 0 && response->hasContentLength() && response->getContentLength() > config.http_max_download) {
        http_handle_t * err = new http_handle_t(NULL);
        err->url = param->url;
        err->failureReason = "Response is too large";
        queueEvent(param->comp, http_failure, err);
        delete param;
        delete response;
        delete session;
        return;
    }
    handle->session = session;
    handle->handle = response;
    handle->url = param->old_url;
    handle->isBinary = param->isBinary;
    if (param->redirect && handle->handle->getStatus() / 100 == 3 && handle->handle->has("Location")) {
        std::string location = handle->handle->get("Location");
        if (location.find("://") == std::string::npos) {
            if (location[0] == '/') location = uri.getScheme() + "://" + uri.getHost() + location;
            else location = uri.getScheme() + "://" + uri.getHost() + uri.getPath() + "/" + location;
        }
        delete handle->handle;
        delete handle->session;
        delete handle;
        param->url = location;
        return downloadThread(param);
    }
    handle->closed = false;
    if (response->getStatus() >= 400) {
        handle->failureReason = HTTPResponse::getReasonForStatus(response->getStatus());
        queueEvent(param->comp, http_failure, handle);
    } else {
        param->comp->requests_open++;
        queueEvent(param->comp, http_success, handle);
    }
    delete param;
}

void HTTPDownload(const std::string& url, const std::function<void(std::istream*, Poco::Exception*)>& callback) {
    Poco::URI uri(url);
    HTTPSClientSession session(uri.getHost(), uri.getPort(), new Context(Context::CLIENT_USE, "", Context::VERIFY_NONE, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"));
    if (!config.http_proxy_server.empty()) session.setProxy(config.http_proxy_server, config.http_proxy_port);
    HTTPRequest request(HTTPRequest::HTTP_GET, uri.getPathAndQuery(), HTTPMessage::HTTP_1_1);
    HTTPResponse response;
    session.setTimeout(Poco::Timespan(5000000));
    request.add("User-Agent", "CraftOS-PC/" CRAFTOSPC_VERSION " ComputerCraft/" CRAFTOSPC_CC_VERSION);
    try {
        session.sendRequest(request);
        std::istream& stream = session.receiveResponse(response);
        if (response.getStatus() / 100 == 3 && response.has("Location")) 
            return HTTPDownload(response.get("Location"), callback);
        callback(&stream, NULL);
    } catch (Poco::Exception &e) {
        callback(NULL, &e);
    }
}

static void* checkThread(void* arg) {
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

static int http_request(lua_State *L) {
    lastCFunction = __func__;
    if (!config.http_enable) {
        lua_pushboolean(L, false);
        return 1;
    }
    if (!lua_isstring(L, 1) && !lua_istable(L, 1)) luaL_typerror(L, 1, "string or table");
    http_param_t * param = new http_param_t;
    param->comp = get_comp(L);
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "url");
        if (!lua_isstring(L, -1)) {delete param; return luaL_error(L, "bad field 'url' (string expected, got %s)", lua_typename(L, lua_type(L, -1)));}
        param->url = lua_tostring(L, -1);
        param->old_url = param->url;
        lua_pop(L, 1);
        lua_getfield(L, 1, "body");
        if (!lua_isnil(L, -1) && !lua_isstring(L, -1)) {delete param; return luaL_error(L, "bad field 'body' (string expected, got %s)", lua_typename(L, lua_type(L, -1)));}
        else if (lua_isstring(L, -1)) param->postData = std::string(lua_tostring(L, -1), lua_strlen(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "binary");
        if (!lua_isnil(L, -1) && !lua_isboolean(L, -1)) {delete param; return luaL_error(L, "bad field 'binary' (boolean expected, got %s)", lua_typename(L, lua_type(L, -1)));}
        else if (lua_isboolean(L, -1)) param->isBinary = lua_toboolean(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 1, "method");
        if (!lua_isnil(L, -1) && !lua_isstring(L, -1)) {delete param; return luaL_error(L, "bad field 'method' (string expected, got %s)", lua_typename(L, lua_type(L, -1)));}
        else if (lua_isstring(L, -1)) param->method = lua_tostring(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 1, "redirect");
        if (!lua_isnil(L, -1) && !lua_isboolean(L, -1)) {delete param; return luaL_error(L, "bad field 'redirect' (boolean expected, got %s)", lua_typename(L, lua_type(L, -1)));}
        else if (lua_isboolean(L, -1)) param->redirect = lua_toboolean(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 1, "headers");
        if (!lua_isnil(L, -1) && !lua_istable(L, -1)) {delete param; return luaL_error(L, "bad field 'headers' (table expected, got %s)", lua_typename(L, lua_type(L, -1)));}
        else if (lua_istable(L, -1)) {
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                param->headers[lua_tostring(L, -2)] = lua_tostring(L, -1);
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    } else {
        param->url = lua_tostring(L, 1);
        param->old_url = param->url;
        param->isBinary = false;
        if (lua_isstring(L, 2)) param->postData = std::string(lua_tostring(L, 2), lua_strlen(L, 2));
        if (lua_istable(L, 3)) {
            lua_pushvalue(L, 3);
            lua_pushnil(L);
            for (int i = 0; lua_next(L, -2); i++) {
                lua_pushvalue(L, -2);
                param->headers[lua_tostring(L, -1)] = lua_tostring(L, -2);
                lua_pop(L, 2);
            }
            lua_pop(L, 1);
        }
        if (lua_isboolean(L, 4)) param->isBinary = lua_toboolean(L, 4);
        if (lua_isstring(L, 5)) param->method = lua_tostring(L, 5);
        param->redirect = !lua_isboolean(L, 6) || lua_toboolean(L, 6);
    }
    std::thread th(downloadThread, param);
    setThreadName(th, "HTTP Request Thread");
    th.detach();
    lua_pushboolean(L, 1);
    return 1;
}

static int http_checkURL(lua_State *L) {
    lastCFunction = __func__;
    if (!config.http_enable) {
        lua_pushboolean(L, false);
        return 1;
    }
    luaL_checkstring(L, 1);
    http_param_t * param = new http_param_t;
    param->comp = get_comp(L);
    param->url = lua_tostring(L, 1);
    std::thread th(checkThread, param);
    setThreadName(th, "HTTP Check Thread");
    th.detach();
    lua_pushboolean(L, true);
    return 1;
}

#ifdef __INTELLISENSE__
#pragma endregion
#pragma region Server
#endif

struct http_res {
    std::string body;
    HTTPServerResponse * res;
};

struct http_request_data {
    int port;
    bool closed;
    HTTPServerRequest * req;
    http_res * res;
};

struct http_server_data {
    int port;
    Computer * comp;
};

static std::string http_request_event(lua_State *L, void* userp) {
    http_request_data* data = (http_request_data*)userp;
    bool* closed = &data->closed;
    *closed = false;
    lua_pushinteger(L, data->port);
    lua_createtable(L, 0, 7);

    lua_pushstring(L, "read");
    lua_pushlightuserdata(L, data->req);
    lua_pushlightuserdata(L, closed);
    lua_pushcclosure(L, req_read, 2);
    lua_settable(L, -3);

    lua_pushstring(L, "readLine");
    lua_pushlightuserdata(L, data->req);
    lua_pushlightuserdata(L, closed);
    lua_pushcclosure(L, req_readLine, 2);
    lua_settable(L, -3);

    lua_pushstring(L, "readAll");
    lua_pushlightuserdata(L, data->req);
    lua_pushlightuserdata(L, closed);
    lua_pushcclosure(L, req_readAll, 2);
    lua_settable(L, -3);

    lua_pushstring(L, "close");
    lua_pushlightuserdata(L, data->req);
    lua_pushlightuserdata(L, closed);
    lua_createtable(L, 0, 1);
    lua_pushstring(L, "__gc");
    lua_pushlightuserdata(L, closed);
    lua_pushcclosure(L, req_free, 1);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);
    lua_pushcclosure(L, req_close, 2);
    lua_settable(L, -3);

    lua_pushstring(L, "getURL");
    lua_pushlightuserdata(L, data->req);
    lua_pushlightuserdata(L, closed);
    lua_pushcclosure(L, req_getURL, 2);
    lua_settable(L, -3);

    lua_pushstring(L, "getMethod");
    lua_pushlightuserdata(L, data->req);
    lua_pushlightuserdata(L, closed);
    lua_pushcclosure(L, req_getMethod, 2);
    lua_settable(L, -3);

    lua_pushstring(L, "getRequestHeaders");
    lua_pushlightuserdata(L, data->req);
    lua_pushlightuserdata(L, closed);
    lua_pushcclosure(L, req_getRequestHeaders, 2);
    lua_settable(L, -3);

    lua_createtable(L, 0, 5);

    lua_pushstring(L, "write");
    lua_pushlightuserdata(L, data->res);
    lua_pushlightuserdata(L, closed);
    lua_pushcclosure(L, res_write, 2);
    lua_settable(L, -3);

    lua_pushstring(L, "writeLine");
    lua_pushlightuserdata(L, data->res);
    lua_pushlightuserdata(L, closed);
    lua_pushcclosure(L, res_writeLine, 2);
    lua_settable(L, -3);

    lua_pushstring(L, "close");
    lua_pushlightuserdata(L, data->res);
    lua_pushlightuserdata(L, closed);
    lua_pushcclosure(L, res_close, 2);
    lua_settable(L, -3);

    lua_pushstring(L, "setStatusCode");
    lua_pushlightuserdata(L, data->res);
    lua_pushlightuserdata(L, closed);
    lua_pushcclosure(L, res_setStatusCode, 2);
    lua_settable(L, -3);

    lua_pushstring(L, "setResponseHeader");
    lua_pushlightuserdata(L, data->res);
    lua_pushlightuserdata(L, closed);
    lua_pushcclosure(L, res_setResponseHeader, 2);
    lua_settable(L, -3);

    return "http_request";
}

class HTTPListener: HTTPRequestHandler {
public:
    Computer *comp;
    int port;
    HTTPListener(int p, Computer *c): comp(c), port(p) {}
    void handleRequest(HTTPServerRequest& req, HTTPServerResponse& res) override {
        //fprintf(stderr, "Got request: %s\n", req.getURI().c_str());
        http_res lres = {"", &res};
        http_request_data evdata = {port, false, &req, &lres};
        queueEvent(comp, http_request_event, &evdata);
        const std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
        while (!(evdata.closed && res.sent()) && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start).count() < 15) std::this_thread::yield();
        if (!res.sent()) {
            res.setContentLength(lres.body.size());
            res.send().write(lres.body.c_str(), lres.body.size());
        }
    }
    class Factory: HTTPRequestHandlerFactory {
    public:
        Computer* comp;
        int port;
        Factory(Computer * c, int p): comp(c), port(p) {}
        HTTPRequestHandler * createRequestHandler(const HTTPServerRequest &request) override {
            return new HTTPListener(port, comp);
        }
    };
};

static std::unordered_map<unsigned short, HTTPServer*> listeners;

/* export */ void http_server_stop() {
    for (std::pair<unsigned short, HTTPServer *> s : listeners) { s.second->stopAll(true); delete s.second; }
}

static int http_addListener(lua_State *L) {
    lastCFunction = __func__;
    const lua_Integer port_ = (int)luaL_checkinteger(L, 1);
    if (port_ < 0 || port_ > 65535) return 0;
    const unsigned short port = (unsigned short)port_;
    if (listeners.find(port) != listeners.end()) {
        delete listeners[port];
        listeners.erase(port);
    }
    HTTPServer * srv;
    try {
        srv = new HTTPServer((HTTPRequestHandlerFactory*)new HTTPListener::Factory(get_comp(L), port), port, new HTTPServerParams);
    } catch (NetException &e) {
        return luaL_error(L, "Could not open server: %s\n", e.message().c_str());
    } catch (std::exception &e) {
        return luaL_error(L, "Could not open server: %s\n", e.what());
    }
    srv->start();
    listeners[port] = srv;
    return 0;
}

static int http_removeListener(lua_State *L) {
    lastCFunction = __func__;
    const lua_Integer port = luaL_checkinteger(L, 1);
    if (port < 0 || port > 65535 || listeners.find((unsigned short)port) == listeners.end()) return 0;
    delete listeners[(unsigned short)port];
    listeners.erase((unsigned short)port);
    return 0;
}

#ifdef __INTELLISENSE__
#pragma endregion
#pragma region WebSockets
#endif

struct ws_handle {
    bool closed;
    std::string url;
    int externalClosed;
    WebSocket * ws;
};

struct websocket_failure_data {
    std::string url;
    std::string reason;
};

struct ws_message {
    std::string url;
    std::string data;
};

static std::string websocket_failure(lua_State *L, void* userp) {
    websocket_failure_data * data = (websocket_failure_data*)userp;
    if (data->url.empty()) lua_pushnil(L);
    else lua_pushstring(L, data->url.c_str());
    lua_pushstring(L, data->reason.c_str());
    delete data;
    return "websocket_failure";
}

static std::string websocket_closed(lua_State *L, void* userp) {
    char * url = (char*)userp;
    if (url == NULL) lua_pushnil(L);
    else lua_pushstring(L, url);
    delete[] url;
    return "websocket_closed";
}

// WebSocket handle functions
static int websocket_send(lua_State *L) {
    lastCFunction = __func__;
    luaL_checkstring(L, 1);
    if (config.http_max_websocket_message > 0 && lua_strlen(L, 1) > (unsigned)config.http_max_websocket_message) luaL_error(L, "Message is too large");
    ws_handle * ws = (ws_handle*)lua_touserdata(L, lua_upvalueindex(1));
    if (ws->closed) return 0;
    if (ws->ws->sendFrame(lua_tostring(L, 1), lua_strlen(L, 1), (int)WebSocket::FRAME_FLAG_FIN | (int)(lua_toboolean(L, 2) ? WebSocket::FRAME_BINARY : WebSocket::FRAME_TEXT)) < 1) 
        ws->closed = true;
    return 0;
}

static int websocket_close(lua_State *L) {
    lastCFunction = __func__;
    ws_handle * ws = (ws_handle*)lua_touserdata(L, lua_upvalueindex(1));
    ws->closed = true;
    return 0;
}

static int websocket_isOpen(lua_State *L) {
    lastCFunction = __func__;
    lua_pushboolean(L, !((ws_handle*)lua_touserdata(L, lua_upvalueindex(1)))->closed);
    return 1;
}

static int websocket_free(lua_State *L) {
    lastCFunction = __func__;
   ((ws_handle*)lua_touserdata(L, lua_upvalueindex(1)))->closed = true;
    return 0;
}

static const char websocket_receive[] = "local _url, _isOpen = ...\n"
"return function(timeout)\n"
"   local tm\n"
"   if timeout then tm = os.startTimer(timeout) end\n"
"   while true do\n"
"       if not _isOpen() then error('attempt to use a closed file', 2) end\n"
"       local ev, url, param = os.pullEvent()\n"
"       if ev == 'websocket_message' and url == _url then return param\n"
"       elseif ev == 'websocket_closed' and url == _url and not _isOpen() then return nil\n"
"       elseif ev == 'timer' and url == tm then return nil end\n"
"   end\n"
"end";

static std::string websocket_success(lua_State *L, void* userp) {
    ws_handle * ws = (ws_handle*)userp;
    luaL_checkstack(L, 10, "Could not grow stack for websocket_success");
    if (ws->url.empty()) lua_pushnil(L);
    else lua_pushstring(L, ws->url.c_str());
    lua_createtable(L, 0, 4);

    lua_pushstring(L, "close");
    lua_pushlightuserdata(L, ws);
    lua_createtable(L, 0, 1);
    lua_pushstring(L, "__gc");
    lua_pushlightuserdata(L, ws);
    lua_pushcclosure(L, websocket_free, 1);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);
    lua_pushcclosure(L, websocket_close, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "receive");
    luaL_loadstring(L, websocket_receive);
    lua_pushstring(L, ws->url.c_str());
    lua_pushlightuserdata(L, ws);
    lua_pushcclosure(L, websocket_isOpen, 1);
    lua_call(L, 2, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "isOpen");
    lua_pushlightuserdata(L, ws);
    lua_pushcclosure(L, websocket_isOpen, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "send");
    lua_pushlightuserdata(L, ws);
    lua_pushcclosure(L, websocket_send, 1);
    lua_settable(L, -3);

    return "websocket_success";
}

static std::string websocket_message(lua_State *L, void* userp) {
    ws_message * message = (ws_message*)userp;
    if (message->url.empty()) lua_pushnil(L);
    else lua_pushstring(L, message->url.c_str());
    lua_pushlstring(L, message->data.c_str(), message->data.size());
    delete message;
    return "websocket_message";
}

class websocket_server: public HTTPRequestHandler {
public:
    Computer * comp;
    HTTPServer *srv;
    std::unordered_map<std::string, std::string> headers;
    websocket_server(Computer * c, HTTPServer *s, const std::unordered_map<std::string, std::string>& h): comp(c), srv(s), headers(h) {}
    void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response) override {
        WebSocket * ws = NULL;
        try {
            ws = new WebSocket(request, response);
        } catch (NetException &e) {
            websocket_failure_data * data = new websocket_failure_data;
            data->url = "";
            data->reason = e.message();
            queueEvent(comp, websocket_failure, data);
            delete ws;
            if (srv != NULL) { try {srv->stop();} catch (...) {} delete srv; }
            return;
        }
#if POCO_VERSION >= 0x01090100
        if (config.http_max_websocket_message > 0) ws->setMaxPayloadSize(config.http_max_websocket_message);
#endif
        ws_handle * wsh = new ws_handle;
        wsh->closed = false;
        wsh->ws = ws;
        wsh->url = "";
        queueEvent(comp, websocket_success, wsh);
        while (!wsh->closed) {
            Poco::Buffer<char> buf(config.http_max_websocket_message);
            int flags = 0;
            try {
                if (ws->receiveFrame(buf, flags) == 0) {
                    wsh->closed = true;
                    queueEvent(comp, websocket_closed, NULL);
                    break;
                }
            } catch (...) {
                wsh->closed = true;
                queueEvent(comp, websocket_closed, NULL);
                break;
            }
            if (flags & WebSocket::FRAME_OP_CLOSE) {
                wsh->closed = true;
                queueEvent(comp, websocket_closed, NULL);
            } else {
                ws_message * message = new ws_message;
                message->url = "";
                message->data = std::string(buf.begin(), buf.end());
                queueEvent(comp, websocket_message, message);
            }
        }
        try {ws->shutdown();} catch (...) {}
        if (srv != NULL) { try {srv->stop();} catch (...) {} delete srv; }
    }
    class Factory: public HTTPRequestHandlerFactory {
    public:
        Computer *comp;
        HTTPServer *srv = NULL;
        std::unordered_map<std::string, std::string> headers;
        Factory(Computer *c, const std::unordered_map<std::string, std::string>& h): comp(c), headers(h) {}
        HTTPRequestHandler* createRequestHandler(const HTTPServerRequest&) override {
            return new websocket_server(comp, srv, headers);
        }
    };
};

/* export */ void stopWebsocket(void* wsh) {
    ws_handle * handle = (ws_handle*)wsh;
    handle->closed = true; 
    handle->externalClosed = 1;
    handle->ws->shutdown();
    for (int i = 0; handle->externalClosed != 2; i++) {
        if (i % 4 == 0) fprintf(stderr, "Waiting for WebSocket...\n");
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    handle->externalClosed = 3;
}

static void websocket_client_thread(Computer *comp, const std::string& str, const std::unordered_map<std::string, std::string>& headers) {
#ifdef __APPLE__
    pthread_setname_np("WebSocket Client Thread");
#endif
    Poco::URI uri(str);
    HTTPClientSession * cs;
    if (uri.getScheme() == "ws") cs = new HTTPClientSession(uri.getHost(), uri.getPort());
    else if (uri.getScheme() == "wss") cs = new HTTPSClientSession(uri.getHost(), uri.getPort(), new Poco::Net::Context(Poco::Net::Context::CLIENT_USE, "", Poco::Net::Context::VERIFY_NONE, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"));
    else {
        websocket_failure_data * data = new websocket_failure_data;
        data->url = str;
        data->reason = std::string("Invalid scheme '" + uri.getScheme() + "'");
        queueEvent(comp, websocket_failure, data);
        return;
    }
    if (uri.getPathAndQuery().empty()) uri.setPath("/");
    if (!config.http_proxy_server.empty()) cs->setProxy(config.http_proxy_server, config.http_proxy_port);
    HTTPRequest request(HTTPRequest::HTTP_GET, uri.getPathAndQuery(), HTTPMessage::HTTP_1_1);
    request.set("origin", "http://www.websocket.org");
    for (std::pair<std::string, std::string> h : headers) request.set(h.first, h.second);
    if (!request.has("User-Agent")) request.add("User-Agent", "computercraft/" CRAFTOSPC_CC_VERSION " CraftOS-PC/" CRAFTOSPC_VERSION);
    if (!request.has("Accept-Charset")) request.add("Accept-Charset", "UTF-8");
    HTTPResponse response;
    WebSocket* ws;
    try {
        ws = new WebSocket(*cs, request, response);
    } catch (Poco::Net::NetException &e) {
        websocket_failure_data * data = new websocket_failure_data;
        data->url = str;
        data->reason = e.displayText();
        queueEvent(comp, websocket_failure, data);
        return;
    }
    //if (config.http_timeout > 0) ws->setReceiveTimeout(Poco::Timespan(config.http_timeout * 1000));
    ws->setReceiveTimeout(Poco::Timespan(1, 0));
#if POCO_VERSION >= 0x01090100
    if (config.http_max_websocket_message > 0) ws->setMaxPayloadSize(config.http_max_websocket_message);
#endif
    ws_handle * wsh = new ws_handle;
    wsh->closed = false;
    wsh->externalClosed = false;
    wsh->url = str;
    wsh->ws = ws;
    comp->openWebsockets.push_back(wsh);
    queueEvent(comp, websocket_success, wsh);
    char * buf = new char[config.http_max_websocket_message];
    while (!wsh->closed) {
        int flags = 0;
        int res;
        try {
            res = ws->receiveFrame(buf, config.http_max_websocket_message, flags);
            if (res == 0) {
                wsh->closed = true;
                wsh->url = "";
                char * sptr = new char[str.length()+1];
                memcpy(sptr, str.c_str(), str.length());
                sptr[str.length()] = 0;
                queueEvent(comp, websocket_closed, sptr);
                break;
            }
        } catch (Poco::TimeoutException &e) {
            if (wsh->closed) {
                char * sptr = new char[str.length()+1];
                memcpy(sptr, str.c_str(), str.length());
                sptr[str.length()] = 0;
                queueEvent(comp, websocket_closed, sptr);
                break;
            }
            continue;
        } catch (NetException &e) {
            wsh->closed = true;
            wsh->url = "";
            char * sptr = new char[str.length()+1];
            memcpy(sptr, str.c_str(), str.length());
            sptr[str.length()] = 0;
            queueEvent(comp, websocket_closed, sptr);
            break;
        }
        if (flags & WebSocket::FRAME_OP_CLOSE) {
            wsh->closed = true;
            wsh->url = "";
            char * sptr = new char[str.length()+1];
            memcpy(sptr, str.c_str(), str.length());
            sptr[str.length()] = 0;
            queueEvent(comp, websocket_closed, sptr);
            break;
        } else {
            ws_message * message = new ws_message;
            message->url = str;
            message->data = std::string((const char*)buf, res);
            queueEvent(comp, websocket_message, message);
        }
        std::this_thread::yield();
    }
    delete[] buf;
    wsh->url = "";
    try {if (!wsh->externalClosed) ws->shutdown();} catch (...) {}
    for (auto it = comp->openWebsockets.begin(); it != comp->openWebsockets.end(); ++it) {
        if (*it == wsh) {
            comp->openWebsockets.erase(it);
            break;
        }
    }
    if (wsh->externalClosed) {wsh->externalClosed = 2; while (wsh->externalClosed == 2) std::this_thread::sleep_for(std::chrono::milliseconds(500));}
    delete wsh;
    delete cs;
}

static int http_websocket(lua_State *L) {
    lastCFunction = __func__;
    if (!config.http_websocket_enabled) luaL_error(L, "Websocket connections are disabled");
    if (lua_isstring(L, 1)) {
        Computer * comp = get_comp(L);
        if (config.http_max_websockets > 0 && comp->openWebsockets.size() >= (unsigned)config.http_max_websockets) luaL_error(L, "Too many websockets already open");
        std::string url = std::string(lua_tostring(L, 1), lua_strlen(L, 1));
        std::unordered_map<std::string, std::string> headers;
        if (lua_istable(L, 2)) {
            lua_pushvalue(L, 2);
            lua_pushnil(L);
            for (int i = 0; lua_next(L, -2); i++) {
                lua_pushvalue(L, -2);
                headers[std::string(lua_tostring(L, -1), lua_strlen(L, -1))] = std::string(lua_tostring(L, -2), lua_strlen(L, -2));
                lua_pop(L, 2);
            }
            lua_pop(L, 1);
        }
        std::thread th(websocket_client_thread, comp, url, headers);
        setThreadName(th, "WebSocket Client Thread");
        th.detach();
    } else if (!(config.serverMode || config.vanilla) && (lua_isnoneornil(L, 1) || lua_isnumber(L, 1))) {
        std::unordered_map<std::string, std::string> headers;
        if (lua_istable(L, 2)) {
            lua_pushvalue(L, 2);
            lua_pushnil(L);
            for (int i = 0; lua_next(L, -2); i++) {
                lua_pushvalue(L, -2);
                headers[std::string(lua_tostring(L, -1), lua_strlen(L, -1))] = std::string(lua_tostring(L, -2), lua_strlen(L, -2));
                lua_pop(L, 2);
            }
            lua_pop(L, 1);
        } else if (!lua_isnoneornil(L, 2)) luaL_error(L, "bad argument #2 (expected table or nil, got %s)", lua_typename(L, lua_type(L, 2)));
        websocket_server::Factory * f = new websocket_server::Factory(get_comp(L), headers);
        try {f->srv = new HTTPServer(f, lua_isnumber(L, 1) ? lua_tointeger(L, 1) : 80);}
        catch (Poco::Exception& e) {
            fprintf(stderr, "Could not open server: %s\n", e.displayText().c_str());
            lua_pushboolean(L, false);
            lua_pushstring(L, e.displayText().c_str());
            return 2;
        }
        f->srv->start();
    } else luaL_error(L, (config.serverMode || config.vanilla) ? "bad argument #1 (expected string, got %s)" : "bad argument #1 (expected string, number, or nil, got %s)", lua_typename(L, lua_type(L, 1)));
    lua_pushboolean(L, true);
    return 1;
}

#ifdef __INTELLISENSE__
#pragma endregion
#endif

static luaL_Reg http_reg[] = {
    {"request", http_request},
    {"checkURL", http_checkURL},
    {"addListener", http_addListener},
    {"removeListener", http_removeListener},
    {"websocket", http_websocket},
    {NULL, NULL}
};

library_t http_lib = {"http", http_reg, nullptr, nullptr};

#endif // __EMSCRIPTEN__