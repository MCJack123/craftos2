/*
 * platform/darwin.cpp
 * CraftOS-PC 2
 * 
 * This file implements functions specific to macOS when run from the Terminal.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#ifdef __APPLE__ // disable error checking on Windows
extern "C" {
#include <lua.h>
}
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>
#include <ApplicationServices/ApplicationServices.h>
#include <dirent.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <libgen.h>
#include <png++/png.hpp>
#include <pthread.h>
#include <SDL2/SDL_syswm.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <wordexp.h>
#include "../platform.hpp"
#include "../util.hpp"

#ifdef CUSTOM_ROM_DIR
const char * rom_path = CUSTOM_ROM_DIR;
path_t rom_path_expanded;
#else
path_t rom_path = "/usr/local/share/craftos";
#endif
const char * base_path = "$HOME/Library/Application\\ Support/CraftOS-PC";
path_t base_path_expanded;

void setBasePath(path_t path) {
    base_path_expanded = path;
}

void setROMPath(path_t path) {
#ifdef CUSTOM_ROM_DIR
    rom_path_expanded = path;
#else
    rom_path = path;
#endif
}

path_t getBasePath() {
    if (!base_path_expanded.empty()) return base_path_expanded;
    wordexp_t p;
    wordexp(base_path, &p, 0);
    base_path_expanded = p.we_wordv[0];
    for (unsigned i = 1; i < p.we_wordc; i++) base_path_expanded += p.we_wordv[i];
    wordfree(&p);
    return base_path_expanded;
}

#ifdef CUSTOM_ROM_DIR
path_t getROMPath() {
    if (!rom_path_expanded.empty()) return rom_path_expanded;
    wordexp_t p;
    wordexp(rom_path, &p, 0);
    rom_path_expanded = p.we_wordv[0];
    for (unsigned i = 1; i < p.we_wordc; i++) rom_path_expanded += p.we_wordv[i];
    wordfree(&p);
    return rom_path_expanded;
}

path_t getPlugInPath() { return getROMPath() / "plugins-luajit"; }
#else
path_t getROMPath() { return rom_path; }
path_t getPlugInPath() { return rom_path / "plugins-luajit"; }
#endif

path_t getMCSavePath() {
    wordexp_t p;
    wordexp("$HOME/Library/Application\\ Support/minecraft/saves/", &p, 0);
    std::string expanded = p.we_wordv[0];
    for (int i = 1; i < p.we_wordc; i++) expanded += p.we_wordv[i];
    wordfree(&p);
    return expanded;
}

void setThreadName(std::thread &t, const std::string& name) {}

void updateNow(const std::string& tag_name, const Poco::JSON::Object::Ptr root) {
    fprintf(stderr, "Updating is not available on Mac terminal builds.\n");
}

static int recursiveMove(const std::string& fromDir, const std::string& toDir) {
    struct stat statbuf;
    if (!stat(fromDir.c_str(), &statbuf)) {
        if (S_ISDIR(statbuf.st_mode)) {
            std::error_code e;
            fs::create_directories(toDir, e);
            if (e) return e.value();
            DIR *d = opendir(fromDir.c_str());
            int r = -1;
            if (d) {
                struct dirent *p;
                r = 0;
                while (!r && (p=readdir(d))) {
                    /* Skip the names "." and ".." as we don't want to recurse on them. */
                    if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;
                    r = recursiveMove(fromDir + "/" + std::string(p->d_name), toDir + "/" + std::string(p->d_name));
                }
                closedir(d);
            }
            if (!r) r = rmdir(fromDir.c_str());
            return r;
        } else return rename(fromDir.c_str(), toDir.c_str());
    } else return -1;
}

