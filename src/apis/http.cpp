/*
 * http.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the http API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
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
#include "../platform.hpp"
#include "../runtime.hpp"
#include "../util.hpp"
#include "handles/http_handle.hpp"

#ifdef __ANDROID__
extern "C" {extern int Android_JNI_SetupThread(void);}
#endif

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
    bool redirect;
    double timeout;
};

struct http_check_t {
    std::string url;
    std::string status;
};

static std::string http_success(lua_State *L, void* data) {
    http_handle_t * handle = (http_handle_t*)data;
    luaL_checkstack(L, 30, "Unable to allocate HTTP handle");
    pushstring(L, handle->url);

    *(http_handle_t**)lua_newuserdata(L, sizeof(http_handle_t*)) = handle;
    lua_createtable(L, 0, 1);
    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, http_handle_free);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);
    lua_createtable(L, 0, 6);

    lua_pushstring(L, "close");
    lua_pushvalue(L, -3);
    lua_pushcclosure(L, http_handle_close, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "readLine");
    lua_pushvalue(L, -3);
    lua_pushcclosure(L, http_handle_readLine, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "readAll");
    lua_pushvalue(L, -3);
    lua_pushcclosure(L, http_handle_readAllByte, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "read");
    lua_pushvalue(L, -3);
    lua_pushcclosure(L, http_handle_readByte, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "seek");
    lua_pushvalue(L, -3);
    lua_pushcclosure(L, http_handle_seek, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "getResponseCode");
    lua_pushvalue(L, -3);
    lua_pushcclosure(L, http_handle_getResponseCode, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "getResponseHeaders");
    lua_pushvalue(L, -3);
    lua_pushcclosure(L, http_handle_getResponseHeaders, 1);
    lua_settable(L, -3);
    lua_remove(L, -2);
    return "http_success";
}

static std::string http_failure(lua_State *L, void* data) {
    http_handle_t * handle = (http_handle_t*)data;
    luaL_checkstack(L, 30, "Unable to allocate HTTP handle");
    pushstring(L, handle->url);
    if (!handle->failureReason.empty()) lua_pushstring(L, handle->failureReason.c_str());
    if (handle->stream != NULL) {
        *(http_handle_t**)lua_newuserdata(L, sizeof(http_handle_t*)) = handle;
        lua_createtable(L, 0, 1);
        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, http_handle_free);
        lua_settable(L, -3);
        lua_setmetatable(L, -2);

        lua_createtable(L, 0, 6);
        lua_pushstring(L, "close");
        lua_pushvalue(L, -3);
        lua_pushcclosure(L, http_handle_close, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "readLine");
        lua_pushvalue(L, -3);
        lua_pushcclosure(L, http_handle_readLine, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "readAll");
        lua_pushvalue(L, -3);
        lua_pushcclosure(L, http_handle_readAllByte, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "read");
        lua_pushvalue(L, -3);
        lua_pushcclosure(L, http_handle_readByte, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "seek");
        lua_pushvalue(L, -3);
        lua_pushcclosure(L, http_handle_seek, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "getResponseCode");
        lua_pushvalue(L, -3);
        lua_pushcclosure(L, http_handle_getResponseCode, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "getResponseHeaders");
        lua_pushvalue(L, -3);
        lua_pushcclosure(L, http_handle_getResponseHeaders, 1);
        lua_settable(L, -3);
        lua_remove(L, -2);
    } else {
        delete handle;
    }
    return "http_failure";
}

static std::string http_check(lua_State *L, void* data) {
    http_check_t * res = (http_check_t*)data;
    pushstring(L, res->url);
    lua_pushboolean(L, res->status.empty());
    if (res->status.empty()) lua_pushnil(L);
    else lua_pushstring(L, res->status.c_str());
    delete res;
    return "http_check";
}


static std::string urlEncode(const std::string& tmppath) {
    static const char * hexstr = "0123456789ABCDEF";
    std::string path;
    for (size_t i = 0; i < tmppath.size(); i++) {
        char c = tmppath[i];
        if (isalnum(c) || (c == '%' && i + 2 < tmppath.size() && isxdigit(tmppath[i+1]) && isxdigit(tmppath[i+2]))) path += c;
        else {
            switch (c) {
                case '!': case '#': case '$': case '&': case '\'': case '(':
                case ')': case '*': case '+': case ',': case '/': case ':':
                case ';': case '=': case '?': case '@': case '[': case ']':
                case '-': case '_': case '.': case '~': path += c; break;
                default: path += '%'; path += hexstr[c >> 4]; path += hexstr[c & 0x0F];
            }
        }
    }
    return path;
}

static void downloadThread(void* arg) {
#ifdef __APPLE__
    pthread_setname_np("HTTP Download Thread");
#endif
#ifdef __ANDROID__
    Android_JNI_SetupThread();
#endif
    http_param_t* param = (http_param_t*)arg;
    Poco::URI uri;
    HTTPClientSession * session;
    std::string status;
    std::string path;
    param->comp->requests_open++;
downloadThread_entry:
    bool isLocalhost = false;
    {
        if (param->url.find(':') == std::string::npos) status = "Must specify http or https";
        else if (param->url.find("://") == std::string::npos) status = "URL malformed";
        else if (param->url.substr(0, 7) != "http://" && param->url.substr(0, 8) != "https://") status = "Invalid protocol '" + param->url.substr(0, param->url.find("://")) + "'";
        try {
            uri = Poco::URI(param->url);
        } catch (Poco::SyntaxException &e) {
            status = "URL malformed";
        }
        if (status.empty()) {
            size_t pos = param->url.find('/', param->url.find(uri.getHost()));
            size_t hash = pos != std::string::npos ? param->url.find('#', pos) : std::string::npos;
            path = urlEncode(pos != std::string::npos ? param->url.substr(pos, hash - pos) : "/");
            if (uri.getHost() == "localhost") {isLocalhost = true; uri.setHost("127.0.0.1");}
            bool found = false;
            for (const std::string& wclass : config.http_whitelist) {
                if (matchIPClass(uri.getHost(), wclass)) {
                    found = true;
                    for (const std::string& bclass : config.http_blacklist) {
                        if (matchIPClass(uri.getHost(), bclass)) {
                            found = false;
                            break;
                        }
                    }
                    if (!found) break;
                }
            }
            if (!found) status = "Domain not permitted";
            else if (uri.getScheme() == "http") {
                session = new HTTPClientSession(uri.getHost(), uri.getPort());
            } else if (uri.getScheme() == "https") {
                try {
                    Context::Ptr context = new Context(Context::CLIENT_USE, "", Context::VERIFY_RELAXED, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
                    addSystemCertificates(context);
#if POCO_VERSION >= 0x010A0000
                    context->disableProtocols(Context::PROTO_TLSV1_3); // Some sites break under TLS 1.3 - disable it to maintain compatibility until fixed (pocoproject/poco#3395)
#endif
                    session = new HTTPSClientSession(uri.getHost(), uri.getPort(), context);
                } catch (Poco::Exception &e) {
                    http_handle_t * err = new http_handle_t(NULL);
                    err->url = param->url;
                    err->failureReason = e.message();
                    queueEvent(param->comp, http_failure, err);
                    goto downloadThread_finish;
                }
            } else status = "Invalid protocol '" + uri.getScheme() + "'";
        }
        if (!status.empty()) {
            http_handle_t * err = new http_handle_t(NULL);
            err->url = param->url;
            err->failureReason = status;
            queueEvent(param->comp, http_failure, err);
            goto downloadThread_finish;
        }

        if (!config.http_proxy_server.empty()) session->setProxy(config.http_proxy_server, config.http_proxy_port);
        HTTPRequest request(!param->method.empty() ? param->method : (!param->postData.empty() ? "POST" : "GET"), path, HTTPMessage::HTTP_1_1);
        HTTPResponse * response = new HTTPResponse();
        if (param->timeout > 0) session->setTimeout(Poco::Timespan(param->timeout * 1000000));
        else if (config.http_timeout > 0) session->setTimeout(Poco::Timespan(config.http_timeout * 1000));
        size_t requestSize = param->postData.size();
        for (const auto& h : param->headers) {request.add(h.first, h.second); requestSize += h.first.size() + h.second.size() + 1;}
        if (isLocalhost) request.add("Host", "localhost");
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
            delete response;
            delete session;
            goto downloadThread_finish;
        }
        try {
            std::ostream& reqs = session->sendRequest(request);
            if (!param->postData.empty()) reqs.write(param->postData.c_str(), param->postData.size());
            if (reqs.bad() || reqs.fail()) {
                http_handle_t * err = new http_handle_t(NULL);
                err->url = param->url;
                err->failureReason = "Failed to send request";
                queueEvent(param->comp, http_failure, err);
                delete response;
                delete session;
                goto downloadThread_finish;
            }
        } catch (Poco::TimeoutException &e) {
            http_handle_t * err = new http_handle_t(NULL);
            err->url = param->url;
            err->failureReason = "Timed out";
            queueEvent(param->comp, http_failure, err);
            delete response;
            delete session;
            goto downloadThread_finish;
        } catch (Poco::Exception &e) {
            fprintf(stderr, "Error while downloading %s: %s\n", param->url.c_str(), e.displayText().c_str());
            http_handle_t * err = new http_handle_t(NULL);
            err->url = param->url;
            err->failureReason = e.name();
            queueEvent(param->comp, http_failure, err);
            delete response;
            delete session;
            goto downloadThread_finish;
        }
        http_handle_t * handle;
        try {
            if (config.standardsMode) {
                // Fix seeking by reading the entire data into a stringstream
                std::istream& instream = session->receiveResponse(*response);
                std::stringstream * ss = new std::stringstream;
                while (!instream.eof()) {
                    char buf[4096];
                    instream.read(buf, 4096);
                    ss->write(buf, instream.gcount());
                }
                handle = new http_handle_t(ss);
            } else handle = new http_handle_t(&session->receiveResponse(*response));
        } catch (Poco::TimeoutException &e) {
            http_handle_t * err = new http_handle_t(NULL);
            err->url = param->url;
            err->failureReason = "Timed out";
            queueEvent(param->comp, http_failure, err);
            delete response;
            delete session;
            goto downloadThread_finish;
        } catch (Poco::Exception &e) {
            fprintf(stderr, "Error while downloading %s: %s\n", param->url.c_str(), e.displayText().c_str());
            http_handle_t * err = new http_handle_t(NULL);
            err->url = param->url;
            err->failureReason = e.name();
            queueEvent(param->comp, http_failure, err);
            delete response;
            delete session;
            goto downloadThread_finish;
        }
        if (config.http_max_download > 0 && response->hasContentLength() && response->getContentLength() > config.http_max_download) {
            http_handle_t * err = new http_handle_t(NULL);
            err->url = param->url;
            err->failureReason = "Response is too large";
            queueEvent(param->comp, http_failure, err);
            if (config.standardsMode) delete (std::stringstream*)handle->stream;
            delete handle;
            delete response;
            delete session;
            goto downloadThread_finish;
        }
        handle->session = session;
        handle->handle = response;
        handle->url = param->old_url;
        if (param->redirect && handle->handle->getStatus() / 100 == 3 && handle->handle->has("Location")) {
            std::string location = handle->handle->get("Location");
            if (location.find("://") == std::string::npos) {
                if (location[0] == '/') location = uri.getScheme() + "://" + uri.getHost() + location;
                else location = uri.getScheme() + "://" + uri.getHost() + path.substr(0, path.find('?')) + "/" + location;
            }
            if (config.standardsMode) delete (std::stringstream*)handle->stream;
            delete handle->handle;
            delete handle->session;
            delete handle;
            param->url = location;
            goto downloadThread_entry;
        }
        if (response->getStatus() >= 400) {
            handle->failureReason = HTTPResponse::getReasonForStatus(response->getStatus());
            queueEvent(param->comp, http_failure, handle);
        } else {
            queueEvent(param->comp, http_success, handle);
        }
    }
downloadThread_finish:
    if (freedComputers.find(param->comp) != freedComputers.end()) return;
    param->comp->httpRequestQueueMutex.lock();
    if (!param->comp->httpRequestQueue.empty()) {
        http_param_t * p = (http_param_t*)param->comp->httpRequestQueue.front();
        param->comp->httpRequestQueue.pop();
        param->comp->httpRequestQueueMutex.unlock();
        delete param;
        param = p;
        goto downloadThread_entry;
    }
    param->comp->requests_open--;
    param->comp->httpRequestQueueMutex.unlock();
    delete param;
}

void HTTPDownload(const std::string& url, const std::function<void(std::istream*, Poco::Exception*, HTTPResponse*)>& callback) {
    Poco::URI uri;
    try {
        uri = Poco::URI(url);
    } catch (Poco::SyntaxException &e) {
        callback(NULL, &e, NULL);
        return;
    }
    Context::Ptr ctx = new Context(Context::CLIENT_USE, "", Context::VERIFY_RELAXED, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
    addSystemCertificates(ctx);
#if POCO_VERSION >= 0x010A0000
    ctx->disableProtocols(Context::PROTO_TLSV1_3);
#endif
    HTTPSClientSession session(uri.getHost(), uri.getPort(), ctx);
    if (!config.http_proxy_server.empty()) session.setProxy(config.http_proxy_server, config.http_proxy_port);
    size_t pos = url.find('/', url.find(uri.getHost()));
    std::string path = urlEncode(pos != std::string::npos ? url.substr(pos) : "/");
    HTTPRequest request(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);
    HTTPResponse response;
    session.setTimeout(Poco::Timespan(5000000));
    request.add("User-Agent", "CraftOS-PC/" CRAFTOSPC_VERSION " ComputerCraft/" CRAFTOSPC_CC_VERSION);
    try {
        session.sendRequest(request);
        std::istream& stream = session.receiveResponse(response);
        if (response.getStatus() / 100 == 3 && response.has("Location")) 
            return HTTPDownload(response.get("Location"), callback);
        callback(&stream, NULL, &response);
    } catch (Poco::Exception &e) {
        callback(NULL, &e, NULL);
    }
}

static void* checkThread(void* arg) {
#ifdef __APPLE__
    pthread_setname_np("HTTP Check Thread");
#endif
#ifdef __ANDROID__
    Android_JNI_SetupThread();
#endif
    http_param_t * param = (http_param_t*)arg;
    std::string status;
    if (param->url.find(':') == std::string::npos) status = "Must specify http or https";
    else if (param->url.find("://") == std::string::npos) status = "URL malformed";
    else if (param->url.substr(0, 7) != "http://" && param->url.substr(0, 8) != "https://") status = "Invalid protocol '" + param->url.substr(0, param->url.find("://")) + "'";
    else {
        Poco::URI uri;
        try {
            uri = Poco::URI(param->url);
        } catch (Poco::SyntaxException &e) {
            status = "URL malformed";
        }
        if (status.empty()) {
            bool found = false;
            for (const std::string& wclass : config.http_whitelist) {
                if (matchIPClass(uri.getHost(), wclass)) {
                    found = true;
                    for (const std::string& bclass : config.http_blacklist) {
                        if (matchIPClass(uri.getHost(), bclass)) {
                            found = false;
                            break;
                        }
                    }
                    if (!found) break;
                }
            }
            if (!found) status = "Domain not permitted";
        }
    }
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
    if (!lua_isstring(L, 1) && !lua_istable(L, 1)) luaL_error(L, "bad argument #1 (expected string or table, got %s)", lua_typename(L, lua_type(L, 1)));
    http_param_t * param = new http_param_t;
    param->comp = get_comp(L);
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "url");
        if (!lua_isstring(L, -1)) {delete param; return luaL_error(L, "bad field 'url' (string expected, got %s)", lua_typename(L, lua_type(L, -1)));}
        param->url = tostring(L, -1);
        param->old_url = param->url;
        lua_pop(L, 1);
        lua_getfield(L, 1, "body");
        if (!lua_isnil(L, -1) && !lua_isstring(L, -1)) {delete param; return luaL_error(L, "bad field 'body' (string expected, got %s)", lua_typename(L, lua_type(L, -1)));}
        else if (lua_isstring(L, -1)) param->postData = tostring(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 1, "method");
        if (!lua_isnil(L, -1) && !lua_isstring(L, -1)) {delete param; return luaL_error(L, "bad field 'method' (string expected, got %s)", lua_typename(L, lua_type(L, -1)));}
        else if (lua_isstring(L, -1)) param->method = tostring(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 1, "redirect");
        param->redirect = true;
        if (!lua_isnil(L, -1) && !lua_isboolean(L, -1)) {delete param; return luaL_error(L, "bad field 'redirect' (boolean expected, got %s)", lua_typename(L, lua_type(L, -1)));}
        else if (lua_isboolean(L, -1)) param->redirect = lua_toboolean(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 1, "headers");
        if (!lua_isnil(L, -1) && !lua_istable(L, -1)) {delete param; return luaL_error(L, "bad field 'headers' (table expected, got %s)", lua_typename(L, lua_type(L, -1)));}
        else if (lua_istable(L, -1)) {
            lua_pushnil(L);
            while (lua_next(L, -2)) {
                size_t keyn = 0, valn = 0;
                const char * key = lua_tolstring(L, -2, &keyn), *val = lua_tolstring(L, -1, &valn);
                if (key && val) param->headers[std::string(key, keyn)] = std::string(val, valn);
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
        lua_getfield(L, 1, "timeout");
        if (!lua_isnil(L, -1) && !lua_isnumber(L, -1)) {delete param; return luaL_error(L, "bad field 'timeout' (number expected, got %s)", lua_typename(L, lua_type(L, -1)));}
        else if (lua_isnumber(L, -1)) param->timeout = lua_tonumber(L, -1);
        else param->timeout = 0;
        lua_pop(L, 1);
    } else {
        param->url = checkstring(L, 1);
        param->old_url = param->url;
        if (!lua_isnoneornil(L, 2)) param->postData = checkstring(L, 2);
        else param->postData = "";
        if (lua_istable(L, 3)) {
            lua_pushvalue(L, 3);
            lua_pushnil(L);
            for (int i = 0; lua_next(L, -2); i++) {
                size_t keyn = 0, valn = 0;
                const char * key = lua_tolstring(L, -2, &keyn), *val = lua_tolstring(L, -1, &valn);
                if (key && val) param->headers[std::string(key, keyn)] = std::string(val, valn);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        param->method = luaL_optstring(L, 5, "");
        param->redirect = !lua_isboolean(L, 6) || lua_toboolean(L, 6);
        param->timeout = 0;
    }
    std::lock_guard<std::mutex> lock(param->comp->httpRequestQueueMutex);
    if (param->comp->requests_open >= config.http_max_requests) {
        param->comp->httpRequestQueue.push(param);
    } else {
        std::thread th(downloadThread, param);
        setThreadName(th, "HTTP Request Thread");
        th.detach();
    }
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

extern int os_startTimer(lua_State *L);

struct ws_handle {
    bool isServer;
    std::string url;
    WebSocket * ws;
    std::mutex lock;
    uint16_t port;
    void * clientID = NULL;
    ws_handle ** ud = NULL;
};

struct websocket_failure_data {
    std::string url;
    std::string reason;
    uint16_t port;
};

struct ws_message {
    std::string url;
    std::string data;
    uint16_t port;
    void * clientID = NULL;
    bool binary;
};

struct websocket_closed_data {
    std::string url;
    std::string reason;
    uint16_t code;
    void* clientID = NULL;
};

static std::string websocket_failure(lua_State *L, void* userp) {
    websocket_failure_data * data = (websocket_failure_data*)userp;
    if (data->url.empty()) lua_pushnumber(L, data->port);
    else lua_pushstring(L, data->url.c_str());
    lua_pushstring(L, data->reason.c_str());
    delete data;
    return "websocket_failure";
}

static std::string websocket_closed(lua_State *L, void* userp) {
    websocket_closed_data * data = (websocket_closed_data*)userp;
    if (data == NULL) lua_pushnil(L);
    else pushstring(L, data->url);
    if (data->code != 0) {
        pushstring(L, data->reason);
        lua_pushinteger(L, data->code);
    }
    delete data;
    return "websocket_closed";
}

// WebSocket handle functions
static int websocket_free(lua_State *L) {
    lastCFunction = __func__;
    ws_handle * ws = *(ws_handle**)lua_touserdata(L, 1);
    if (ws == NULL) return 0;
    std::lock_guard<std::mutex> lock(ws->lock);
    if (ws->ws == NULL) return 0;
    ws->ws = NULL;
    return 0;
}

static int websocket_close(lua_State *L) {
    lastCFunction = __func__;
    ws_handle * ws = *(ws_handle**)lua_touserdata(L, lua_upvalueindex(1));
    if (ws == NULL) return 0;
    std::lock_guard<std::mutex> lock(ws->lock);
    if (ws->ws == NULL) return 0;
    ws->ws = NULL;
    return 0;
}

static int websocket_send(lua_State *L) {
    lastCFunction = __func__;
    std::string str = checkstring(L, 1);
    if (config.http_max_websocket_message > 0 && str.size() > (unsigned)config.http_max_websocket_message) luaL_error(L, "Message is too large");
    ws_handle * ws = *(ws_handle**)lua_touserdata(L, lua_upvalueindex(1));
    if (ws == NULL) return luaL_error(L, "attempt to use a closed file");
    std::lock_guard<std::mutex> lock(ws->lock);
    if (ws->ws == NULL) return luaL_error(L, "attempt to use a closed file");
    ws->ws->sendFrame(str.c_str(), str.size(), (int)WebSocket::FRAME_FLAG_FIN | (int)(lua_toboolean(L, 2) ? WebSocket::FRAME_BINARY : WebSocket::FRAME_TEXT));
    return 0;
}

static int websocket_receive(lua_State *L) {
    lastCFunction = __func__;
    ws_handle * ws = *(ws_handle**)lua_touserdata(L, lua_upvalueindex(1));
    int tm = 0;
    if (lua_getctx(L, &tm) == LUA_YIELD) {
        if (lua_isstring(L, 1)) {
            // haha, another string scoping issue :DDD
            // can M$ PLEASE fix this? (maybe I need to repro & report? :thinking:)
            if (ws == NULL) {
                lua_pushnil(L);
                return 1;
            }
            std::string * ev = new std::string(lua_tostring(L, 1));
            std::string * url = new std::string();
            int tmid = 0;
            if (lua_isnumber(L, 2)) tmid = lua_tointeger(L, 2);
            else if (lua_isstring(L, 2)) {
                delete url;
                url = new std::string(lua_tostring(L, 2));
            }
            if (*ev == "websocket_message" && !ws->isServer && *url == ws->url) {
                lua_pushvalue(L, 3);
                lua_pushvalue(L, 4);
                delete ev;
                delete url;
                return 2;
            } else if (*ev == "websocket_server_message" && ws->isServer && lua_touserdata(L, 2) == ws->clientID) {
                lua_pushvalue(L, 3);
                lua_pushvalue(L, 4);
                delete ev;
                delete url;
                return 2;
            } else if ((*ev == "websocket_closed" && !ws->isServer && *url == ws->url && ws->ws == NULL) ||
                       (*ev == "websocket_server_closed" && ws->isServer && lua_touserdata(L, 2) == ws->clientID && ws->ws == NULL) ||
                       (tm > 0 && *ev == "timer" && lua_isnumber(L, 2) && tmid == tm)) {
                lua_pushnil(L);
                delete ev;
                delete url;
                return 1;
            } else if (*ev == "terminate") {
                delete ev;
                delete url;
                return luaL_error(L, "Terminated");
            }
            delete ev;
            delete url;
        }
    } else {
        if (ws == NULL || ws->ws == NULL) luaL_error(L, "attempt to use a closed file");
        // instead of using native timer routines, we're using os.startTimer so we can be resumed
        if (!lua_isnoneornil(L, 1)) {
            luaL_checknumber(L, 1);
            lua_pushcfunction(L, os_startTimer);
            lua_pushvalue(L, 1);
            lua_call(L, 1, 1);
            tm = lua_tointeger(L, -1);
            lua_pop(L, 1);
        } else tm = -1;
    }
    lua_settop(L, 0);
    return lua_yieldk(L, 0, tm, websocket_receive);
}

static luaL_Reg websocket_funcs[] = {
    {"close", websocket_close},
    {"receive", websocket_receive},
    {"send", websocket_send},
    {NULL, NULL}
};

static std::string websocket_success(lua_State *L, void* userp) {
    ws_handle ** wsh = (ws_handle**)userp;
    luaL_checkstack(L, 10, "Could not grow stack for websocket_success");
    lua_pushstring(L, (*wsh)->url.c_str());
    lua_createtable(L, 0, 4);
    ws_handle ** ws = (ws_handle**)lua_newuserdata(L, sizeof(ws_handle*));
    *ws = *wsh;
    (*wsh)->ud = ws;
    lua_createtable(L, 0, 1);
    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, websocket_free);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);
    luaL_setfuncs(L, websocket_funcs, 1);
    return "websocket_success";
}

static std::string websocket_message(lua_State *L, void* userp) {
    ws_message * message = (ws_message*)userp;
    lua_pushstring(L, message->url.c_str());
    pushstring(L, message->data);
    lua_pushboolean(L, message->binary);
    delete message;
    return "websocket_message";
}

// websocket_server_connect: port, handle
// handles have an additional clientID member
static std::string websocket_server_connect(lua_State *L, void* userp) {
    ws_handle ** wsh = (ws_handle**)userp;
    luaL_checkstack(L, 10, "Could not grow stack for websocket_server_connect");
    lua_pushnumber(L, (*wsh)->port);
    lua_createtable(L, 0, 4);
    ws_handle ** ws = (ws_handle**)lua_newuserdata(L, sizeof(ws_handle*));
    *ws = *wsh;
    (*wsh)->ud = ws;
    lua_createtable(L, 0, 1);
    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, websocket_free);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);
    luaL_setfuncs(L, websocket_funcs, 1);
    lua_pushlightuserdata(L, (*ws)->clientID);
    lua_setfield(L, -2, "clientID");
    return "websocket_server_connect";
}

static std::string websocket_server_message(lua_State *L, void* userp) {
    ws_message * message = (ws_message*)userp;
    lua_pushlightuserdata(L, message->clientID);
    pushstring(L, message->data);
    lua_pushboolean(L, message->binary);
    delete message;
    return "websocket_server_message";
}

static std::string websocket_server_closed(lua_State *L, void* userp) {
    websocket_closed_data * data = (websocket_closed_data*)userp;
    lua_pushlightuserdata(L, data->clientID);
    if (data->code != 0) {
        pushstring(L, data->reason);
        lua_pushinteger(L, data->code);
    }
    delete data;
    return "websocket_server_closed";
}

class websocket_server: public HTTPRequestHandler {
public:
    Computer * comp;
    HTTPServer *srv;
    std::unordered_map<std::string, std::string> headers;
    int * retainCount;
    websocket_server(Computer * c, HTTPServer *s, const std::unordered_map<std::string, std::string>& h, int *r): comp(c), srv(s), headers(h), retainCount(r) {}
    void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response) override {
        WebSocket * ws = NULL;
        try {
            ws = new WebSocket(request, response);
        } catch (NetException &e) {
            return;
        }
        (*retainCount)++;
        ws->setReceiveTimeout(Poco::Timespan(1, 0));
#if POCO_VERSION >= 0x01090100
        if (config.http_max_websocket_message > 0) ws->setMaxPayloadSize(config.http_max_websocket_message);
#endif
        ws_handle ws_orig;
        ws_handle * wsh = &ws_orig;
        wsh->ws = ws;
        wsh->isServer = true;
        wsh->port = srv->port();
        wsh->clientID = &request;
        {
            std::lock_guard<std::mutex> lock(comp->openWebsocketsMutex);
            comp->openWebsockets.push_back(&wsh);
        }
        queueEvent(comp, websocket_server_connect, &wsh);
        char * buf = new char[config.http_max_websocket_message];
        while (wsh->ws) {
            int flags = 0;
            int res;
            try {
                res = ws->receiveFrame(buf, config.http_max_websocket_message, flags);
                if (res < 0 || (res == 0 && flags == 0)) {
                    wsh->ws = NULL;
                    websocket_closed_data * d = new websocket_closed_data;
                    d->clientID = wsh->clientID;
                    d->code = 0;
                    queueEvent(comp, websocket_server_closed, d);
                    break;
                }
            } catch (Poco::TimeoutException &e) {
                if (!wsh->ws) {
                    websocket_closed_data * d = new websocket_closed_data;
                    d->clientID = wsh->clientID;
                    d->code = 1006;
                    d->reason = "Timed out";
                    queueEvent(comp, websocket_server_closed, d);
                    break;
                }
                continue;
            } catch (NetException &e) {
                wsh->ws = NULL;
                websocket_closed_data * d = new websocket_closed_data;
                d->clientID = wsh->clientID;
                d->code = 1006;
                d->reason = e.message();
                queueEvent(comp, websocket_server_closed, d);
                break;
            }
            if ((flags & 0x0f) == WebSocket::FRAME_OP_CLOSE) {
                wsh->ws = NULL;
                websocket_closed_data * d = new websocket_closed_data;
                d->clientID = wsh->clientID;
                if (res >= 2) {
                    d->code = (((uint8_t*)buf)[0] << 8) | ((uint8_t*)buf)[1];
                    d->reason = std::string(buf + 2, res - 2);
                } else d->code = 0;
                queueEvent(comp, websocket_server_closed, d);
                break;
            } else if ((flags & 0x0f) == WebSocket::FRAME_OP_PING) {
                ws->sendFrame(buf, res, WebSocket::FRAME_FLAG_FIN | WebSocket::FRAME_OP_PONG);
            } else {
                ws_message * message = new ws_message;
                message->clientID = wsh->clientID;
                message->binary = (flags & WebSocket::FRAME_OP_BITMASK) == WebSocket::FRAME_OP_BINARY;
                message->data = std::string((const char*)buf, res);
                queueEvent(comp, websocket_server_message, message);
            }
            std::this_thread::yield();
        }
        {
            std::lock_guard<std::mutex> lock(comp->openWebsocketsMutex);
            auto it = std::find(comp->openWebsockets.begin(), comp->openWebsockets.end(), (void*)&wsh);
            if (it != comp->openWebsockets.end()) comp->openWebsockets.erase(it);
        }
        try {ws->shutdown();} catch (...) {}
        std::lock_guard<std::mutex> lock(wsh->lock);
        wsh->ws = NULL;
        if (wsh->ud != NULL) *wsh->ud = NULL;
        delete ws;
    }
    class Factory: public HTTPRequestHandlerFactory {
    public:
        Computer *comp;
        HTTPServer *srv = NULL;
        std::unordered_map<std::string, std::string> headers;
        int retainCount = 0;
        Factory(Computer *c, const std::unordered_map<std::string, std::string>& h): comp(c), headers(h) {}
        HTTPRequestHandler* createRequestHandler(const HTTPServerRequest&) override {
            return new websocket_server(comp, srv, headers, &retainCount);
        }
    };
};

/* export */ void stopWebsocket(void* wsh) {
    ws_handle * handle = *(ws_handle**)wsh;
    if (handle->ws != NULL) {
        //handle->ws->close();
        handle->ws = NULL;
    }
}

