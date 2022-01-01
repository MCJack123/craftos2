/*
 * platform/ios.mm
 * CraftOS-PC 2
 * 
 * This file implements functions specific to iOS app binaries.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
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
#include "../termsupport.hpp"
#include "../terminal/SDLTerminal.hpp"

extern bool exiting;
std::string base_path_expanded;
std::string rom_path_expanded;
static SDL_SysWMinfo window_info;
static UIView * sdlView;

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

void updateNow(const std::string& tag_name, const Poco::JSON::Object::Ptr root) {
    
}

void migrateOldData() {
    
}

void copyImage(SDL_Surface* surf, SDL_Window* win) {
    /*png::solid_pixel_buffer<png::rgb_pixel> pixbuf(surf->w, surf->h);
    memcpy((void*)&pixbuf.get_bytes()[0], surf->pixels, surf->h * surf->pitch);
    png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel> > img(surf->w, surf->h);
    img.set_pixbuf(pixbuf);
    std::stringstream ss;
    img.write_stream(ss);
    NSData * nsdata = [NSData dataWithBytes:ss.str().c_str() length:surf->w*surf->h*3];
    [[UIPasteboard generalPasteboard] clearContents];
    [[UIPasteboard generalPasteboard] setData:nsdata forPasteboardType:kUTTypePNG];
    [nsdata release];*/
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

struct hold_data {
    int x;
    int y;
    int winid;
};

static Uint32 holdTimerCallback(Uint32 interval, void* param) {
    hold_data * hold = (hold_data*)param;
    SDL_Event e;
    e.type = SDL_KEYDOWN;
    e.key.timestamp = time(0);
    e.key.windowID = hold->winid;
    e.key.state = SDL_PRESSED;
    e.key.repeat = 1;
    e.key.keysym.mod = 0;
    if (hold->x != 0) {
        e.key.keysym.scancode = hold->x > 0 ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_LEFT;
        e.key.keysym.sym = hold->x > 0 ? SDLK_RIGHT : SDLK_LEFT;
        SDL_PushEvent(&e);
    }
    if (hold->y != 0) {
        e.key.keysym.scancode = hold->y > 0 ? SDL_SCANCODE_UP : SDL_SCANCODE_DOWN;
        e.key.keysym.sym = hold->y > 0 ? SDLK_UP : SDLK_DOWN;
        SDL_PushEvent(&e);
    }
    return interval;
}

@interface ViewController : UIViewController<UIGestureRecognizerDelegate> {
    CGFloat lastPinchScale;
    hold_data hold;
    SDL_TimerID holdTimer;
    BOOL isCtrlDown;
    BOOL isAltDown;
}
@property (strong, nonatomic) IBOutlet UIToolbar *hotkeyToolbar;
@property (strong, nonatomic) UIViewController * oldvc;
@property (strong, nonatomic) IBOutlet UIBarButtonItem *ctrlButton;
@property (strong, nonatomic) IBOutlet UIBarButtonItem *altButton;
@property (strong, nonatomic) IBOutlet UIBarButtonItem *closeButton;
@property (strong, nonatomic) IBOutlet UIBarButtonItem *nextButton;
@property (strong, nonatomic) IBOutlet UIBarButtonItem *previousButton;
@property (assign) SDL_Window * sdlWindow;
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    // Add insets to let the view go under the navigation bar
    self.additionalSafeAreaInsets = UIEdgeInsetsMake(-self.navigationController.navigationBar.frame.size.height, 0.0, 0.0, 0.0);
    // Add the old view controller as a child of the new one
    [self addChildViewController:self.oldvc];
    [self.view addSubview:self.oldvc.view];
    // Set up constraints inside the safe area
    self.oldvc.view.translatesAutoresizingMaskIntoConstraints = NO;
    [self.oldvc.view.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor].active = YES;
    [self.oldvc.view.bottomAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor].active = YES;
    [self.oldvc.view.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor].active = YES;
    [self.oldvc.view.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor].active = YES;
    [self.oldvc didMoveToParentViewController:self];
    // Add the hotkey toolbar to the invisible text field so it shows up on input grab
    UITextField * textField = (UITextField*)object_getIvar(self.oldvc, class_getInstanceVariable([self.oldvc class], "textField"));
    [self.view addSubview:textField]; // ?
    textField.inputAccessoryView = self.hotkeyToolbar;
    textField.keyboardAppearance = UIKeyboardAppearanceDark;
    // Fix transparent navigation bar background on iOS 15+
    if (@available(iOS 15, *)) {
        UINavigationBarAppearance *appearance = [[UINavigationBarAppearance alloc] init];
        [appearance configureWithDefaultBackground];
        appearance.backgroundColor = [UIColor colorWithRed:44.0/255.0 green:44.0/255.0 blue:46.0/255.0 alpha:1.0];
        appearance.titleTextAttributes = @{NSForegroundColorAttributeName: [UIColor whiteColor]};
        self.navigationController.navigationBar.standardAppearance = appearance;
        self.navigationController.navigationBar.scrollEdgeAppearance = appearance;
    }
    // Initialize properties
    isCtrlDown = NO;
    isAltDown = NO;
    holdTimer = 0;
    hold.winid = SDL_GetWindowID(self.sdlWindow);
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    SDL_StartTextInput();
}

