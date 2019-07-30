#ifndef PLATFORM_H
#define PLATFORM_H
#ifdef __cplusplus
extern "C" {
#endif
#include <lua.h>
extern char * expandEnvironment(const char * src);
extern char * fixpath(const char * path);
extern void * createThread(void*(*func)(void*), void* arg);
extern void joinThread(void * thread);
extern int getUptime();
extern int createDirectory(const char * path);
extern void msleep(unsigned long time);
extern unsigned long long getFreeSpace(char* path);
extern void platform_fs_find(lua_State* L, char* wildcard);
extern int removeDirectory(char* path);
extern const char * rom_path;
extern const char * bios_path;
#ifdef WIN32
extern char* basename(char* path);
extern char* dirname(char* path);
#endif
#ifdef __cplusplus
}
#endif
#endif