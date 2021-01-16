/*
 * platform/ios.mm
 * CraftOS-PC 2
 * 
 * This file implements functions specific to iOS app binaries.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

extern "C" {
#include <lua.h>
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
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/SSLException.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <png++/png.hpp>
#import <Foundation/Foundation.h>
#include "../platform.hpp"
#include "../mounter.hpp"
#include "../http.hpp"
#include "../os.hpp"

extern bool exiting;
std::string base_path_expanded;
std::string rom_path_expanded;

void setBasePath(const char * path) {
    base_path_expanded = path;
}

void setROMPath(const char * path) {
    rom_path_expanded = path;
}

std::string getBasePath() {
    if (!base_path_expanded.empty()) return rom_path_expanded;
    NSArray * paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString * path = paths[0];
    char * retval = new char[path.length + 1];
    [path getCString:retval maxLength:path.length+1 encoding:NSASCIIStringEncoding];
    base_path_expanded = retval;
    delete[] retval;
    return base_path_expanded;
}

std::string getROMPath() {
    if (!rom_path_expanded.empty()) return rom_path_expanded;
    NSString * path = [NSBundle mainBundle].resourcePath;
    char * retval = new char[path.length + 1];
    [path getCString:retval maxLength:path.length+1 encoding:NSASCIIStringEncoding];
    rom_path_expanded = retval;
    delete[] retval;
    return rom_path_expanded;
}

std::string getPlugInPath() {
    NSString * path = [NSBundle mainBundle].builtInPlugInsPath;
    char * retval = new char[path.length + 1];
    [path getCString:retval maxLength:path.length+1 encoding:NSASCIIStringEncoding];
    std::string s((const char*)retval);
    delete[] retval;
    return s;
}

std::string getMCSavePath() {
    return "";
}

void setThreadName(std::thread &t, std::string name) {}

int createDirectory(std::string path) {
    if (mkdir(path.c_str(), 0777) != 0) {
        if (errno == ENOENT && path != "/" && !path.empty()) {
            if (createDirectory(path.substr(0, path.find_last_of('/')).c_str())) return 1;
            mkdir(path.c_str(), 0777);
        } else if (errno != EEXIST) return 1;
    }
    return 0;
}

int removeDirectory(std::string path) {
    struct stat statbuf;
    if (!stat(path.c_str(), &statbuf)) {
        if (S_ISDIR(statbuf.st_mode)) {
            DIR *d = opendir(path.c_str());
            int r = -1;
            if (d) {
                struct dirent *p;
                r = 0;
                while (!r && (p=readdir(d))) {
                    /* Skip the names "." and ".." as we don't want to recurse on them. */
                    if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;
                    r = removeDirectory(path + "/" + std::string(p->d_name));
                }
                closedir(d);
            }
            if (!r) r = rmdir(path.c_str());
            return r;
        } else return unlink(path.c_str());
    } else return -1;
}

unsigned long long getFreeSpace(std::string path) {
    NSDictionary * dict = [[NSFileManager defaultManager] attributesOfFileSystemForPath:[NSString stringWithCString:path.c_str() encoding:NSASCIIStringEncoding] error:nil];
    if (dict == nil) {
        if (path.find_last_of("/") == std::string::npos || path.substr(0, path.find_last_of("/")-1).empty()) return 0;
        else return getFreeSpace(path.substr(0, path.find_last_of("/")-1));
    }
    return [(NSNumber*)dict[NSFileSystemFreeSize] unsignedLongLongValue];
}

unsigned long long getCapacity(std::string path) {
    NSDictionary * dict = [[NSFileManager defaultManager] attributesOfFileSystemForPath:[NSString stringWithCString:path.c_str() encoding:NSASCIIStringEncoding] error:nil];
    if (dict == nil) {
        if (path.find_last_of("/") == std::string::npos || path.substr(0, path.find_last_of("/")-1).empty()) return 0;
        else return getCapacity(path.substr(0, path.find_last_of("/")-1));
    }
    return [(NSNumber*)dict[NSFileSystemSize] unsignedLongLongValue];
}

void updateNow(std::string tag_name) {
    
}

void migrateData() {
    
}

void copyImage(SDL_Surface* surf) {
    /*png::solid_pixel_buffer<png::rgb_pixel> pixbuf(surf->w, surf->h);
    memcpy((void*)&pixbuf.get_bytes()[0], surf->pixels, surf->h * surf->pitch);
    png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel> > img(surf->w, surf->h);
    img.set_pixbuf(pixbuf);
    std::stringstream ss;
    img.write_stream(ss);
    NSData * nsdata = [NSData dataWithBytes:ss.str().c_str() length:surf->w*surf->h*3];
    [[NSPasteboard generalPasteboard] clearContents];
    [[NSPasteboard generalPasteboard] setData:nsdata forType:NSPasteboardTypePNG];
    [nsdata release];*/
}

void handler(int sig) {
    void *array[25];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 25);

    // print out all the frames to stderr
    fprintf(stderr, "Uh oh, CraftOS-PC has crashed! Reason: %s. Please report this to https://www.craftos-pc.cc/bugreport. Paste the following text under the 'Screenshots' section:\nOS: Mac (Application)\n", strsignal(sig));
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    signal(sig, NULL);
}

void setupCrashHandler() {
    signal(SIGSEGV, handler);
    signal(SIGILL, handler);
    signal(SIGBUS, handler);
    signal(SIGTRAP, handler);
}

float getBackingScaleFactor(SDL_Window *win) {
    /*SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(win, &info);
    if (info.info.cocoa.window.screen == nil) return 1.0f;
    if ([info.info.cocoa.window.screen respondsToSelector:@selector(backingScaleFactor)])  // Mac OS X 10.7 and later
        return [info.info.cocoa.window.screen backingScaleFactor];*/
    return 1.0f;
}
