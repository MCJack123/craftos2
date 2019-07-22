#ifdef __cplusplus
extern "C" {
#endif
#include "lib.h"
typedef const char * (*event_provider)(lua_State *L, void* data);
extern library_t term_lib;
extern void termInit();
extern void termClose();
extern const char * termGetEvent(lua_State *L);
extern void* termRenderLoop(void*);
extern void termQueueProvider(event_provider p, void* data);
#ifdef __cplusplus
}
#endif