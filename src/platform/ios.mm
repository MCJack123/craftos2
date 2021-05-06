/*
 * platform/ios.mm
 * CraftOS-PC 2
 * 
 * This file implements functions specific to iOS app binaries.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

extern "C" {
#include "lua.h"
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
//#include <png++/png.hpp>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#include "../platform.hpp"
#include "../runtime.hpp"
#include "../terminal/SDLTerminal.hpp"

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
    if (!base_path_expanded.empty()) return base_path_expanded;
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

void setThreadName(std::thread &t, const std::string& name) {}

int createDirectory(const path_t& path) {
    if (mkdir(path.c_str(), 0777) != 0) {
        if (errno == ENOENT && path != "/" && !path.empty()) {
            if (createDirectory(path.substr(0, path.find_last_of('/')).c_str())) return 1;
            mkdir(path.c_str(), 0777);
        } else if (errno != EEXIST) return 1;
    }
    return 0;
}

int removeDirectory(const path_t& path) {
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

unsigned long long getFreeSpace(const path_t& path) {
    NSDictionary * dict = [[NSFileManager defaultManager] attributesOfFileSystemForPath:[NSString stringWithCString:path.c_str() encoding:NSASCIIStringEncoding] error:nil];
    if (dict == nil) {
        if (path.find_last_of("/") == std::string::npos || path.substr(0, path.find_last_of("/")-1).empty()) return 0;
        else return getFreeSpace(path.substr(0, path.find_last_of("/")-1));
    }
    return [(NSNumber*)dict[NSFileSystemFreeSize] unsignedLongLongValue];
}

unsigned long long getCapacity(const path_t& path) {
    NSDictionary * dict = [[NSFileManager defaultManager] attributesOfFileSystemForPath:[NSString stringWithCString:path.c_str() encoding:NSASCIIStringEncoding] error:nil];
    if (dict == nil) {
        if (path.find_last_of("/") == std::string::npos || path.substr(0, path.find_last_of("/")-1).empty()) return 0;
        else return getCapacity(path.substr(0, path.find_last_of("/")-1));
    }
    return [(NSNumber*)dict[NSFileSystemSize] unsignedLongLongValue];
}

void updateNow(const std::string& tag_name) {
    
}

void migrateOldData() {
    
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

static std::string mobile_keyboard_open(lua_State *L, void* ud) {
    SDLTerminal * sdlterm = (SDLTerminal*)get_comp(L)->term;
    int size = ((int)(ptrdiff_t)ud - 4*(2/SDLTerminal::fontScale)*sdlterm->charScale*sdlterm->dpiScale) / (sdlterm->charHeight*sdlterm->dpiScale);
    if (size >= sdlterm->height) return "_CCPC_mobile_keyboard_close";
    lua_pushinteger(L, size);
    return "_CCPC_mobile_keyboard_open";
}

void setupCrashHandler() {
    signal(SIGSEGV, handler);
    signal(SIGILL, handler);
    signal(SIGBUS, handler);
    signal(SIGTRAP, handler);
    [[NSNotificationCenter defaultCenter] addObserverForName:UIKeyboardDidShowNotification object:nil queue:nil usingBlock:^(NSNotification* notif) {
        NSValue* obj = (NSValue*)[notif.userInfo valueForKey:UIKeyboardFrameEndUserInfoKey];
        CGRect keyboardBound = CGRectNull;
        [obj getValue:&keyboardBound];
        CGRect screenSize = [[UIApplication sharedApplication] keyWindow].rootViewController.view.bounds;
        if (!computers->empty()) queueEvent(computers->front(), mobile_keyboard_open, (void*)(ptrdiff_t)(screenSize.size.height - keyboardBound.size.height));
    }];
    [[NSNotificationCenter defaultCenter] addObserverForName:UIKeyboardDidHideNotification object:nil queue:nil usingBlock:^(NSNotification* notif) {
        if (!computers->empty()) queueEvent(computers->front(), mobile_keyboard_open, (void*)PTRDIFF_MAX);
    }];
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

void setFloating(SDL_Window* win, bool state) {}

/*
 * The following code is a long-winded workaround to make SDL views work nicely on iPhone X/iPad Pro.
 * Normally, the view SDL creates is set as the root view, which means it is the size of the screen.
 * However, we need to constrain it using the safe area insets to make sure we don't draw outside the
 * screen area (in the corners or notch). To do this, we need to adjust the view controller so that
 * the view SDL uses is inside another view, and that the view is automatically resized to fit the
 * safe area. Unfortunately, a bit of the logic in SDL's view controllers assumes that a) the view is
 * always the size of the screen, and b) that the view is the root view. Since all of the initialization
 * code and classes are private to SDL, we can't just extend the view controller and use an initializer
 * that uses that class. Instead, we have to swizzle (override - a new word I learned for this!) the
 * affected methods to use our overridden ones that can handle the new view hierarchy. Luckily the
 * Objective-C runtime is very flexible, so we can just set up a template class that has the properties
 * we need from SDL's view controller, and then swap in the methods from that into the view controller
 * object/class.
 */

