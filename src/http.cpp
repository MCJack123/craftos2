/*
 * http.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the http API.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "http.hpp"
#include "http_handle.hpp"
#include "term.hpp"
#include "platform.hpp"
#include "config.hpp"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <algorithm>
#include <cctype>
#include <functional>
extern "C" {
#include <lauxlib.h>
}
#include <Poco/URI.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPMessage.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>
#include <Poco/Net/HTTPServer.h>

using namespace Poco::Net;

#pragma region Client

typedef struct {
    Computer *comp;
    char * url;
    char * postData;
    std::unordered_map<std::string, std::string> headers;
} http_param_t;

typedef struct http_handle {
    bool closed;
    char * url;
    HTTPSClientSession * session;
    HTTPResponse * handle;
    std::istream& stream;
    http_handle(std::istream& s): stream(s) {}
} http_handle_t;

typedef struct {
    char * url;
    const char * status;
} http_check_t;

const char * http_success(lua_State *L, void* data) {
    http_handle_t * handle = (http_handle_t*)data;
    luaL_checkstack(L, 30, "Unable to allocate HTTP handle");
    lua_pushstring(L, handle->url);
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

    lua_pushstring(L, "readLine");
    lua_pushlightuserdata(L, handle);
    lua_pushcclosure(L, http_handle_readLine, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "readAll");
    lua_pushlightuserdata(L, handle);
    lua_pushcclosure(L, http_handle_readAll, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "read");
    lua_pushlightuserdata(L, handle);
    lua_pushcclosure(L, http_handle_read, 1);
    lua_settable(L, -3);

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

const char * http_failure(lua_State *L, void* data) {
    lua_pushstring(L, (char*)data);
    free(data);
    return "http_failure";
}

const char * http_check(lua_State *L, void* data) {
    http_check_t * res = (http_check_t*)data;
    lua_pushstring(L, res->url);
    lua_pushboolean(L, res->status == NULL);
    if (res->status == NULL) lua_pushnil(L);
    else lua_pushstring(L, res->status);
    free(res->url);
    free(res);
    return "http_check";
}

void downloadThread(void* arg) {
    http_param_t* param = (http_param_t*)arg;
    Poco::URI uri(param->url);
    const Context::Ptr context = new Context(Context::CLIENT_USE, "", "", "", Context::VERIFY_NONE, 9, false, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
    HTTPSClientSession * session = new HTTPSClientSession(uri.getHost(), uri.getPort(), context);
    HTTPRequest request(HTTPRequest::HTTP_GET, uri.getPathAndQuery(), HTTPMessage::HTTP_1_1);
    HTTPResponse * response = new HTTPResponse();
    if (param->postData != NULL) request.setMethod("POST");
    for (auto it = param->headers.begin(); it != param->headers.end(); it++) request.add(it->first, it->second);
    std::ostream& reqs = session->sendRequest(request);
    if (param->postData != NULL) reqs << param->postData;
    if (reqs.bad() || reqs.fail()) {
        if (param->postData != NULL) free(param->postData);
        termQueueProvider(param->comp, http_failure, param->url);
        delete param;
        return;
    }
    http_handle_t * handle = new http_handle_t(session->receiveResponse(*response));
    handle->session = session;
    handle->handle = response;
    handle->url = param->url;
    if (handle->handle->getStatus() / 100 == 3 && handle->handle->has("Location")) {
        free(param->url);
        std::string location = handle->handle->get("Location");
        delete handle->handle;
        delete handle->session;
        delete handle;
        param->url = (char*)malloc(location.size() + 1);
        strcpy(param->url, location.c_str());
        return downloadThread(param);
    }
    handle->closed = false;
    termQueueProvider(param->comp, http_success, handle);
    if (param->postData != NULL) free(param->postData);
    delete param;
}

void* checkThread(void* arg) {
    http_param_t * param = (http_param_t*)arg;
    const char * status = NULL;
    if (strstr(param->url, "://") == NULL) status = "URL malformed";
    else if (strstr(param->url, "http") == NULL) status = "URL not http";
    else if (strstr(param->url, "192.168.") != NULL || strstr(param->url, "10.0.") != NULL) status = "Domain not permitted";
    http_check_t * res = (http_check_t*)malloc(sizeof(http_check_t));
    res->url = param->url;
    res->status = status;
    termQueueProvider(param->comp, http_check, res);
    free(param);
    return NULL;
}

int http_request(lua_State *L) {
    if (!config.http_enable) {
        lua_pushboolean(L, false);
        return 1;
    }
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    http_param_t * param = new http_param_t;
    param->comp = get_comp(L);
    param->url = (char*)malloc(lua_strlen(L, 1) + 1); 
    strcpy(param->url, lua_tostring(L, 1));
    param->postData = NULL;
    if (lua_isstring(L, 2)) {
        param->postData = (char*)malloc(lua_strlen(L, 2) + 1);
        strcpy(param->postData, lua_tostring(L, 2));
    }
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
    std::thread th(downloadThread, param);
    setThreadName(th, "HTTP Request Thread");
    th.detach();
    lua_pushboolean(L, 1);
    return 1;
}

int http_checkURL(lua_State *L) {
    if (!config.http_enable) {
        lua_pushboolean(L, false);
        return 1;
    }
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    http_param_t * param = (http_param_t*)malloc(sizeof(http_param_t));
    param->comp = get_comp(L);
    param->url = (char*)malloc(lua_strlen(L, 1) + 1);
    strcpy(param->url, lua_tostring(L, 1));
    std::thread th(checkThread, param);
    setThreadName(th, "HTTP Check Thread");
    th.detach();
    lua_pushboolean(L, true);
    return 1;
}

#pragma endregion
#pragma region Server

struct http_res {
    std::string body;
    HTTPServerResponse * res;
};

int req_read(lua_State *L) {
    HTTPServerRequest * req = (HTTPServerRequest*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || !req->stream().good()) return 0;
    char tmp[2];
    tmp[0] = req->stream().get();
    lua_pushstring(L, tmp);
    return 1;
}

int req_readLine(lua_State *L) {
    HTTPServerRequest * req = (HTTPServerRequest*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || !req->stream().good()) return 0;
    std::string line;
    std::getline(req->stream(), line);
    line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
    lua_pushstring(L, line.c_str());
    return 1;
}

int req_readAll(lua_State *L) {
    HTTPServerRequest * req = (HTTPServerRequest*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || !req->stream().good()) return 0;
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
    return 0;
}

int req_free(lua_State *L) {
    delete (bool*)lua_touserdata(L, lua_upvalueindex(1));
    return 0;
}

int req_getURL(lua_State *L) {
    HTTPServerRequest * req = (HTTPServerRequest*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2))) return 0;
    lua_pushstring(L, req->getURI().c_str());
    return 1;
}

int req_getMethod(lua_State *L) {
    HTTPServerRequest * req = (HTTPServerRequest*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2))) return 0;
    lua_pushstring(L, req->getMethod().c_str());
    return 1;
}

int req_getRequestHeaders(lua_State *L) {
    HTTPServerRequest * req = (HTTPServerRequest*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2))) return 0;
    lua_newtable(L);
    for (auto h = req->begin(); h != req->end(); h++) {
        lua_pushstring(L, h->first.c_str());
        lua_pushstring(L, h->second.c_str());
        lua_settable(L, -3);
    }
    return 1;
}

int res_write(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    struct http_res * res = (struct http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || res->res->sent()) return 0;
    size_t len = 0;
    const char * buf = lua_tolstring(L, 1, &len);
    res->body += std::string(buf, len);
    return 0;
}

int res_writeLine(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    struct http_res * res = (struct http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || res->res->sent()) return 0;
    size_t len = 0;
    const char * buf = lua_tolstring(L, 1, &len);
    res->body += std::string(buf, len);
    res->body += "\n";
    return 0;
}

int res_close(lua_State *L) {
    struct http_res * res = (struct http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || res->res->sent()) return 0;
    std::string body((const std::string)res->body);
    try {
        res->res->setContentLength(body.size());
        res->res->send().write(body.c_str(), body.size());
    } catch (std::exception &e) {
        *(bool*)lua_touserdata(L, lua_upvalueindex(2)) = true;
        lua_pushfstring(L, "Could not send data: %s", e.what());
        lua_error(L);
    }
    *(bool*)lua_touserdata(L, lua_upvalueindex(2)) = true;
    return 0;
}

int res_setStatusCode(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    struct http_res * res = (struct http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || res->res->sent()) return 0;
    res->res->setStatus((HTTPResponse::HTTPStatus)lua_tointeger(L, 1));
    return 0;
}

int res_setResponseHeader(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    struct http_res * res = (struct http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (*(bool*)lua_touserdata(L, lua_upvalueindex(2)) || res->res->sent()) return 0;
    res->res->set(lua_tostring(L, 1), lua_tostring(L, 2));
    return 0;
}

struct http_request_data {
    int port;
    bool closed;
    HTTPServerRequest * req;
    struct http_res * res;
};

const char * http_request_event(lua_State *L, void* userp) {
    struct http_request_data* data = (struct http_request_data*)userp;
    bool* closed = &data->closed;
    *closed = false;
    lua_pushinteger(L, data->port);
    lua_newtable(L);

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
    lua_newtable(L);
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

    lua_newtable(L);

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
    void handleRequest(HTTPServerRequest& req, HTTPServerResponse& res) {
        printf("Got request: %s\n", req.getURI().c_str());
        struct http_res lres = {"", &res};
        struct http_request_data evdata = {port, false, &req, &lres};
        termQueueProvider(comp, http_request_event, &evdata);
        std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
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
        HTTPRequestHandler * createRequestHandler(const HTTPServerRequest &request) {
            return new HTTPListener(port, comp);
        }
    };
};

std::unordered_map<unsigned short, HTTPServer*> listeners;

struct http_server_data {
    int port;
    Computer * comp;
};

void http_server_stop() {
    for (std::pair<unsigned short, HTTPServer *> s : listeners) { s.second->stopAll(true); delete s.second; }
}

int http_addListener(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    int port = lua_tointeger(L, 1);
    if (port < 0 || port > 65535) return 0;
    if (listeners.find(port) != listeners.end()) {
        delete listeners[port];
        listeners.erase(port);
    }
    HTTPServer * srv = new HTTPServer((HTTPRequestHandlerFactory*)new HTTPListener::Factory(get_comp(L), port), port, new HTTPServerParams);
    srv->start();
    listeners[port] = srv;
    return 0;
}

int http_removeListener(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    int port = lua_tointeger(L, 1);
    if (port < 0 || port > 65535 || listeners.find(port) == listeners.end()) return 0;
    delete listeners[port];
    listeners.erase(port);
    return 0;
}

#pragma endregion
#pragma region WebSockets

struct websocket_failure_data {
    char * url;
    const char * reason;
};

const char * websocket_failure(lua_State *L, void* userp) {
    struct websocket_failure_data * data = (struct websocket_failure_data*)userp;
    if (data->url == NULL) lua_pushnil(L);
    else { lua_pushstring(L, data->url); delete[] data->url; }
    lua_pushstring(L, data->reason);
    delete data;
    return "websocket_failure";
}

const char * websocket_closed(lua_State *L, void* userp) {
    char * url = (char*)userp;
    if (url == NULL) lua_pushnil(L);
    else lua_pushstring(L, url);
    delete[] url;
    return "websocket_closed";
}

struct ws_handle {
    bool closed;
    const char * url;
    bool binary;
    WebSocket * ws;
};

// WebSocket handle functions
int websocket_send(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    struct ws_handle * ws = (struct ws_handle*)lua_touserdata(L, lua_upvalueindex(1));
    if (ws->closed) return 0;
    if (ws->ws->sendFrame(lua_tostring(L, 1), lua_strlen(L, 1), WebSocket::FRAME_FLAG_FIN | (ws->binary ? WebSocket::FRAME_OP_BINARY : WebSocket::FRAME_OP_TEXT)) < 1) ws->closed = true;
    return 0;
}

int websocket_close(lua_State *L) {
    struct ws_handle * ws = (struct ws_handle*)lua_touserdata(L, lua_upvalueindex(1));
    ws->closed = true;
    return 0;
}

int websocket_isOpen(lua_State *L) {
    lua_pushboolean(L, !((struct ws_handle*)lua_touserdata(L, lua_upvalueindex(1)))->closed);
    return 1;
}

int websocket_free(lua_State *L) {
   ((struct ws_handle*)lua_touserdata(L, lua_upvalueindex(1)))->closed = true;
    return 0;
}

const char websocket_receive[] = "local _url, _isOpen = ...\n"
"return function()\n"
"   while true do\n"
"       if not _isOpen() then return nil end\n"
"       local ev, url, param = os.pullEvent()\n"
"       if ev == 'websocket_message' and url == _url then return param\n"
"       elseif ev == 'websocket_closed' and url == _url then return nil end\n"
"   end\n"
"end";

const char * websocket_success(lua_State *L, void* userp) {
    struct ws_handle * ws = (struct ws_handle*)userp;
    luaL_checkstack(L, 10, "Could not grow stack for websocket_success");
    if (ws->url == NULL) lua_pushnil(L);
    else lua_pushstring(L, ws->url);
    lua_newtable(L);

    lua_pushstring(L, "close");
    lua_pushlightuserdata(L, ws);
    lua_newtable(L);
    lua_pushstring(L, "__gc");
    lua_pushlightuserdata(L, ws);
    lua_pushcclosure(L, websocket_free, 1);
    lua_settable(L, -3);
    lua_setmetatable(L, -2);
    lua_pushcclosure(L, websocket_close, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "receive");
    assert(luaL_loadstring(L, websocket_receive) == 0);
    assert(lua_isfunction(L, -1));
    lua_pushstring(L, ws->url);
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

struct ws_message {
    const char * url;
    char* data;
};

const char * websocket_message(lua_State *L, void* userp) {
    struct ws_message * message = (struct ws_message*)userp;
    if (message->url == NULL) lua_pushnil(L);
    else lua_pushstring(L, message->url);
    lua_pushstring(L, message->data);
    delete[] message->data;
    delete message;
    return "websocket_message";
}

class websocket_server: public HTTPRequestHandler {
public:
    Computer * comp;
    HTTPServer *srv;
    bool binary;
    websocket_server(Computer * c, bool b, HTTPServer *s): comp(c), binary(b), srv(s) {}
    void handleRequest(HTTPServerRequest &request, HTTPServerResponse &response) {
        WebSocket * ws = NULL;
        try {
            ws = new WebSocket(request, response);
        } catch (std::exception &e) {
            struct websocket_failure_data * data = new struct websocket_failure_data;
            data->url = NULL;
            data->reason = e.what();
            termQueueProvider(comp, websocket_failure, data);
            if (ws != NULL) delete ws;
            if (srv != NULL) { srv->stop(); delete srv; }
            return;
        }
        struct ws_handle * wsh = new struct ws_handle;
        wsh->closed = false;
        wsh->ws = ws;
        wsh->url = NULL;
        wsh->binary = binary;
        termQueueProvider(comp, websocket_success, wsh);
        while (!wsh->closed) {
            Poco::Buffer<char> buf(0);
            int flags = 0;
            try {
                if (ws->receiveFrame(buf, flags) == 0) {
                    wsh->closed = true;
                    termQueueProvider(comp, websocket_closed, NULL);
                    break;
                }
            } catch (std::exception &e) {
                wsh->closed = true;
                termQueueProvider(comp, websocket_closed, NULL);
                break;
            }
            if (flags & WebSocket::FRAME_OP_CLOSE) {
                wsh->closed = true;
                termQueueProvider(comp, websocket_closed, NULL);
            } else {
                struct ws_message * message = new struct ws_message;
                message->url = NULL;
                message->data = new char[buf.sizeBytes()];
                memcpy(message->data, buf.begin(), buf.sizeBytes());
                termQueueProvider(comp, websocket_message, message);
            }
        }
        ws->shutdown();
        if (srv != NULL) { srv->stop(); delete srv; }
    }
    class Factory: public HTTPRequestHandlerFactory {
    public:
        Computer *comp;
        HTTPServer *srv = NULL;
        bool binary;
        Factory(Computer *c, bool b): comp(c), binary(b) {}
        virtual HTTPRequestHandler* createRequestHandler(const HTTPServerRequest&) {
            assert(srv != NULL);
            return new websocket_server(comp, binary, srv);
        }
    };
};

void websocket_client_thread(Computer *comp, char * str, bool binary) {
    Poco::URI uri(str);
    HTTPClientSession cs(uri.getHost(), uri.getPort());
    HTTPRequest request(HTTPRequest::HTTP_GET, uri.getPathAndQuery(), HTTPMessage::HTTP_1_1);
    request.set("origin", "http://www.websocket.org");
    HTTPResponse response;
    WebSocket* ws;
    try {
        ws = new WebSocket(cs, request, response);
    } catch (std::exception &e) {
        struct websocket_failure_data * data = new struct websocket_failure_data;
        data->url = str;
        data->reason = e.what();
        termQueueProvider(comp, websocket_failure, data);
        return;
    }
    struct ws_handle * wsh = new struct ws_handle;
    wsh->closed = false;
    wsh->url = str;
    wsh->ws = ws;
    wsh->binary = binary;
    termQueueProvider(comp, websocket_success, wsh);
    while (!wsh->closed) {
        Poco::Buffer<char> buf(0);
        int flags = 0;
        try {
            if (ws->receiveFrame(buf, flags) == 0) {
                wsh->closed = true;
                termQueueProvider(comp, websocket_closed, str);
                break;
            }
        } catch (std::exception &e) {
            wsh->closed = true;
            termQueueProvider(comp, websocket_closed, str);
            break;
        }
        if (flags & WebSocket::FRAME_OP_CLOSE) {
            wsh->closed = true;
            termQueueProvider(comp, websocket_closed, str);
        } else {
            struct ws_message * message = new struct ws_message;
            message->url = str;
            message->data = new char[buf.sizeBytes()+1];
            memcpy(message->data, buf.begin(), buf.sizeBytes());
            message->data[buf.sizeBytes()] = 0;
            termQueueProvider(comp, websocket_message, message);
        }
        std::this_thread::yield();
    }
    ws->shutdown();
}

int http_websocket(lua_State *L) {
    if (lua_isstring(L, 1)) {
        char* url = new char[lua_strlen(L, 1) + 1];
        strcpy(url, lua_tostring(L, 1));
        std::thread th(websocket_client_thread, get_comp(L), url, lua_isboolean(L, 2) && lua_toboolean(L, 2));
        setThreadName(th, "WebSocket Client Thread");
        th.detach();
    } else {
        websocket_server::Factory * f = new websocket_server::Factory(get_comp(L), lua_isboolean(L, 2) && lua_toboolean(L, 2));
        f->srv = new HTTPServer(f, 80);
        f->srv->start();
    }
    lua_pushboolean(L, true);
    return 1;
}

#pragma endregion

const char * http_keys[5] = {
    "request",
    "checkURL",
    "addListener",
    "removeListener",
    "websocket"
};

lua_CFunction http_values[5] = {
    http_request,
    http_checkURL,
    http_addListener,
    http_removeListener,
    http_websocket
};

library_t http_lib = {"http", 5, http_keys, http_values, NULL, NULL};