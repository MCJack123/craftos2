/*
 * platform/macapp.mm
 * CraftOS-PC 2
 * 
 * This file implements functions specific to macOS app binaries.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
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
#include <Poco/SHA2Engine.h>
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
        if (path.find_last_of("/") == std::string::npos || path.substr(0, path.find_last_of("/")-1).empty()) return 0;
        else return getFreeSpace(path.substr(0, path.find_last_of("/")-1));
    }
    return [(NSNumber*)dict[NSFileSystemFreeSize] unsignedLongLongValue];
}

unsigned long long getCapacity(const std::string& path) {
    NSDictionary * dict = [[NSFileManager defaultManager] attributesOfFileSystemForPath:[NSString stringWithCString:path.c_str() encoding:NSASCIIStringEncoding] error:nil];
    if (dict == nil) {
        if (path.find_last_of("/") == std::string::npos || path.substr(0, path.find_last_of("/")-1).empty()) return 0;
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
@property(retain) NSProgressIndicator * bar;
@property(retain) NSTextField * label;
- (void) loadView;
@end

@implementation UpdateViewController
- (void) loadView {
    self.view = [[NSView alloc] initWithFrame:CGRectCreate(0, 20, 480, 88)];
    NSImageView * img = [[NSImageView alloc] initWithFrame:CGRectCreate(20, 15, 48, 48)];
    img.image = [[NSBundle mainBundle] imageForResource:@"CraftOS-PC"];
    [self.view addSubview:img];

    self.label = [[NSTextField alloc] initWithFrame:CGRectCreate(76, 47, 400, 16)];
    self.label.stringValue = @"Downloading update...";
    self.label.editable = NO;
    self.label.drawsBackground = NO;
    self.label.bezeled = NO;
    self.label.bordered = NO;
    [self.label setFont:[NSFont systemFontOfSize:13.0 weight:NSFontWeightMedium]];
    [self.view addSubview:self.label];

    self.bar = [[NSProgressIndicator alloc] initWithFrame:CGRectCreate(76, 20, 384, 20)];
    self.bar.indeterminate = NO;
    self.bar.usesThreadedAnimation = YES;
    self.bar.style = NSProgressIndicatorStyleBar;
    [self.view addSubview:self.bar];
}
@end

static std::string makeSize(double n) {
    if (n >= 100) return std::to_string((long)floor(n));
    else return std::to_string(n).substr(0, 4);
}

void updateNow(const std::string& tag_name, const Poco::JSON::Object::Ptr root) {
    UpdateViewController * vc = [[UpdateViewController alloc] initWithNibName:nil bundle:[NSBundle mainBundle]];
    NSWindow* win = [NSWindow windowWithContentViewController:vc];
    [win setFrame:CGRectCreate(win.frame.origin.x, win.frame.origin.y, 480, 103) display:YES];
    [win setTitle:@"Updating..."];
    win.titleVisibility = NSWindowTitleHidden;
    win.minSize = {480, 103};
    win.maxSize = {480, 103};
    win.releasedWhenClosed = YES;
    [win makeKeyAndOrderFront:NSApp];
    HTTPDownload("https://github.com/MCJack123/craftos2/releases/download/" + tag_name + "/sha256-hashes.txt", [win, tag_name, vc](std::istream * shain, Poco::Exception * e, Poco::Net::HTTPResponse * res) {
        if (e != NULL) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Update Error", std::string("An error occurred while downloading the update: " + e->displayText()).c_str(), NULL);
            [win close];
            return;
        }
        std::string line;
        bool found = false;
        while (!shain->eof()) {
            std::getline(*shain, line);
            if (line.find("CraftOS-PC.dmg") != std::string::npos) {found = true; break;}
        }
        if (!found) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Update Error", "A file required for verification could not be downloaded sucessfully. Please download the installer manually.", NULL);
            [win close];
            return;
        }
        std::string hash = line.substr(0, 64);
        HTTPDownload("https://github.com/MCJack123/craftos2/releases/download/" + tag_name + "/CraftOS-PC.dmg", [hash, win, vc](std::istream * in, Poco::Exception * e, Poco::Net::HTTPResponse * res) {
            if (e != NULL) {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Update Error", std::string("An error occurred while downloading the update: " + e->displayText()).c_str(), NULL);
                [win close];
                return;
            }
            size_t totalSize = res->getContentLength64();
            std::string label = "Downloading update... (0.0 / " + makeSize(totalSize / 1048576.0) + " MB, 0 B/s)";
            std::string data;
            data.reserve(totalSize);
            char buf[2048];
            size_t total = 0;
            size_t bps = 0;
            size_t lastSecondSize = 0;
            std::chrono::system_clock::time_point lastSecond = std::chrono::system_clock::now();
            while (in->good() && !in->eof()) {
                in->read(buf, 2048);
                size_t sz = in->gcount();
                data += std::string(buf, sz);
                total += sz;
                if (std::chrono::system_clock::now() - lastSecond >= std::chrono::milliseconds(50) || in->eof()) {
                    bps = (total - lastSecondSize) * 20;
                    lastSecondSize = total;
                    lastSecond = std::chrono::system_clock::now();
                    label = "Downloading update... (" + makeSize(total / 1048576.0) + " / " + makeSize(totalSize / 1048576.0) + " MB, ";
                    if (bps >= 1048576) label += makeSize(bps / 1048576.0) + " MB/s)";
                    else if (bps >= 1024) label += makeSize(bps / 1024.0) + " kB/s)";
                    else label += std::to_string(bps) + " B/s)";
                    vc.label.stringValue = [NSString stringWithCString:label.c_str() encoding:NSASCIIStringEncoding];
                    vc.bar.doubleValue = (double)total / (double)totalSize * 100.0;
                    SDL_PumpEvents();
                }
            }
            vc.label.stringValue = @"Installing...";
            vc.bar.doubleValue = 0.0;
            vc.bar.needsDisplay = YES;
            // This sleep is very specific and necessary, and any smaller value
            // will cause the bar to not be indeterminate for some reason.
            // 285's the lowest I got to work, but it's less reliable below 300.
            // Instead, it will just slowly blink. Why? I have no idea.
            // I hope nobody minds losing 300 milliseconds of their life for a loading bar.
            // The rest of the code needs to be the same too - DO NOT CHANGE.
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            [vc.bar setIndeterminate:YES];
            vc.bar.needsDisplay = YES;
            SDL_PumpEvents();
            [vc.bar startAnimation:nil];
            SDL_PumpEvents();

            Poco::SHA2Engine engine;
            engine.update(data);
            std::string myhash = Poco::SHA2Engine::digestToHex(engine.digest());
            if (hash != myhash) {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Update Error", "The installer file could not be verified. Please try again later. If this issue persists, please download the installer manually.", NULL);
                [win close];
                return;
            }
            @autoreleasepool {
                NSString *tempFileTemplate = [NSTemporaryDirectory() stringByAppendingPathComponent:@"CraftOS-PC.dmg"];
                const char *tempFileTemplateCString = [tempFileTemplate fileSystemRepresentation];
                std::ofstream out(tempFileTemplateCString, std::ios::binary);
                out << data;
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
    });
}

void migrateOldData() {
    wordexp_t p;
    struct stat st;
    wordexp("$HOME/.craftos", &p, 0);
    std::string oldpath = p.we_wordv[0];
    for (int i = 1; i < p.we_wordc; i++) oldpath += p.we_wordv[i];
    wordfree(&p);
    if (stat(oldpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && stat(getBasePath().c_str(), &st) != 0) 
        [[NSFileManager defaultManager] moveItemAtPath:[NSString stringWithCString:oldpath.c_str() encoding:NSASCIIStringEncoding] toPath:[NSString stringWithCString:getBasePath().c_str() encoding:NSASCIIStringEncoding] error:nil];
}

void copyImage(SDL_Surface* surf, SDL_Window* win) {
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

void platformExit() {}