- (IBAction)handleSwipe:(UISwipeGestureRecognizer*)rec {
    SDL_Event e;
    e.type = SDL_KEYDOWN;
    e.key.timestamp = time(0);
    e.key.windowID = SDL_GetWindowID(self.sdlWindow);
    e.key.state = SDL_PRESSED;
    e.key.repeat = 0;
    switch (rec.direction) {
    case UISwipeGestureRecognizerDirectionUp:
        e.key.keysym.scancode = SDL_SCANCODE_UP;
        e.key.keysym.sym = SDLK_UP;
        break;
    case UISwipeGestureRecognizerDirectionDown:
        e.key.keysym.scancode = SDL_SCANCODE_DOWN;
        e.key.keysym.sym = SDLK_DOWN;
        break;
    case UISwipeGestureRecognizerDirectionLeft:
        e.key.keysym.scancode = SDL_SCANCODE_LEFT;
        e.key.keysym.sym = SDLK_LEFT;
        break;
    case UISwipeGestureRecognizerDirectionRight:
        e.key.keysym.scancode = SDL_SCANCODE_RIGHT;
        e.key.keysym.sym = SDLK_RIGHT;
        break;
    }
    e.key.keysym.mod = 0;
    SDL_PushEvent(&e);
    e.type = SDL_KEYUP;
    e.key.state = SDL_RELEASED;
    SDL_PushEvent(&e);
}

- (IBAction)handleHold:(UILongPressGestureRecognizer*)rec {
    if (rec.state == UIGestureRecognizerStateBegan) {
        CGPoint touchPoint = [rec locationInView:self.oldvc.view];
        hold.x = 0; hold.y = 0;
        if (touchPoint.x < self.oldvc.view.bounds.size.width / 4.0) hold.x = -1;
        else if (touchPoint.x > self.oldvc.view.bounds.size.width / 4.0 * 3.0) hold.x = 1;
        if (touchPoint.y < self.oldvc.view.bounds.size.height / 4.0) hold.y = 1;
        else if (touchPoint.y > self.oldvc.view.bounds.size.height / 4.0 * 3.0) hold.y = -1;
        if (hold.x == 0 && hold.y == 0) return;
        if (holdTimer != 0) SDL_RemoveTimer(holdTimer);
        
        SDL_Event e;
        e.type = SDL_KEYDOWN;
        e.key.timestamp = time(0);
        e.key.windowID = SDL_GetWindowID(self.sdlWindow);
        e.key.state = SDL_PRESSED;
        e.key.repeat = 0;
        e.key.keysym.mod = 0;
        if (hold.x != 0) {
            e.key.keysym.scancode = hold.x > 0 ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_LEFT;
            e.key.keysym.sym = hold.x > 0 ? SDLK_RIGHT : SDLK_LEFT;
            SDL_PushEvent(&e);
        }
        if (hold.y != 0) {
            e.key.keysym.scancode = hold.y > 0 ? SDL_SCANCODE_UP : SDL_SCANCODE_DOWN;
            e.key.keysym.sym = hold.y > 0 ? SDLK_UP : SDLK_DOWN;
            SDL_PushEvent(&e);
        }
        holdTimer = SDL_AddTimer(100, holdTimerCallback, &hold);
    } else if (rec.state == UIGestureRecognizerStateEnded) {
        if (holdTimer != 0) SDL_RemoveTimer(holdTimer);
        SDL_Event e;
        e.type = SDL_KEYUP;
        e.key.timestamp = time(0);
        e.key.windowID = SDL_GetWindowID(self.sdlWindow);
        e.key.state = SDL_RELEASED;
        e.key.repeat = 0;
        e.key.keysym.mod = 0;
        if (hold.x != 0) {
            e.key.keysym.scancode = hold.x > 0 ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_LEFT;
            e.key.keysym.sym = hold.x > 0 ? SDLK_RIGHT : SDLK_LEFT;
            SDL_PushEvent(&e);
        }
        if (hold.y != 0) {
            e.key.keysym.scancode = hold.y > 0 ? SDL_SCANCODE_UP : SDL_SCANCODE_DOWN;
            e.key.keysym.sym = hold.y > 0 ? SDLK_UP : SDLK_DOWN;
            SDL_PushEvent(&e);
        }
    }
}