// From SDL/src/video/SDL_sysvideo.h
extern "C" {
    struct SDL_WindowShaper;
    struct SDL_WindowUserData;
    /* Define the SDL window structure, corresponding to toplevel windows */
    struct SDL_Window
    {
        const void *magic;
        Uint32 id;
        char *title;
        SDL_Surface *icon;
        int x, y;
        int w, h;
        int min_w, min_h;
        int max_w, max_h;
        Uint32 flags;
        Uint32 last_fullscreen_flags;

        /* Stored position and size for windowed mode */
        SDL_Rect windowed;

        SDL_DisplayMode fullscreen_mode;

        float opacity;

        float brightness;
        Uint16 *gamma;
        Uint16 *saved_gamma;        /* (just offset into gamma) */

        SDL_Surface *surface;
        SDL_bool surface_valid;

        SDL_bool is_hiding;
        SDL_bool is_destroying;
        SDL_bool is_dropping;       /* drag/drop in progress, expecting SDL_SendDropComplete(). */

        SDL_WindowShaper *shaper;

        SDL_HitTest hit_test;
        void *hit_test_data;

        SDL_WindowUserData *data;

        void *driverdata;

        SDL_Window *prev;
        SDL_Window *next;
    };
}

static int RemovePendingSizeChangedAndResizedEvents(void * userdata, SDL_Event *event) {
    SDL_Event *new_event = (SDL_Event *)userdata;

    if (event->type == SDL_WINDOWEVENT &&
        (event->window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
         event->window.event == SDL_WINDOWEVENT_RESIZED) &&
        event->window.windowID == new_event->window.windowID) {
        /* We're about to post a new size event, drop the old one */
        return 0;
    }
    return 1;
}

static bool forceInitView = true;

// Small view to hold a reference to the inner view held by SDL
@interface SDLRootView : UIView
@property UIView * sdlView;
@end
@implementation SDLRootView
- (id) initWithSDLView:(UIView*)view {
    self = [super init];
    self.sdlView = view;
    self.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    return self;
}
@end

