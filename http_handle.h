#include <lua.h>
extern int http_handle_close(lua_State *L);
extern int http_handle_readAll(lua_State *L);
extern int http_handle_readLine(lua_State *L);
extern int http_handle_read(lua_State *L);
extern int http_handle_getResponseCode(lua_State *L);
extern int http_handle_getResponseHeaders(lua_State *L);