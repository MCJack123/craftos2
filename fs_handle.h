#include <lua.h>
extern int handle_close(lua_State *L);
extern int handle_readAll(lua_State *L);
extern int handle_readLine(lua_State *L);
extern int handle_readChar(lua_State *L);
extern int handle_readByte(lua_State *L);
extern int handle_writeString(lua_State *L);
extern int handle_writeLine(lua_State *L);
extern int handle_writeByte(lua_State *L);
extern int handle_flush(lua_State *L);