// This class holds the overridden methods we'll be inserting into SDL's view controller
// This should never be instantiated as it is not complete
@interface VCOverride : UIViewController
@property (nonatomic, assign) SDL_Window *window;
@property (nonatomic, assign, getter=isKeyboardVisible) BOOL keyboardVisible;
- (void)viewDidLayoutSubviews;
- (void)setView:(UIView*)view;
- (void)showKeyboard;
@end
@implementation VCOverride
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    const CGSize size = ((SDLRootView*)self.view).sdlView.bounds.size;
    int data1 = (int) size.width;
    int data2 = (int) size.height;
    SDL_Window *window = self.window;

    // SDL_SendWindowEvent
    if (!(window->flags & SDL_WINDOW_FULLSCREEN)) {
        window->windowed.w = data1;
        window->windowed.h = data2;
    }
    if (data1 == window->w && data2 == window->h) {
        return;
    }
    window->w = data1;
    window->h = data2;
    
    window->surface_valid = SDL_FALSE;
    if (SDL_GetEventState(SDL_WINDOWEVENT) == SDL_ENABLE) {
        SDL_Event event;
        event.type = SDL_WINDOWEVENT;
        event.window.event = SDL_WINDOWEVENT_SIZE_CHANGED;
        event.window.data1 = data1;
        event.window.data2 = data2;
        event.window.windowID = window->id;

        /* Fixes queue overflow with resize events that aren't processed */
        SDL_FilterEvents(RemovePendingSizeChangedAndResizedEvents, &event);
        SDL_PushEvent(&event);
        
        event.window.event = SDL_WINDOWEVENT_RESIZED;

        SDL_PushEvent(&event);
    }
}
- (void)setView:(UIView*)view {
    if (self.view == nil || forceInitView) {
        forceInitView = false;
        // Create the new parent view
        SDLRootView* rview = [[SDLRootView alloc] initWithSDLView:view];
        // Since super doesn't work, we directly invoke the setView method to set the view
        ((void(*)(id, SEL, UIView*))class_getMethodImplementation([self superclass], @selector(setView:)))(self, @selector(setView:), rview);
        // Add the view to the parent
        [rview addSubview:view];
        // Set constraints to size to safe area
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [view.topAnchor constraintEqualToAnchor:rview.safeAreaLayoutGuide.topAnchor].active = YES;
        [view.bottomAnchor constraintEqualToAnchor:rview.safeAreaLayoutGuide.bottomAnchor].active = YES;
        [view.leadingAnchor constraintEqualToAnchor:rview.safeAreaLayoutGuide.leadingAnchor].active = YES;
        [view.trailingAnchor constraintEqualToAnchor:rview.safeAreaLayoutGuide.trailingAnchor].active = YES;
    } else {
        // If the parent already exists, remove the old view and add the new view
        SDLRootView * v = (SDLRootView*)self.view;
        [v.sdlView removeFromSuperview];
        [v addSubview:view];
        v.sdlView = view;
    }

    // We don't have access to textField from here, so we use the ObjC runtime to grab it for us
    [view addSubview:object_getIvar(self, class_getInstanceVariable([self class], "textField"))];

    if (self.keyboardVisible) {
        [self showKeyboard];
    }
}
- (void)showKeyboard {} // to suppress warnings
@end

void iosSetSafeAreaConstraints(SDLTerminal * term) {
    // First, grab the view controller + view
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(term->win, &info);
    UIView * view = info.info.uikit.window.rootViewController.view;
    // Set the viewDidLayoutSubview and setView methods to our overrides
    method_setImplementation(class_getInstanceMethod([info.info.uikit.window.rootViewController class], @selector(viewDidLayoutSubviews)), [VCOverride instanceMethodForSelector:@selector(viewDidLayoutSubviews)]);
    method_setImplementation(class_getInstanceMethod([info.info.uikit.window.rootViewController class], @selector(setView:)), [VCOverride instanceMethodForSelector:@selector(setView:)]);
    // Add the new superview through the overridden setView method
    [info.info.uikit.window.rootViewController setView:view];
    info.info.uikit.window.rootViewController.view.backgroundColor = [[UIColor alloc] initWithRed:term->palette[15].r / 255.0 green:term->palette[15].g / 255.0 blue:term->palette[15].b / 255.0 alpha:1.0];
    // Force a layout update to reload the renderer and set the proper dimensions
    [view layoutSubviews];
}

#ifdef __INTELLISENSE__
#region Mobile API
#endif

static int mobile_openKeyboard(lua_State *L) {
    if (lua_isnone(L, 1) || lua_toboolean(L, 1)) SDL_StartTextInput();
    else SDL_StopTextInput();
    return 0;
}

static int mobile_isKeyboardOpen(lua_State *L) {
    lua_pushboolean(L, SDL_IsTextInputActive());
    return 1;
}

static luaL_Reg mobile_reg[] = {
    {"openKeyboard", mobile_openKeyboard},
    {"isKeyboardOpen", mobile_isKeyboardOpen},
    {NULL, NULL}
};

static luaL_Reg ios_reg[] = {
    {NULL, NULL}
};

int mobile_luaopen(lua_State *L) {
    luaL_register(L, "mobile", mobile_reg);
    /*lua_pushstring(L, "ios");
    lua_newtable(L);
    for (luaL_Reg* r = ios_reg; r->name && r->func; r++) {
        lua_pushstring(L, r->name);
        lua_pushcfunction(L, r->func);
        lua_settable(L, -3);
    }
    lua_settable(L, -3);*/
    return 1;
}

#ifdef __INTELLISENSE__
#endregion
#endif