- (IBAction)toggleNavbar:(id)sender {
    [self.navigationController setNavigationBarHidden:![self.navigationController isNavigationBarHidden] animated:YES];
}

- (IBAction)toggleKeyboard:(id)sender {
    if (SDL_IsTextInputActive()) SDL_StopTextInput();
    else SDL_StartTextInput();
}

- (IBAction)onPrevious:(id)sender {
    previousRenderTarget();
    self.closeButton.enabled = computers->size() > 1 || (*renderTarget) != computers->front()->term;
}

- (IBAction)onNext:(id)sender {
    nextRenderTarget();
    self.closeButton.enabled = computers->size() > 1 || (*renderTarget) != computers->front()->term;
}

- (IBAction)onClose:(id)sender {
    if (renderTargets.size() < 2) return;
    SDL_Event e;
    e.type = SDL_WINDOWEVENT;
    e.window.timestamp = time(0);
    e.window.windowID = SDL_GetWindowID(self.sdlWindow);
    e.window.event = SDL_WINDOWEVENT_CLOSE;
    e.window.data1 = 0;
    e.window.data2 = 0;
    SDL_PushEvent(&e);
}

- (IBAction)onCtrl:(id)sender {
    SDL_Event e;
    e.key.timestamp = time(0);
    e.key.windowID = SDL_GetWindowID(self.sdlWindow);
    e.key.state = SDL_PRESSED;
    e.key.repeat = 0;
    e.key.keysym.scancode = SDL_SCANCODE_LCTRL;
    e.key.keysym.sym = SDLK_LCTRL;
    e.key.keysym.mod = KMOD_LCTRL;
    if (isCtrlDown) {
        e.type = SDL_KEYUP;
        self.ctrlButton.style = UIBarButtonItemStylePlain;
    } else {
        e.type = SDL_KEYDOWN;
        self.ctrlButton.style = UIBarButtonItemStyleDone;
    }
    isCtrlDown = !isCtrlDown;
    SDL_PushEvent(&e);
}

- (IBAction)onAlt:(id)sender {
    SDL_Event e;
    e.key.timestamp = time(0);
    e.key.windowID = SDL_GetWindowID(self.sdlWindow);
    e.key.state = SDL_PRESSED;
    e.key.repeat = 0;
    e.key.keysym.scancode = SDL_SCANCODE_LALT;
    e.key.keysym.sym = SDLK_LALT;
    e.key.keysym.mod = KMOD_LALT;
    if (isAltDown) {
        e.type = SDL_KEYUP;
        self.altButton.style = UIBarButtonItemStylePlain;
    } else {
        e.type = SDL_KEYDOWN;
        self.altButton.style = UIBarButtonItemStyleDone;
    }
    isAltDown = !isAltDown;
    SDL_PushEvent(&e);
}

