#include "http.h"
#include "http_handle.h"
#include "term.h"
#include "platform.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <lauxlib.h>
#include <curl/curl.h>
#include <curl/easy.h>

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
    lua_State *L;
    const char * url;
    const char * postData;
    int headers_size;
    dict_val_t * headers;
} http_param_t;

typedef struct {
    int closed;
    const char * url;
    CURL * handle;
    buffer_t buf;
    int headers_size;
    dict_val_t * headers;
} http_handle_t;

typedef struct {
    const char * url;
    const char * status;
} http_check_t;

size_t read_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    buffer_t* buf = (buffer_t*)userdata;
    size_t sz = (size * nitems + buf->offset > buf->size) ? buf->size - buf->offset : size * nitems;
    memcpy(buffer, &buf->data[buf->offset], sz);
    return sz;
}

size_t write_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    buffer_t* buf = (buffer_t*)userdata;
    if (size * nitems + buf->offset > buf->size) {
        char * newbuf = (char*)malloc(buf->size + 4096);
        if (buf->size > 0) {
            memcpy(newbuf, buf->data, buf->size);
            free(buf->data);
        }
        buf->size += 4096;
        buf->data = newbuf;
    }
    memcpy(&buf->data[buf->offset], buffer, size * nitems);
    buf->offset += size * nitems;
    return size * nitems;
}

size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    http_handle_t * handle = (http_handle_t*)userdata;
    if (size * nitems >= 8 && 
        buffer[0] == 'H' && 
        buffer[0] == 'T' && 
        buffer[0] == 'T' && 
        buffer[0] == 'P' && 
        buffer[0] == '/') {
        handle->headers_size = 0;
        free(handle->headers);
        return size * nitems;
    }
    int s = 0, e = 0;
    for (size_t i = 0; i < size * nitems; i++) {
        if (buffer[i] == ':' && s == 0) s = 2 - i, e = i + 1;
        else if (buffer[i] == ' ' && s < 0) s = abs(s);
    }
    if (e == 0 && s == 0) return size * nitems;
    dict_val_t * newhead = (dict_val_t*)malloc((handle->headers_size+1) * sizeof(dict_val_t));
    if (handle->headers_size > 0) {
        memcpy(newhead, handle->headers, handle->headers_size * sizeof(dict_val_t));
        free(handle->headers);
    }
    handle->headers = newhead;
    dict_val_t * val = &handle->headers[handle->headers_size++];
    val->key = (char*)malloc(e);
    val->value = (char*)malloc((size * nitems) - s + 1);
    memcpy((char*)val->key, buffer, e);
    memcpy((char*)val->value, &buffer[s], (size * nitems) - s);
    return size * nitems;
}

const char * http_success(lua_State *L, void* data) {
    http_handle_t * handle = (data);
    luaL_checkstack(L, 30, "Unable to allocate HTTP handle");
    lua_pushstring(L, handle->url);
    lua_newtable(L);

    lua_pushstring(L, "close");
    lua_pushlightuserdata(L, handle);
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
    lua_pushstring(L, ((http_handle_t*)data)->url);
    free(data);
    return "http_failure";
}

const char * http_check(lua_State *L, void* data) {
    http_check_t * res = (http_check_t*)data;
    lua_pushstring(L, res->url);
    lua_pushboolean(L, res->status == NULL);
    if (res->status == NULL) lua_pushnil(L);
    else lua_pushstring(L, res->status);
    free(res);
    return "http_check";
}

void* downloadThread(void* arg) {
    http_param_t* param = (http_param_t*)arg;
    http_handle_t * handle = (http_handle_t*)malloc(sizeof(http_handle_t));
    handle->url = param->url;
    handle->buf.data = NULL;
    handle->buf.size = 0;
    handle->buf.offset = 0;
    handle->closed = 0;
    handle->handle = curl_easy_init();
    curl_easy_setopt(handle->handle, CURLOPT_URL, param->url);
    if (param->postData == NULL) curl_easy_setopt(handle->handle, CURLOPT_HTTPGET, 1);
    else curl_easy_setopt(handle->handle, CURLOPT_POST, 1);
    curl_easy_setopt(handle->handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle->handle, CURLOPT_WRITEDATA, &(handle->buf));
    curl_easy_setopt(handle->handle, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(handle->handle, CURLOPT_HEADERDATA, handle);
    if (param->postData != NULL) {
        buffer_t postbuf = {strlen(param->postData), 0, (char*)param->postData};
        curl_easy_setopt(handle->handle, CURLOPT_READFUNCTION, read_callback);
        curl_easy_setopt(handle->handle, CURLOPT_READDATA, &postbuf);
    }
    struct curl_slist *header_list = NULL;
    if (param->headers != NULL) {
        for (int i = 0; i < param->headers_size; i++) {
            char * tmp = (char*)malloc(strlen(param->headers[i].key) + strlen(param->headers[i].value) + 3);
            strcpy(tmp, param->headers[i].key);
            strcat(tmp, ": ");
            strcat(tmp, param->headers[i].value);
            header_list = curl_slist_append(header_list, tmp);
            free(tmp);
        }
        curl_easy_setopt(handle->handle, CURLOPT_HTTPHEADER, header_list);
    }
    handle->headers = NULL;
    handle->headers_size = 0;
    if (curl_easy_perform(handle->handle) == CURLE_OK) {
        handle->buf.size = handle->buf.offset;
        handle->buf.offset = 0;
        termQueueProvider(http_success, handle);
    } else termQueueProvider(http_failure, handle);
    free(param);
    return NULL;
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
    termQueueProvider(http_check, res);
    free(param);
    return NULL;
}

int http_request(lua_State *L) {
    if (!config.http_enable) {
        lua_pushboolean(L, false);
        return 1;
    }
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    http_param_t * param = (http_param_t*)malloc(sizeof(http_param_t));
    param->url = lua_tostring(L, 1);
    param->postData = NULL;
    param->headers = NULL;
    param->headers_size = 0;
    if (lua_isstring(L, 2)) param->postData = lua_tostring(L, 2);
    if (lua_istable(L, 3)) {
        param->headers_size = lua_objlen(L, 3);
        param->headers = (dict_val_t*)malloc(param->headers_size * sizeof(dict_val_t));
        lua_pushvalue(L, 3);
        lua_pushnil(L);
        for (int i = 0; lua_next(L, -2); i++) {
            lua_pushvalue(L, -2);
            param->headers[i].key = lua_tostring(L, -1);
            param->headers[i].value = lua_tostring(L, -2);
            lua_pop(L, 2);
        }
        lua_pop(L, 1);
    }
#ifdef WIN32
    createThread(downloadThread, param);
#else
    free(createThread(downloadThread, param));
#endif
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
    param->L = L;
    param->url = lua_tostring(L, 1);
#ifdef WIN32
    createThread(checkThread, param);
#else
    free(createThread(checkThread, param));
#endif
    lua_pushboolean(L, true);
    return 1;
}

const char * http_keys[2] = {
    "request",
    "checkURL"
};

lua_CFunction http_values[2] = {
    http_request,
    http_checkURL
};

library_t http_lib = {"http", 2, http_keys, http_values, NULL, NULL};