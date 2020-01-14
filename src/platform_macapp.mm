/*
 * platform_macapp.mm
 * CraftOS-PC 2
 * 
 * This file implements functions specific to macOS app binaries.
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
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/SSLException.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <SDL2/SDL.h>
#include <png++/png.hpp>
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include "platform.hpp"
#include "mounter.hpp"
#include "http.hpp"
#include "os.hpp"

extern bool exiting;
std::string rom_path_expanded;

std::string getBasePath() {
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
	return s;
}

void setThreadName(std::thread &t, std::string name) {}

int createDirectory(std::string path) {
    if (mkdir(path.c_str(), 0777) != 0) {
        if (errno == ENOENT && path != "/") {
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
	struct statvfs st;
	if (statvfs(path.c_str(), &st) != 0) return 0;
	return st.f_bavail * st.f_bsize;
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

void updateNow(std::string tag_name) {
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
            printf("Wrote: %s\n", tempFileTemplateCString);
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
                    if (![[NSFileManager defaultManager] isExecutableFileAtPath:[path path]]) {
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
                    if (pid < 0) printf("Could not fork: %d\n", pid);
                    else if (pid == 0) {
                        system(("/usr/bin/osascript -e 'do shell script \"/bin/sh " + std::string([path fileSystemRepresentation]) + " " + std::string([[NSBundle mainBundle].bundlePath fileSystemRepresentation]) + "\" with administrator privileges'").c_str()); exit(0);
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

std::unordered_map<std::string, void*> dylibs;

void * loadSymbol(std::string path, std::string symbol) {
    void * handle;
    if (dylibs.find(path) == dylibs.end()) dylibs[path] = dlopen(path.c_str(), RTLD_LAZY);
    handle = dylibs[path];
    return dlsym(handle, symbol.c_str());
}

void unloadLibraries() {
    for (auto lib : dylibs) dlclose(lib.second);
}

void copyImage(SDL_Surface* surf) {
    png::solid_pixel_buffer<png::rgb_pixel> pixbuf(surf->w, surf->h);
    memcpy((void*)&pixbuf.get_bytes()[0], surf->pixels, surf->h * surf->pitch);
    png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel> > img(surf->w, surf->h);
    img.set_pixbuf(pixbuf);
    std::stringstream ss;
    img.write_stream(ss);
    NSData * nsdata = [NSData dataWithBytes:ss.str().c_str() length:surf->w*surf->h*3];
    NSImage * nsimg = [[NSImage alloc] initWithData:nsdata];
    NSArray * arr = [NSArray arrayWithObject:nsimg];
    [[NSPasteboard generalPasteboard] clearContents];
    [[NSPasteboard generalPasteboard] writeObjects:arr];
}