static void websocket_client_thread(Computer *comp, const std::string& str, const std::unordered_map<std::string, std::string>& headers, double timeout) {
#ifdef __APPLE__
    pthread_setname_np("WebSocket Client Thread");
#endif
#ifdef __ANDROID__
    Android_JNI_SetupThread();
#endif
    Poco::URI uri;
    try {
        uri = Poco::URI(str);
    } catch (Poco::SyntaxException &e) {
        websocket_failure_data * data = new websocket_failure_data;
        data->url = str;
        data->reason = "URL malformed";
        queueEvent(comp, websocket_failure, data);
        return;
    }
    if (uri.getHost() == "localhost") uri.setHost("127.0.0.1");
    bool found = false;
    for (const std::string& wclass : config.http_whitelist) {
        if (matchIPClass(uri.getHost(), wclass)) {
            found = true;
            for (const std::string& bclass : config.http_blacklist) {
                if (matchIPClass(uri.getHost(), bclass)) {
                    found = false;
                    break;
                }
            }
            if (!found) break;
        }
    }
    if (!found) {
        websocket_failure_data * data = new websocket_failure_data;
        data->url = str;
        data->reason = "Domain not permitted";
        queueEvent(comp, websocket_failure, data);
        return;
    }
    HTTPClientSession * cs;
    if (uri.getScheme() == "ws") cs = new HTTPClientSession(uri.getHost(), uri.getPort());
    else if (uri.getScheme() == "wss") {
        Context::Ptr ctx = new Context(Context::CLIENT_USE, "", Context::VERIFY_RELAXED, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
        addSystemCertificates(ctx);
#if POCO_VERSION >= 0x010A0000
        ctx->disableProtocols(Context::PROTO_TLSV1_3);
#endif
        cs = new HTTPSClientSession(uri.getHost(), uri.getPort(), ctx);
    } else {
        websocket_failure_data * data = new websocket_failure_data;
        data->url = str;
        data->reason = "Invalid scheme '" + uri.getScheme() + "'";
        queueEvent(comp, websocket_failure, data);
        return;
    }
#if POCO_VERSION >= 0x010C0000
    cs->setReceiveTimeout(Poco::Timespan(timeout * 1000000));
#endif
    size_t pos = str.find('/', str.find(uri.getHost()));
    size_t hash = pos != std::string::npos ? str.find('#', pos) : std::string::npos;
    std::string path = urlEncode(pos != std::string::npos ? str.substr(pos, hash - pos) : "/");
    if (!config.http_proxy_server.empty()) cs->setProxy(config.http_proxy_server, config.http_proxy_port);
    HTTPRequest request(HTTPRequest::HTTP_GET, path, HTTPMessage::HTTP_1_1);
    for (std::pair<std::string, std::string> h : headers) request.set(h.first, h.second);
    if (!request.has("User-Agent")) request.add("User-Agent", "computercraft/" CRAFTOSPC_CC_VERSION " CraftOS-PC/" CRAFTOSPC_VERSION);
    if (!request.has("Accept-Charset")) request.add("Accept-Charset", "UTF-8");
    HTTPResponse response;
    WebSocket* ws;
    try {
        ws = new WebSocket(*cs, request, response);
    } catch (Poco::Exception &e) {
        websocket_failure_data * data = new websocket_failure_data;
        data->url = str;
        data->reason = e.displayText();
        queueEvent(comp, websocket_failure, data);
        return;
    } catch (std::exception &e) {
        websocket_failure_data * data = new websocket_failure_data;
        data->url = str;
        data->reason = e.what();
        queueEvent(comp, websocket_failure, data);
        return;
    }
    //if (config.http_timeout > 0) ws->setReceiveTimeout(Poco::Timespan(config.http_timeout * 1000));
    ws->setReceiveTimeout(Poco::Timespan(1, 0));
#if POCO_VERSION >= 0x01090100
    if (config.http_max_websocket_message > 0) ws->setMaxPayloadSize(config.http_max_websocket_message);
#endif
    ws_handle wsh_orig;
    ws_handle * wsh = &wsh_orig;
    wsh->isServer = false;
    wsh->url = str;
    wsh->ws = ws;
    {
        std::lock_guard<std::mutex> lock(comp->openWebsocketsMutex);
        comp->openWebsockets.push_back(&wsh);
    }
    queueEvent(comp, websocket_success, &wsh);
    char * buf = new char[config.http_max_websocket_message];
    while (wsh->ws) {
        int flags = 0;
        int res;
        try {
            res = ws->receiveFrame(buf, config.http_max_websocket_message, flags);
            if (res < 0 || (res == 0 && flags == 0)) {
                wsh->ws = NULL;
                websocket_closed_data * d = new websocket_closed_data;
                d->url = str;
                d->code = 0;
                queueEvent(comp, websocket_closed, d);
                break;
            }
        } catch (Poco::TimeoutException &e) {
            if (!wsh->ws) {
                websocket_closed_data * d = new websocket_closed_data;
                d->url = str;
                d->code = 1006;
                d->reason = "Timed out";
                queueEvent(comp, websocket_closed, d);
                break;
            }
            continue;
        } catch (NetException &e) {
            wsh->ws = NULL;
            websocket_closed_data * d = new websocket_closed_data;
            d->url = str;
            d->code = 1006;
            d->reason = e.message();
            queueEvent(comp, websocket_closed, d);
            break;
        }
        if ((flags & 0x0f) == WebSocket::FRAME_OP_CLOSE) {
            wsh->ws = NULL;
            websocket_closed_data * d = new websocket_closed_data;
            d->url = str;
            if (res >= 2) {
                d->code = (((uint8_t*)buf)[0] << 8) | ((uint8_t*)buf)[1];
                d->reason = std::string(buf + 2, res - 2);
            } else d->code = 0;
            queueEvent(comp, websocket_closed, d);
            break;
        } else if ((flags & 0x0f) == WebSocket::FRAME_OP_PING) {
            ws->sendFrame(buf, res, WebSocket::FRAME_FLAG_FIN | WebSocket::FRAME_OP_PONG);
        } else {
            ws_message * message = new ws_message;
            message->url = str;
            message->binary = (flags & WebSocket::FRAME_OP_BITMASK) == WebSocket::FRAME_OP_BINARY;
            message->data = std::string((const char*)buf, res);
            queueEvent(comp, websocket_message, message);
        }
        std::this_thread::yield();
    }
    delete[] buf;
    {
        std::lock_guard<std::mutex> lock(comp->openWebsocketsMutex);
        auto it = std::find(comp->openWebsockets.begin(), comp->openWebsockets.end(), (void*)&wsh);
        if (it != comp->openWebsockets.end()) comp->openWebsockets.erase(it);
    }
    try {ws->shutdown();} catch (...) {}
    std::lock_guard<std::mutex> lock(wsh->lock);
    wsh->ws = NULL;
    if (wsh->ud != NULL) *wsh->ud = NULL;
    delete ws;
    delete cs;
}

static int http_websocket(lua_State *L) {
    lastCFunction = __func__;
    if (!config.http_websocket_enabled) luaL_error(L, "Websocket connections are disabled");
    Computer * comp = get_comp(L);
    if (comp->openWebsockets.size() >= (size_t)config.http_max_websockets) return luaL_error(L, "Too many websockets already open");
    if (!(config.serverMode || config.vanilla) && (lua_isnoneornil(L, 1) || lua_isnumber(L, 1))) {
        luaL_error(L, "function has been replaced with http.websocketServer");
    } else if (lua_istable(L, 1)) {
        Computer * comp = get_comp(L);
        if (config.http_max_websockets > 0 && comp->openWebsockets.size() >= (unsigned)config.http_max_websockets) luaL_error(L, "Too many websockets already open");
        std::unordered_map<std::string, std::string> headers;
        lua_getfield(L, 1, "url");
        if (!lua_isstring(L, -1)) luaL_error(L, "bad field 'url' (expected string, got %s)", lua_typename(L, lua_type(L, -1)));
        std::string url = tostring(L, -1);
        lua_pop(L, 1);
        lua_getfield(L, 1, "headers");
        if (!lua_isnil(L, -1) && !lua_istable(L, -1)) luaL_error(L, "bad field 'headers' (expected table, got %s)", lua_typename(L, lua_type(L, -1)));
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            for (int i = 0; lua_next(L, -2); i++) {
                size_t keyn = 0, valn = 0;
                const char * key = lua_tolstring(L, -2, &keyn), *val = lua_tolstring(L, -1, &valn);
                if (key && val) headers[std::string(key, keyn)] = std::string(val, valn);
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
        lua_getfield(L, 1, "timeout");
        if (!lua_isnil(L, -1) && !lua_isnumber(L, -1)) luaL_error(L, "bad field 'timeout' (expected number, got %s)", lua_typename(L, lua_type(L, -1)));
        double timeout = luaL_optnumber(L, -1, config.http_timeout / 1000.0);
        lua_pop(L, 1);
        std::thread th(websocket_client_thread, comp, url, headers, timeout);
        setThreadName(th, "WebSocket Client Thread");
        th.detach();
    } else if (lua_isstring(L, 1)) {
        Computer * comp = get_comp(L);
        if (config.http_max_websockets > 0 && comp->openWebsockets.size() >= (unsigned)config.http_max_websockets) luaL_error(L, "Too many websockets already open");
        std::string url = tostring(L, 1);
        std::unordered_map<std::string, std::string> headers;
        if (lua_istable(L, 2)) {
            lua_pushvalue(L, 2);
            lua_pushnil(L);
            for (int i = 0; lua_next(L, -2); i++) {
                size_t keyn = 0, valn = 0;
                const char * key = lua_tolstring(L, -2, &keyn), *val = lua_tolstring(L, -1, &valn);
                if (key && val) headers[std::string(key, keyn)] = std::string(val, valn);
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
        }
        std::thread th(websocket_client_thread, comp, url, headers, config.http_timeout / 1000.0);
        setThreadName(th, "WebSocket Client Thread");
        th.detach();
    } else luaL_error(L, (config.serverMode || config.vanilla) ? "bad argument #1 (expected string or table, got %s)" : "bad argument #1 (expected string, table, number, or nil, got %s)", lua_typename(L, lua_type(L, 1)));
    lua_pushboolean(L, true);
    return 1;
}

/**
 * Listens for a new connection on the server, and returns its handle.
 * @param timeout (number, optional) The amount of time to wait
 * @return The WebSocket handle for the connection, or nil on timeout.
 * @note Server handles are the same as client handles, but with an additional
 *       clientID member for identifying events.
 */
static int websocket_server_listen(lua_State *L) {
    lastCFunction = __func__;
    websocket_server::Factory * f = *(websocket_server::Factory**)lua_touserdata(L, lua_upvalueindex(1));
    int tm = 0;
    if (lua_getctx(L, &tm) == LUA_YIELD) {
        if (lua_isstring(L, 1)) {
            // haha, another string scoping issue :DDD
            // can M$ PLEASE fix this? (maybe I need to repro & report? :thinking:)
            if (f == NULL) {
                lua_pushnil(L);
                return 1;
            }
            std::string * ev = new std::string(lua_tostring(L, 1));
            if (lua_isnumber(L, 2)) {
                int id = lua_tointeger(L, 2);
                if (*ev == "websocket_server_connect" && id == f->srv->port()) {
                    lua_pushvalue(L, 3);
                    delete ev;
                    return 1;
                } else if (tm > 0 && *ev == "timer" && id == tm) {
                    lua_pushnil(L);
                    delete ev;
                    return 1;
                } else if (*ev == "terminate") {
                    delete ev;
                    return luaL_error(L, "Terminated");
                }
            }
            delete ev;
        }
    } else {
        if (f == NULL) luaL_error(L, "attempt to use a closed file");
        // instead of using native timer routines, we're using os.startTimer so we can be resumed
        if (!lua_isnoneornil(L, 1)) {
            luaL_checknumber(L, 1);
            lua_pushcfunction(L, os_startTimer);
            lua_pushvalue(L, 1);
            lua_call(L, 1, 1);
            tm = lua_tointeger(L, -1);
            lua_pop(L, 1);
        } else tm = -1;
    }
    lua_settop(L, 0);
    return lua_yieldk(L, 0, tm, websocket_server_listen);
}

/**
 * Closes the WebSocket server.
 */
static int websocket_server_close(lua_State *L) {
    lastCFunction = __func__;
    Computer * comp = get_comp(L);
    websocket_server::Factory * f = *(websocket_server::Factory**)lua_touserdata(L, lua_upvalueindex(1));
    if (f == NULL) return 0;
    comp->openWebsocketServers.erase(f->srv->port());
    f->srv->stop();
    delete f->srv;
    *(websocket_server::Factory**)lua_touserdata(L, lua_upvalueindex(1)) = NULL;
    return 0;
}

static int websocket_server_free(lua_State *L) {
    lastCFunction = __func__;
    Computer * comp = get_comp(L);
    websocket_server::Factory * f = *(websocket_server::Factory**)lua_touserdata(L, 1);
    if (f == NULL) return 0;
    comp->openWebsocketServers.erase(f->srv->port());
    f->srv->stop();
    delete f->srv;
    *(websocket_server::Factory**)lua_touserdata(L, 1) = NULL;
    return 0;
}

static luaL_Reg websocket_server_funcs[] = {
    {"listen", websocket_server_listen},
    {"close", websocket_server_close},
    {NULL, NULL}
};

/**
 * Creates a new WebSocket server.
 * @param port (number) The port to listen on
 * @return Either a WebSocketServer handle, or nil + error
 */
static int http_websocketServer(lua_State *L) {
    if (config.serverMode) return 0;
    Computer * comp = get_comp(L);
    int port = luaL_checkinteger(L, 1);
    if (port < 0 || port > 65535) luaL_error(L, "bad argument #1 (port out of range)");
    if (comp->openWebsocketServers.find(port) != comp->openWebsocketServers.end()) {
        // if there's already an open server, reuse that
        lua_pushnil(L);
        lua_pushliteral(L, "Port already in use");
        return 2;
    }
    std::unordered_map<std::string, std::string> headers;
    if (lua_istable(L, 2)) {
        lua_pushvalue(L, 2);
        lua_pushnil(L);
        for (int i = 0; lua_next(L, -2); i++) {
            size_t keyn = 0, valn = 0;
            const char * key = lua_tolstring(L, -2, &keyn), *val = lua_tolstring(L, -1, &valn);
            if (key && val) headers[std::string(key, keyn)] = std::string(val, valn);
            lua_pop(L, 2);
        }
        lua_pop(L, 1);
    } else if (!lua_isnoneornil(L, 2)) luaL_error(L, "bad argument #2 (expected table, got %s)", lua_typename(L, lua_type(L, 2)));
    websocket_server::Factory * f = new websocket_server::Factory(comp, headers);
    try {f->srv = new HTTPServer(f, port);}
    catch (Poco::Exception& e) {
        fprintf(stderr, "Could not open server: %s\n", e.displayText().c_str());
        lua_pushnil(L);
        lua_pushstring(L, e.displayText().c_str());
        return 2;
    }
    comp->openWebsocketServers.insert(port);
    f->srv->start();
    lua_createtable(L, 0, 2);
    *(websocket_server::Factory**)lua_newuserdata(L, sizeof(websocket_server::Factory*)) = f;
    lua_createtable(L, 0, 1);
    lua_pushcfunction(L, websocket_server_free);
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);
    luaL_setfuncs(L, websocket_server_funcs, 1);
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
    {"websocketServer", http_websocketServer},
    {NULL, NULL}
};

library_t http_lib = {"http", http_reg, nullptr, nullptr};

#endif // __EMSCRIPTEN__
