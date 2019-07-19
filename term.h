#ifdef __cplusplus
extern "C" {
#endif
#include "lib.h"
extern library_t term_lib;
extern void termInit();
extern void termClose();
extern const char * termGetEvent(lua_State *L);
extern void* termRenderLoop(void*);
#ifdef __cplusplus
}
#endif