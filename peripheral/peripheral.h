#ifndef PERIPHERAL_H
#define PERIPHERAL_H
#ifdef __cplusplus
extern "C" {
#endif
#include "../lib.h"
extern library_t peripheral_lib;
extern void peripheral_update();

#ifdef __cplusplus
}
class peripheral {
public:
    peripheral() {} // unused
    peripheral(lua_State *L, const char * side) {}
    ~peripheral() {}
    virtual int call(lua_State *L, const char * method)=0;
    virtual void update()=0;
    virtual library_t getMethods()=0;
};
#endif
#endif