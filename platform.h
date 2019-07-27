#ifndef PLATFORM_H
#define PLATFORM_H
#ifdef __cplusplus
extern "C" {
#endif
extern char * expandEnvironment(const char * src);
extern char * fixpath(const char * path);
extern void * createThread(void*(*func)(void*), void* arg);
extern void joinThread(void * thread);
extern int getUptime();
extern int createDirectory(const char * path);
extern void msleep(unsigned long time);
extern const char * rom_path;
extern const char * bios_path;
#ifdef __cplusplus
}
#endif
#endif