- (IBAction)onTab:(id)sender {
    SDL_Event e;
    e.type = SDL_KEYDOWN;
    e.key.timestamp = time(0);
    e.key.windowID = SDL_GetWindowID(self.sdlWindow);
    e.key.state = SDL_PRESSED;
    e.key.repeat = 0;
    e.key.keysym.scancode = SDL_SCANCODE_TAB;
    e.key.keysym.sym = SDLK_TAB;
    e.key.keysym.mod = 0;
    SDL_PushEvent(&e);
    e.type = SDL_KEYUP;
    e.key.state = SDL_RELEASED;
    SDL_PushEvent(&e);
}

- (IBAction)onPaste:(id)sender {
    SDL_Event e;
    e.type = SDL_KEYDOWN;
    e.key.timestamp = time(0);
    e.key.windowID = SDL_GetWindowID(self.sdlWindow);
    e.key.state = SDL_PRESSED;
    e.key.repeat = 0;
    e.key.keysym.scancode = SDL_SCANCODE_V;
    e.key.keysym.sym = SDLK_v;
    e.key.keysym.mod = KMOD_GUI;
    SDL_PushEvent(&e);
    e.type = SDL_KEYUP;
    e.key.state = SDL_RELEASED;
    SDL_PushEvent(&e);
}

- (IBAction)onTerminate:(id)sender forEvent:(UIEvent*)event {
    if (event.allTouches == nil || event.allTouches.anyObject == nil || event.allTouches.anyObject.tapCount != 0) return;
    SDL_Event e;
    e.type = SDL_KEYDOWN;
    e.key.timestamp = time(0);
    e.key.windowID = SDL_GetWindowID(self.sdlWindow);
    e.key.state = SDL_PRESSED;
    e.key.repeat = 0;
    e.key.keysym.scancode = SDL_SCANCODE_T;
    e.key.keysym.sym = SDLK_t;
    e.key.keysym.mod = KMOD_CTRL;
    SDL_PushEvent(&e);
    e.key.repeat = 1;
    SDL_PushEvent(&e);
    e.type = SDL_KEYUP;
    e.key.state = SDL_RELEASED;
    SDL_PushEvent(&e);
}

- (IBAction)onShutdown:(id)sender forEvent:(UIEvent*)event {
    if (event.allTouches == nil || event.allTouches.anyObject == nil || event.allTouches.anyObject.tapCount != 0) return;
    SDL_Event e;
    e.type = SDL_KEYDOWN;
    e.key.timestamp = time(0);
    e.key.windowID = SDL_GetWindowID(self.sdlWindow);
    e.key.state = SDL_PRESSED;
    e.key.repeat = 0;
    e.key.keysym.scancode = SDL_SCANCODE_S;
    e.key.keysym.sym = SDLK_s;
    e.key.keysym.mod = KMOD_CTRL;
    SDL_PushEvent(&e);
    e.key.repeat = 1;
    SDL_PushEvent(&e);
    e.type = SDL_KEYUP;
    e.key.state = SDL_RELEASED;
    SDL_PushEvent(&e);
}

- (IBAction)onReboot:(id)sender forEvent:(UIEvent*)event {
    if (event.allTouches == nil || event.allTouches.anyObject == nil || event.allTouches.anyObject.tapCount != 0) return;
    SDL_Event e;
    e.type = SDL_KEYDOWN;
    e.key.timestamp = time(0);
    e.key.windowID = SDL_GetWindowID(self.sdlWindow);
    e.key.state = SDL_PRESSED;
    e.key.repeat = 0;
    e.key.keysym.scancode = SDL_SCANCODE_R;
    e.key.keysym.sym = SDLK_r;
    e.key.keysym.mod = KMOD_CTRL;
    SDL_PushEvent(&e);
    e.key.repeat = 1;
    SDL_PushEvent(&e);
    e.type = SDL_KEYUP;
    e.key.state = SDL_RELEASED;
    SDL_PushEvent(&e);
}

- (void)updateKeyboard {} // placeholder empty method

// MARK: UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer
shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer {
    return YES;
}

@end

@interface CCAppDelegate : NSObject<UIApplicationDelegate>
@property (retain, nonatomic) NSObject<UIApplicationDelegate> * delegate;
@end
@implementation CCAppDelegate
- (id)initWithDelegate:(NSObject<UIApplicationDelegate> *)del {
    self.delegate = del;
    return self;
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
    [self.delegate applicationDidBecomeActive:application];
    SDL_StartTextInput();
}

