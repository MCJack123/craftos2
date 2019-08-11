extern "C" {
#include <lua.h>
#include "platform.h"
}
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <libgen.h>
#include <pthread.h>
#include <glob.h>
#include <dirent.h>
#include <string>
#include <vector>
#include <sstream>
#import <Foundation/Foundation.h>

const char * base_path = "$HOME/.craftos";
char * base_path_expanded = NULL;
char * rom_path_expanded = NULL;

extern "C" {

void platformFree() {
    if (base_path_expanded != NULL) free(base_path_expanded);
	if (rom_path_expanded != NULL) free(rom_path_expanded);
}

const char * getBasePath() {
    if (base_path_expanded != NULL) return base_path_expanded;
    wordexp_t p;
    wordexp(base_path, &p, 0);
    int size = 0;
    for (int i = 0; i < p.we_wordc; i++) size += strlen(p.we_wordv[i]);
    char * retval = (char*)malloc(size + 1);
    strcpy(retval, p.we_wordv[0]);
    for (int i = 1; i < p.we_wordc; i++) strcat(retval, p.we_wordv[i]);
    base_path_expanded = retval;
    return retval;
}

const char * getROMPath() {
	if (rom_path_expanded != NULL) return rom_path_expanded;
	NSString * path = [NSBundle mainBundle].resourcePath;
    char * retval = (char*)malloc(path.length + 1);
    [path getCString:retval maxLength:path.length+1 encoding:NSASCIIStringEncoding];
	rom_path_expanded = retval;
    return retval;
}

char * getBIOSPath() {
    NSString * path = [[NSBundle mainBundle] pathForResource:@"bios" ofType:@"lua"];
    char * retval = (char*)malloc(path.length + 1);
    [path getCString:retval maxLength:path.length+1 encoding:NSASCIIStringEncoding];
    return retval;
}

void * createThread(void*(*func)(void*), void* arg) {
    pthread_t * tid = new pthread_t;
    pthread_create(tid, NULL, func, arg);
    return (void*)tid;
}

void joinThread(void* thread) {
    pthread_join(*(pthread_t*)thread, NULL);
    delete (pthread_t*)thread;
}

int createDirectory(const char * path) {
    if (mkdir(path, 0777) != 0) {
        if (errno == ENOENT && strcmp(path, "/") != 0) {
            char * dir = (char*)malloc(strlen(path) + 1);
            strcpy(dir, path);
            if (createDirectory(dirname(dir))) return 1;
            free(dir);
            mkdir(path, 0777);
        } else return 1;
    }
    return 0;
}

int removeDirectory(char *path) {
    struct stat statbuf;
    if (!stat(path, &statbuf)) {
        if (S_ISDIR(statbuf.st_mode)) {
            DIR *d = opendir(path);
            size_t path_len = strlen(path);
            int r = -1;
            if (d) {
                struct dirent *p;
                r = 0;
                while (!r && (p=readdir(d))) {
                    int r2 = -1;
                    char *buf;
                    size_t len;

                    /* Skip the names "." and ".." as we don't want to recurse on them. */
                    if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;
                    len = path_len + strlen(p->d_name) + 2; 
                    buf = (char*)malloc(len);
                    if (buf) {
                        snprintf(buf, len, "%s/%s", path, p->d_name);
                        r2 = removeDirectory(buf);
                        free(buf);
                    }
                    r = r2;
                }
                closedir(d);
            }
            if (!r) r = rmdir(path);
            return r;
        } else return unlink(path);
    } else return -1;
}

void msleep(unsigned long time) {
    usleep(time * 1000);
}

unsigned long long getFreeSpace(char* path) {
	struct statvfs st;
	if (statvfs(path, &st) != 0) return 0;
	return st.f_bavail * st.f_bsize;
}

void platform_fs_find(lua_State* L, char* wildcard) {
	glob_t g;
	int rval = 0;
	rval = glob(wildcard, 0, NULL, &g);
	if (rval == 0) {
        int l = [NSBundle mainBundle].resourcePath.length;
		for (int i = 0; i < g.gl_pathc; i++) {
			lua_pushnumber(L, i + 1);
			lua_pushstring(L, &g.gl_pathv[i][l + 1]);
			lua_settable(L, -3);
		}
		globfree(&g);
	}
}

void pushHostString(lua_State *L) {
    struct utsname host;
    uname(&host);
    lua_pushfstring(L, "%s %s %s", host.sysname, "i386", host.release);
}
}