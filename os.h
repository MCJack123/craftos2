#include "lib.h"
extern library_t os_lib;
extern int getNextEvent(lua_State *L, const char * filter);
extern void queueEvent(const char * name, lua_State *param);
extern int running;