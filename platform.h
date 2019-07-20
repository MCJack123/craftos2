extern char * expandEnvironment(const char * src);
extern char * fixpath(const char * path);
extern void * createThread(void*(*func)(void*));
extern void joinThread(void * thread);
extern int getUptime();
extern const char * rom_path;
extern const char * bios_path;