#include "http.h"
#include "os.h"
#include <curl/curl.h>
#include <curl/easy.h>

typedef struct {
    const char * key;
    const char * value;
} dict_val_t;

typedef struct {
    const char * url;
    const char * postData;
    int headers_size;
    dict_val_t * headers;
} http_param_t;

int http_request(lua_State *L) {
    http_param_t param;
    param.url = lua_tostring(L, 1);
    param.postData = NULL;
    param.headers = NULL;
    param.headers_size = 0;
    if (lua_isstring(L, 2)) param.postData = lua_tostring(L, 2);
    if (lua_istable(L, 3)) {
        param.headers_size = lua_objlen(L, 3);
        param.headers = (dict_val_t*)malloc(param.headers_size * sizeof(dict_val_t));
        lua_pushvalue(L, 3);
        lua_pushnil(L);
        for (int i = 0; lua_next(L, -2); i++) {
            lua_pushvalue(L, -2);
            param.headers[i].key = lua_tostring(L, -1);
            param.headers[i].value = lua_tostring(L, -2);
            lua_pop(L, 2);
        }
        lua_pop(L, 1);
    }
    
}

const char * http_keys[1] = {
    "request"
};

lua_CFunction http_values[1] = {
    http_request
};

library_t http_lib = {"http", 1, http_keys, http_values};