void migrateOldData() {
    wordexp_t p;
    struct stat st;
    wordexp("$HOME/.craftos", &p, 0);
    std::string oldpath = p.we_wordv[0];
    for (int i = 1; i < p.we_wordc; i++) oldpath += p.we_wordv[i];
    wordfree(&p);
    if (stat(oldpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && stat(getBasePath().c_str(), &st) != 0) 
        recursiveMove(oldpath, getBasePath());
}

void copyImage(SDL_Surface* surf, SDL_Window* win) {
    png::solid_pixel_buffer<png::rgb_pixel> pixbuf(surf->w, surf->h);
    memcpy((void*)&pixbuf.get_bytes()[0], surf->pixels, surf->h * surf->pitch);
    png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel> > img(surf->w, surf->h);
    img.set_pixbuf(pixbuf);
    std::stringstream ss;
    img.write_stream(ss);
    PasteboardRef clipboard;
    PasteboardCreate(kPasteboardClipboard, &clipboard);
    PasteboardClear(clipboard);
    CFDataRef imgdata = CFDataCreate(kCFAllocatorDefault, (const uint8_t*)ss.str().c_str(), ss.str().size());
    PasteboardPutItemFlavor(clipboard, NULL, kUTTypePNG, imgdata, 0);
    CFRelease(imgdata);
    CFRelease(clipboard);
}

void handler(int sig) {
    void *array[25];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 25);

    // print out all the frames to stderr
    if (!loadingPlugin.empty()) fprintf(stderr, "Uh oh, CraftOS-PC has crashed! Reason: %s. It appears the plugin \"%s\" may have been responsible for this. Please remove it and try again.\n", strsignal(sig), loadingPlugin.c_str());
    else fprintf(stderr, "Uh oh, CraftOS-PC has crashed! Reason: %s. Please report this to https://www.craftos-pc.cc/bugreport. Paste the following text under the 'Screenshots' section:\nOS: Mac (Console build)\nLast C function: %s\n", strsignal(sig), lastCFunction);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    signal(sig, NULL);
}

void setupCrashHandler() {
    signal(SIGSEGV, handler);
    signal(SIGILL, handler);
    signal(SIGBUS, handler);
    signal(SIGTRAP, handler);
    signal(SIGABRT, handler);
}

extern void MySDL_GetDisplayDPI(int displayIndex, float* dpi, float* defaultDpi);

float getBackingScaleFactor(SDL_Window *win) {
    float dpi, defaultDpi;
    MySDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(win), &dpi, &defaultDpi);
    return dpi / defaultDpi;
}

#ifdef _X11_XLIB_H_
// thanks sgx1: https://stackoverflow.com/questions/20733215/how-to-make-a-window-always-on-top
#define _NET_WM_STATE_REMOVE        0    /* remove/unset property */
#define _NET_WM_STATE_ADD           1    /* add/set property */
#define _NET_WM_STATE_TOGGLE        2    /* toggle property  */
// change a window's _NET_WM_STATE property so that it can be kept on top.
// @display: x11 display singleton.
// @xid    : the window to set on top.
Status x11_window_set_on_top (Display* display, Window xid, bool state)
{
    XEvent event;
    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.display = display;
    event.xclient.window  = xid;
    event.xclient.message_type = XInternAtom (display, "_NET_WM_STATE", False);
    event.xclient.format = 32;

    event.xclient.data.l[0] = state ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
    event.xclient.data.l[1] = XInternAtom (display, "_NET_WM_STATE_ABOVE", False);
    event.xclient.data.l[2] = 0; //unused.
    event.xclient.data.l[3] = 0;
    event.xclient.data.l[4] = 0;

    return XSendEvent (display, DefaultRootWindow(display), False,
                       SubstructureRedirectMask|SubstructureNotifyMask, &event);
}
#endif

void setFloating(SDL_Window* win, bool state) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(win, &info);
    if (info.subsystem == SDL_SYSWM_COCOA) {
        fprintf(stderr, "Warning: Mac console builds do not support keeping windows on top.\n");
    } else if (info.subsystem == SDL_SYSWM_X11) {
        // rare, but it's possible if someone built it for XQuartz
#ifdef _X11_XLIB_H_
        x11_window_set_on_top(info.info.x11.display, info.info.x11.window, state);
#endif
    }
}

void platformExit() {}

void addSystemCertificates(Poco::Net::Context::Ptr context) {}

void unblockInput() {
    close(STDIN_FILENO);
}

#endif // __INTELLISENSE__