- (id)forwardingTargetForSelector:(SEL)sel {
    if ([self.delegate respondsToSelector:sel]) return self.delegate;
    return nil;
}
@end

static ViewController * viewController = NULL;
static CCAppDelegate * appDelegate = NULL;

void iosSetSafeAreaConstraints(SDLTerminal * term) {
    @autoreleasepool {
        if (viewController != NULL) return;
        // First, grab the view controller + view
        SDL_VERSION(&window_info.version);
        SDL_GetWindowWMInfo(term->win, &window_info);
        // Instantiate the new view controller from the storyboard (that's linked, right?), and add the properties in
        UIViewController * oldvc = window_info.info.uikit.window.rootViewController;
        UINavigationController * rootvc = (UINavigationController*)[[UIStoryboard storyboardWithName:@"Main" bundle:[NSBundle mainBundle]] instantiateInitialViewController];
        ViewController * vc = (ViewController*)[rootvc.viewControllers firstObject];
        vc.oldvc = oldvc;
        vc.sdlWindow = term->win;
        // Start setting up the new view controller in the view hierarchy (we'll finish this once the VC is loaded)
        [oldvc.view removeFromSuperview];
        window_info.info.uikit.window.rootViewController = rootvc;
        sdlView = oldvc.view;
        viewController = vc;
        // Remove the updateKeyboard method in the old view controller, as it glitches out the screen when opening/closing the keyboard
        method_setImplementation(class_getInstanceMethod([oldvc class], @selector(updateKeyboard)), [ViewController instanceMethodForSelector:@selector(updateKeyboard)]);
        // Set fullscreen mode, but only on devices without a notch
        if (window_info.info.uikit.window.safeAreaInsets.top <= 30) SDL_SetWindowFullscreen(term->win, SDL_WINDOW_FULLSCREEN_DESKTOP);
        viewController.navigationItem.title = [NSString stringWithCString:term->title.c_str() encoding:NSASCIIStringEncoding];
        // Add an application delegate override so we can hook some things
        appDelegate = [[CCAppDelegate alloc] initWithDelegate:[UIApplication sharedApplication].delegate];
        [UIApplication sharedApplication].delegate = appDelegate;
        //std::thread([](){while (true) queueTask([](void*)->void*{[viewController toggleNavbar:nil]; return NULL;}, NULL);}).detach();
    }
}

void updateCloseButton() {
    if (computers->empty()) return;
    viewController.closeButton.enabled = computers->size() > 1 || (*renderTarget) != computers->front()->term;
    viewController.nextButton.enabled = renderTargets.size() > 1;
    viewController.previousButton.enabled = renderTargets.size() > 1;
}

void iOS_SetWindowTitle(SDL_Window * win, const char * title) {
    viewController.navigationItem.title = [NSString stringWithCString:title encoding:NSASCIIStringEncoding];
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
    int size = ((int)(ptrdiff_t)ud - 4*sdlterm->charScale*sdlterm->dpiScale) / (sdlterm->charHeight*sdlterm->dpiScale);
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
        CGRect termSize = sdlView.frame;
        if (!computers->empty()) queueEvent(computers->front(), mobile_keyboard_open, (void*)(ptrdiff_t)(screenSize.size.height - keyboardBound.size.height - termSize.origin.y));
    }];
    [[NSNotificationCenter defaultCenter] addObserverForName:UIKeyboardDidHideNotification object:nil queue:nil usingBlock:^(NSNotification* notif) {
        if (!computers->empty()) queueEvent(computers->front(), mobile_keyboard_open, (void*)PTRDIFF_MAX);
    }];
}

void platformExit() {}

#ifdef __INTELLISENSE__
#region Mobile API
#endif

static int mobile_openKeyboard(lua_State *L) {
    if (lua_isnone(L, 1) || lua_toboolean(L, 1)) queueTask([](void*)->void*{SDL_StartTextInput(); return NULL;}, NULL, true);
    else queueTask([](void*)->void*{SDL_StopTextInput(); return NULL;}, NULL, true);
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

/*static luaL_Reg ios_reg[] = {
    {NULL, NULL}
};*/

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
