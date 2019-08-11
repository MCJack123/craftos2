#include "httplib.h"
#include "platform.h"
#include "term.h"
#include <unordered_map>
#include <chrono>

struct http_req {
    int pos;
    const httplib::Request * req;
};

struct http_res {
    bool open;
    std::string body;
    httplib::Response * res;
};

int req_read(lua_State *L) {
    struct http_req * req = (struct http_req*)lua_touserdata(L, lua_upvalueindex(1));
    if (req == NULL || req->pos >= req->req->body.size()) return 0;
    lua_pushstring(L, req->req->body.substr(req->pos++, 1).c_str());
    return 1;
}

int req_readLine(lua_State *L) {
    struct http_req * req = (struct http_req*)lua_touserdata(L, lua_upvalueindex(1));
    if (req == NULL || req->pos >= req->req->body.size()) return 0;
    int p = req->req->body.find('\n', req->pos);
    if (p == std::string::npos) {
        lua_pushstring(L, req->req->body.substr(req->pos).c_str());
        req->pos = req->req->body.size();
    } else {
        lua_pushstring(L, req->req->body.substr(req->pos, p - req->pos - 1).c_str());
        req->pos = p + 1;
    }
    return 1;
}

int req_readAll(lua_State *L) {
    struct http_req * req = (struct http_req*)lua_touserdata(L, lua_upvalueindex(1));
    if (req == NULL || req->pos >= req->req->body.size()) return 0;
    lua_pushstring(L, req->req->body.substr(req->pos).c_str());
    req->pos = req->req->body.size();
    return 1;
}

int req_close(lua_State *L) {
    return 0;
}

int req_getURL(lua_State *L) {
    struct http_req * req = (struct http_req*)lua_touserdata(L, lua_upvalueindex(1));
    if (req == NULL) return 0;
    lua_pushstring(L, req->req->path.c_str());
    return 1;
}

int req_getMethod(lua_State *L) {
    struct http_req * req = (struct http_req*)lua_touserdata(L, lua_upvalueindex(1));
    if (req == NULL) return 0;
    lua_pushstring(L, req->req->method.c_str());
    return 1;
}

int req_getRequestHeaders(lua_State *L) {
    struct http_req * req = (struct http_req*)lua_touserdata(L, lua_upvalueindex(1));
    if (req == NULL) return 0;
    lua_newtable(L);
    for (auto h : req->req->headers) {
        lua_pushstring(L, h.first.c_str());
        lua_pushstring(L, h.second.c_str());
        lua_settable(L, -3);
    }
    return 1;
}

int res_write(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    struct http_res * res = (struct http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (res == NULL || !res->open) return 0;
    res->body += lua_tostring(L, 1);
    return 0;
}

int res_writeLine(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    struct http_res * res = (struct http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (res == NULL || !res->open) return 0;
    res->body += lua_tostring(L, 1);
    res->body += "\n";
    return 0;
}

int res_close(lua_State *L) {
    struct http_res * res = (struct http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (res == NULL || !res->open) return 0;
    if (res->res->has_header("Content-Type")) res->res->set_content(res->body, res->res->get_header_value("Content-Type").c_str());
    else res->res->set_content(res->body, "text/plain");
    res->open = false;
    return 0;
}

int res_setStatusCode(lua_State *L) {
    if (!lua_isnumber(L, 1)) bad_argument(L, "number", 1);
    struct http_res * res = (struct http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (res == NULL || !res->open) return 0;
    res->res->status = lua_tonumber(L, 1);
    return 0;
}

int res_setResponseHeader(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    if (!lua_isstring(L, 2)) bad_argument(L, "string", 2);
    struct http_res * res = (struct http_res*)lua_touserdata(L, lua_upvalueindex(1));
    if (res == NULL || !res->open) return 0;
    res->res->set_header(lua_tostring(L, 1), lua_tostring(L, 2));
    return 0;
}

struct http_request_data {
    int port;
    struct http_req * req;
    struct http_res * res;
};

const char * http_request_event(lua_State *L, void* userp) {
    struct http_request_data* data = (struct http_request_data*)userp;
    lua_pushinteger(L, data->port);
    lua_newtable(L);
    
    lua_pushstring(L, "read");
    lua_pushlightuserdata(L, data->req);
    lua_pushcclosure(L, req_read, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "readLine");
    lua_pushlightuserdata(L, data->req);
    lua_pushcclosure(L, req_readLine, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "readAll");
    lua_pushlightuserdata(L, data->req);
    lua_pushcclosure(L, req_readAll, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "close");
    lua_pushlightuserdata(L, data->req);
    lua_pushcclosure(L, req_close, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "getURL");
    lua_pushlightuserdata(L, data->req);
    lua_pushcclosure(L, req_getURL, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "getMethod");
    lua_pushlightuserdata(L, data->req);
    lua_pushcclosure(L, req_getMethod, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "getRequestHeaders");
    lua_pushlightuserdata(L, data->req);
    lua_pushcclosure(L, req_getRequestHeaders, 1);
    lua_settable(L, -3);

    lua_newtable(L);

    lua_pushstring(L, "write");
    lua_pushlightuserdata(L, data->res);
    lua_pushcclosure(L, res_write, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "writeLine");
    lua_pushlightuserdata(L, data->res);
    lua_pushcclosure(L, res_writeLine, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "close");
    lua_pushlightuserdata(L, data->res);
    lua_pushcclosure(L, res_close, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "setStatusCode");
    lua_pushlightuserdata(L, data->res);
    lua_pushcclosure(L, res_setStatusCode, 1);
    lua_settable(L, -3);

    lua_pushstring(L, "setResponseHeader");
    lua_pushlightuserdata(L, data->res);
    lua_pushcclosure(L, res_setResponseHeader, 1);
    lua_settable(L, -3);

    return "http_request";
}

class HTTPListener {
public:
    httplib::Server server;
    bool running = true;
    int port;
    void callback(const httplib::Request& req, httplib::Response& res) {
        printf("Got request: %s\n", req.path.c_str());
        if (!running) server.stop();
        struct http_req lreq = {0, &req};
        struct http_res lres = {true, "", &res};
        struct http_request_data evdata = {port, &lreq, &lres};
        termQueueProvider(http_request_event, &evdata);
        std::chrono::system_clock::time_point start = std::chrono::system_clock::now();
        while (lres.open && std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - start).count() < 15) {}
        if (lres.open) {
            if (res.has_header("Content-Type")) res.set_content(lres.body, res.get_header_value("Content-Type").c_str());
            else res.set_content(lres.body, "text/plain");
            lres.open = false;
        }
    }
};

std::unordered_map<unsigned short, HTTPListener*> listeners;

void * httpListener(void* data) {
    int port = (int)data;
    HTTPListener * listener = new HTTPListener();
    listeners[port] = listener;
    listener->port = port;
    listener->server.Get(R"(.+)", [=](const httplib::Request& req, httplib::Response& res) { listener->callback(req, res);});
    listener->server.Post(R"(.+)", [=](const httplib::Request& req, httplib::Response& res) { listener->callback(req, res); });
    printf("Listening on port %d\n", port);
    listener->server.listen("localhost", port);
    delete listener;
    return NULL;
}

extern "C" void http_startServer(int port) {
    if (port < 0 || port > 65535) return;
    createThread(httpListener, (void*)port);
}

extern "C" void http_stopServer(int port) {
    if (port < 0 || port > 65535 || listeners.find(port) == listeners.end()) return;
    listeners[port]->running = false;
    listeners[port]->server.stop();
}