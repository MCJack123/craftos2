/*
 * platform/macapp.mm
 * CraftOS-PC 2
 * 
 * This file implements functions specific to macOS app binaries.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

extern "C" {
#include <lua.h>
}
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <libgen.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <wordexp.h>
#include <png++/png.hpp>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/SSLException.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include "../platform.hpp"
#include "../util.hpp"
#include "../runtime.hpp"
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

extern bool exiting;
std::string rom_path_expanded;
const char * customBasePath = NULL;

void setBasePath(const char * path) {
    customBasePath = path;
}

void setROMPath(const char * path) {
    rom_path_expanded = path;
}

std::string getBasePath() {
    if (customBasePath != NULL) return customBasePath;
    return std::string([[[NSFileManager defaultManager] 
                         URLForDirectory:NSApplicationSupportDirectory 
                         inDomain:NSUserDomainMask 
                         appropriateForURL:[NSURL fileURLWithPath:@"/"] 
                         create:NO 
                         error:nil
                        ] fileSystemRepresentation]) + "/CraftOS-PC";
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
    return s + "/";
}

std::string getMCSavePath() {
    if (customBasePath != NULL) return customBasePath;
    return std::string([[[NSFileManager defaultManager] 
                         URLForDirectory:NSApplicationSupportDirectory 
                         inDomain:NSUserDomainMask 
                         appropriateForURL:[NSURL fileURLWithPath:@"/"] 
                         create:NO 
                         error:nil
                        ] fileSystemRepresentation]) + "/minecraft/saves/";
}

void setThreadName(std::thread &t, const std::string& name) {}

int createDirectory(const std::string& path) {
    if (mkdir(path.c_str(), 0777) != 0) {
        if (errno == ENOENT && path != "/" && !path.empty()) {
            if (createDirectory(path.substr(0, path.find_last_of('/')).c_str())) return 1;
            mkdir(path.c_str(), 0777);
        } else if (errno != EEXIST) return 1;
    }
    return 0;
}

int removeDirectory(const std::string& path) {
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

unsigned long long getFreeSpace(const std::string& path) {
    NSDictionary * dict = [[NSFileManager defaultManager] attributesOfFileSystemForPath:[NSString stringWithCString:path.c_str() encoding:NSASCIIStringEncoding] error:nil];
    if (dict == nil) {
        if (path.substr(0, path.find_last_of("/")-1).empty()) return 0;
        else return getFreeSpace(path.substr(0, path.find_last_of("/")-1));
    }
    return [(NSNumber*)dict[NSFileSystemFreeSize] unsignedLongLongValue];
}

unsigned long long getCapacity(const std::string& path) {
    NSDictionary * dict = [[NSFileManager defaultManager] attributesOfFileSystemForPath:[NSString stringWithCString:path.c_str() encoding:NSASCIIStringEncoding] error:nil];
    if (dict == nil) {
        if (path.substr(0, path.find_last_of("/")-1).empty()) return 0;
        else return getCapacity(path.substr(0, path.find_last_of("/")-1));
    }
    return [(NSNumber*)dict[NSFileSystemSize] unsignedLongLongValue];
}

CGRect CGRectCreate(CGFloat x, CGFloat y, CGFloat width, CGFloat height) {
    CGRect retval;
    retval.origin.x = x;
    retval.origin.y = y;
    retval.size.width = width;
    retval.size.height = height;
    return retval;
}

@interface UpdateViewController : NSViewController
- (void) loadView;
@end

@implementation UpdateViewController
- (void) loadView {
    self.view = [[NSView alloc] initWithFrame:CGRectCreate(0, 20, 480, 88)];
    NSImageView * img = [[NSImageView alloc] initWithFrame:CGRectCreate(20, 20, 48, 48)];
    img.image = [[NSBundle mainBundle] imageForResource:@"CraftOS-PC"];
    [self.view addSubview:img];

    NSTextField * label = [[NSTextField alloc] initWithFrame:CGRectCreate(76, 52, 144, 16)];
    label.stringValue = @"Downloading update...";
    label.editable = NO;
    label.drawsBackground = NO;
    label.bezeled = NO;
    label.bordered = NO;
    [label setFont:[NSFont systemFontOfSize:13.0 weight:NSFontWeightMedium]];
    [self.view addSubview:label];

    NSProgressIndicator * bar = [[NSProgressIndicator alloc] initWithFrame:CGRectCreate(76, 25, 384, 20)];
    bar.indeterminate = true;
    bar.style = NSProgressIndicatorStyleBar;
    [self.view addSubview:bar];
    [bar startAnimation:nil];
}
@end

void updateNow(const std::string& tag_name) {
    UpdateViewController * vc = [[UpdateViewController alloc] initWithNibName:nil bundle:[NSBundle mainBundle]];
    NSWindow* win = [NSWindow windowWithContentViewController:vc];
    [win setFrame:CGRectCreate(win.frame.origin.x, win.frame.origin.y, 480, 103) display:YES];
    [win setTitle:@"Updating..."];
    win.titleVisibility = NSWindowTitleHidden;
    win.minSize = {480, 103};
    win.maxSize = {480, 103};
    win.releasedWhenClosed = YES;
    [win makeKeyAndOrderFront:NSApp];
    HTTPDownload("https://github.com/MCJack123/craftos2/releases/download/" + tag_name + "/CraftOS-PC.dmg", [win](std::istream& in) {
        @autoreleasepool {
            NSString *tempFileTemplate = [NSTemporaryDirectory() stringByAppendingPathComponent:@"CraftOS-PC.dmg"];
            const char *tempFileTemplateCString = [tempFileTemplate fileSystemRepresentation];
            std::ofstream out(tempFileTemplateCString, std::ios::binary);
            char c = in.get();
            while (in.good()) {out.put(c); c = in.get();}
            out.close();
            fprintf(stderr, "Wrote: %s\n", tempFileTemplateCString);
            NSError * err = nil;
            NSUserUnixTask * task = [[NSUserUnixTask alloc] initWithURL:[NSURL fileURLWithPath:@"/usr/bin/hdiutil" isDirectory:false] error:&err];
            if (err != nil) {
                NSLog(@"Could not open mount task: %@\n", [err localizedDescription]);
                [win close];
                if (exiting) exit(0);
                return;
            }
            NSPipe * pipe = [NSPipe pipe];
            task.standardOutput = [pipe fileHandleForWriting];
            [task executeWithArguments:@[@"attach", @"-plist", tempFileTemplate] completionHandler:^(NSError *error){
                NSError * err2 = nil;
                if (error == NULL) {
                    NSDictionary * res = [NSPropertyListSerialization propertyListWithData:[[pipe fileHandleForReading] readDataToEndOfFile] options:NSPropertyListImmutable format:nil error:&err2];
                    if (err2 != nil) {
                        NSLog(@"Could not read property list: %@\n", [err2 localizedDescription]);
                        [win close];
                        if (exiting) exit(0);
                        return;
                    }
                    NSString * pathstr = NULL;
                    for (int i = 0; i < ((NSArray*)res[@"system-entities"]).count; i++)
                        if ([((NSDictionary*)res[@"system-entities"][i]) valueForKey:@"mount-point"] != nil)
                            pathstr = res[@"system-entities"][i][@"mount-point"];
                    if (pathstr == NULL) {
                        NSLog(@"Could not find mount point: %@\n", [err2 localizedDescription]);
                        [win close];
                        if (exiting) exit(0);
                        return;
                    }
                    NSURL * path = [NSURL fileURLWithPath:@".install" relativeToURL:[NSURL fileURLWithPath:pathstr isDirectory:true]];
                    if (![[NSFileManager defaultManager] isReadableFileAtPath:[path path]]) {
                        [res retain];
                        [path retain];
                        queueTask([win, path, res, pathstr](void*)->void*{
                            NSLog(@"Could not find %@\n", [path path]); 
                            system((std::string("/usr/bin/hdiutil detach ") + [pathstr cStringUsingEncoding:NSASCIIStringEncoding]).c_str());
                            [res release];
                            [path release];
                            NSAlert * alert = [[NSAlert alloc] init];
                            alert.informativeText = @"This version does not support auto updating. Please go to https://github.com/MCJack123/craftos2/releases to install manually.";
                            alert.messageText = @"Update failed";
                            [alert beginSheetModalForWindow:win completionHandler:^(NSModalResponse returnCode) {
                                [alert.window close];
                                SDL_AddTimer(500, [](Uint32 interval, void* win)->Uint32{[((NSWindow*)win) close]; if (exiting) exit(0); return interval;}, win);
                            }];
                            return NULL;
                        }, NULL);
                        return;
                    }
                    int pid = fork();
                    if (pid < 0) fprintf(stderr, "Could not fork: %d\n", pid);
                    else if (pid == 0) {
                        if ([[NSFileManager defaultManager] isWritableFileAtPath:[NSBundle mainBundle].bundlePath]) system(("/bin/sh " + std::string([path fileSystemRepresentation]) + " " + std::string([[NSBundle mainBundle].bundlePath fileSystemRepresentation])).c_str());
                        else system(("/usr/bin/osascript -e 'do shell script \"/bin/sh " + std::string([path fileSystemRepresentation]) + " " + std::string([[NSBundle mainBundle].bundlePath fileSystemRepresentation]) + "\" with administrator privileges'").c_str());
                        exit(0);
                    }
                    [win close];
                    exit(0);
                } else NSLog(@"Error: %@\n", [error localizedDescription]);
                [win close];
            }];
        }
    });
}

void migrateData() {
    wordexp_t p;
    struct stat st;
    wordexp("$HOME/.craftos", &p, 0);
    std::string oldpath = p.we_wordv[0];
    for (int i = 1; i < p.we_wordc; i++) oldpath += p.we_wordv[i];
    wordfree(&p);
    if (stat(oldpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && stat(getBasePath().c_str(), &st) != 0) 
        [[NSFileManager defaultManager] moveItemAtPath:[NSString stringWithCString:oldpath.c_str() encoding:NSASCIIStringEncoding] toPath:[NSString stringWithCString:getBasePath().c_str() encoding:NSASCIIStringEncoding] error:nil];
}

void copyImage(SDL_Surface* surf) {
    png::solid_pixel_buffer<png::rgb_pixel> pixbuf(surf->w, surf->h);
    memcpy((void*)&pixbuf.get_bytes()[0], surf->pixels, surf->h * surf->pitch);
    png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel> > img(surf->w, surf->h);
    img.set_pixbuf(pixbuf);
    std::stringstream ss;
    img.write_stream(ss);
    NSData * nsdata = [NSData dataWithBytes:ss.str().c_str() length:surf->w*surf->h*3];
    [[NSPasteboard generalPasteboard] clearContents];
    [[NSPasteboard generalPasteboard] setData:nsdata forType:NSPasteboardTypePNG];
    [nsdata release];
}

void handler(int sig) {
    void *array[25];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 25);

    // print out all the frames to stderr
    if (!loadingPlugin.empty()) fprintf(stderr, "Uh oh, CraftOS-PC has crashed! Reason: %s. It appears the plugin \"%s\" may have been responsible for this. Please remove it and try again.\n", strsignal(sig), loadingPlugin.c_str());
    else fprintf(stderr, "Uh oh, CraftOS-PC has crashed! Reason: %s. Please report this to https://www.craftos-pc.cc/bugreport. Paste the following text under the 'Screenshots' section:\nOS: Mac (Application)\nLast C function: %s\n", strsignal(sig), lastCFunction);
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

float getBackingScaleFactor(SDL_Window *win) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(win, &info);
    if (info.info.cocoa.window.screen == nil) return 1.0f;
    if ([info.info.cocoa.window.screen respondsToSelector:@selector(backingScaleFactor)])  // Mac OS X 10.7 and later
        return [info.info.cocoa.window.screen backingScaleFactor];
    return 1.0f;
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
        info.info.cocoa.window.level = state ? NSFloatingWindowLevel : NSNormalWindowLevel;
    } else if (info.subsystem == SDL_SYSWM_X11) {
        // rare, but it's possible if someone built it for XQuartz
#ifdef _X11_XLIB_H_
        x11_window_set_on_top(info.info.x11.display, info.info.x11.window, state);
#endif
    }
}
