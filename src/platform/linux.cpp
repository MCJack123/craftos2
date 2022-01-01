/*
 * platform/linux.cpp
 * CraftOS-PC 2
 * 
 * This file implements functions specific to Linux.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#ifdef __linux__ // disable error checking on Windows
extern "C" {
#include <lua.h>
}
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <dirent.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <libgen.h>
#include <pthread.h>
#include <SDL2/SDL_syswm.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <ucontext.h>
#include <unistd.h>
#include <wordexp.h>
#ifndef NO_PNG
#include <png++/png.hpp>
#endif
#ifndef NO_WEBP
#include <webp/mux.h>
#include <webp/encode.h>
#endif
#include "../platform.hpp"
#include "../util.hpp"

#ifdef CUSTOM_ROM_DIR
const char * rom_path = CUSTOM_ROM_DIR;
std::string rom_path_expanded;
#else
const char * rom_path = "/usr/share/craftos";
#endif
#ifdef FS_ROOT
const char * base_path = "";
#else
const char * base_path = "$XDG_DATA_HOME/craftos-pc";
#endif
std::string base_path_expanded;

void setBasePath(const char * path) {
    base_path = path;
    base_path_expanded = path;
}

void setROMPath(const char * path) {
    rom_path = path;
#ifdef CUSTOM_ROM_DIR
    rom_path_expanded = path;
#endif
}

std::string getBasePath() {
    if (!base_path_expanded.empty()) return base_path_expanded;
    wordexp_t p;
    wordexp(base_path, &p, 0);
    base_path_expanded = p.we_wordv[0];
    for (unsigned i = 1; i < p.we_wordc; i++) base_path_expanded += p.we_wordv[i];
    wordfree(&p);
    if (base_path_expanded == "/craftos-pc") {
        wordexp("$HOME/.local/share/craftos-pc", &p, 0);
        base_path_expanded = p.we_wordv[0];
        for (unsigned i = 1; i < p.we_wordc; i++) base_path_expanded += p.we_wordv[i];
        wordfree(&p);
    }
    return base_path_expanded;
}

#ifdef CUSTOM_ROM_DIR
std::string getROMPath() {
    if (!rom_path_expanded.empty()) return rom_path_expanded;
    wordexp_t p;
    wordexp(rom_path, &p, 0);
    rom_path_expanded = p.we_wordv[0];
    for (unsigned i = 1; i < p.we_wordc; i++) rom_path_expanded += p.we_wordv[i];
    wordfree(&p);
    return rom_path_expanded;
}

std::string getPlugInPath() { return getROMPath() + "/plugins/"; }
#else
std::string getROMPath() { return rom_path; }
std::string getPlugInPath() { return std::string(rom_path) + "/plugins/"; }
#endif

std::string getMCSavePath() {
    wordexp_t p;
    wordexp("$HOME/.minecraft/saves/", &p, 0);
    std::string expanded = p.we_wordv[0];
    for (unsigned i = 1; i < p.we_wordc; i++) expanded += p.we_wordv[i];
    wordfree(&p);
    return expanded;
}

void setThreadName(std::thread &t, const std::string& name) {
    pthread_setname_np(t.native_handle(), name.c_str());
}

int createDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0 ) return !S_ISDIR(st.st_mode);
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
    struct statvfs st;
    if (statvfs(path.c_str(), &st) != 0) {
        if (path.find_last_of("/") == std::string::npos || path.substr(0, path.find_last_of("/")-1).empty()) return 0;
        else return getFreeSpace(path.substr(0, path.find_last_of("/")-1));
    }
    return st.f_bavail * st.f_bsize;
}

unsigned long long getCapacity(const std::string& path) {
    struct statvfs st;
    if (statvfs(path.c_str(), &st) != 0) {
        if (path.find_last_of("/") == std::string::npos || path.substr(0, path.find_last_of("/")-1).empty()) return 0;
        else return getCapacity(path.substr(0, path.find_last_of("/")-1));
    }
    return st.f_blocks * st.f_frsize;
}

void updateNow(const std::string& tag_name, const Poco::JSON::Object::Ptr root) {
    
}

static int recursiveMove(const std::string& fromDir, const std::string& toDir) {
    struct stat statbuf;
    if (!stat(fromDir.c_str(), &statbuf)) {
        if (S_ISDIR(statbuf.st_mode)) {
            createDirectory(toDir);
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
    for (unsigned i = 1; i < p.we_wordc; i++) oldpath += p.we_wordv[i];
    wordfree(&p);
    if (stat(oldpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && stat(getBasePath().c_str(), &st) != 0) 
        recursiveMove(oldpath, getBasePath());
}

static ProtectedObject<SDL_Surface*> copiedSurface((SDL_Surface*)NULL);

#ifdef _X11_XLIB_H_
static Atom ATOM_CLIPBOARD, ATOM_TARGETS, ATOM_MULTIPLE, ATOM_WEBP, ATOM_PNG, ATOM_BMP;
static bool didInitAtoms = false;
static void* x11_handle = NULL;
static int (*_XChangeProperty)(Display*, Window, Atom, Atom, int, int, _Xconst unsigned char*, int);
static Status (*_XSendEvent)(Display*, Window, Bool, long, XEvent*);
static Atom (*_XInternAtom)(Display*, _Xconst char*, Bool);
static int (*_XSetSelectionOwner)(Display*, Atom, Window, Time);

static int eventFilter(void* userdata, SDL_Event* e) {
    if (e->type == SDL_SYSWMEVENT) {
        XSelectionRequestEvent xe = e->syswm.msg->msg.x11.event.xselectionrequest;
        if (xe.type == SelectionRequest && xe.selection == ATOM_CLIPBOARD && *copiedSurface != NULL) {
            LockGuard lock(copiedSurface);
            SDL_Surface* temp = *copiedSurface;
            XSelectionEvent sev;
            sev.type = SelectionNotify; sev.display = xe.display; sev.requestor = xe.requestor;
            sev.selection = xe.selection; sev.time = xe.time; sev.target = xe.target; sev.property = xe.property;
            if (xe.target == ATOM_TARGETS) {
                std::vector<Atom> targets;
                targets.reserve(6);
                targets.push_back(ATOM_TARGETS);
                targets.push_back(ATOM_MULTIPLE);
#ifndef NO_WEBP
                targets.push_back(ATOM_WEBP);
#endif
#ifndef NO_PNG
                targets.push_back(ATOM_PNG);
#endif
                targets.push_back(ATOM_BMP);
                //targets.push_back(XA_PIXMAP);
                _XChangeProperty(xe.display, xe.requestor, xe.property, XA_ATOM, 32, PropModeReplace, (unsigned char*)&targets[0], targets.size());
#ifndef NO_WEBP
            } else if (xe.target == ATOM_WEBP) {
                uint8_t * data = NULL;
                size_t size = WebPEncodeLosslessRGB((uint8_t*)temp->pixels, temp->w, temp->h, temp->pitch, &data);
                if (size) {
                    _XChangeProperty(xe.display, xe.requestor, xe.property, ATOM_WEBP, 8, PropModeReplace, data, size);
                    WebPFree(data);
                } else sev.property = None;
#endif
#ifndef NO_PNG
            } else if (xe.target == ATOM_PNG) {
                png::solid_pixel_buffer<png::rgb_pixel> pixbuf(temp->w, temp->h);
                for (int i = 0; i < temp->h; i++)
                    memcpy((void*)&pixbuf.get_bytes()[i * temp->w * 3], (char*)temp->pixels + (i * temp->pitch), temp->w * 3);
                png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel> > img(temp->w, temp->h);
                img.set_pixbuf(pixbuf);
                std::stringstream out;
                img.write_stream(out);
                std::string data = out.str();
                _XChangeProperty(xe.display, xe.requestor, xe.property, ATOM_PNG, 8, PropModeReplace, (const unsigned char*)data.c_str(), data.size());
#endif
            } else if (xe.target == ATOM_BMP) {
                size_t size = temp->w * temp->h * 4 + 4096;
                unsigned char * data = new unsigned char[size]; // this should be enough for an image, right?
                SDL_RWops* rw = SDL_RWFromMem(data, size);
                SDL_SaveBMP_RW(temp, rw, false);
                size = SDL_RWtell(rw);
                SDL_RWclose(rw);
                _XChangeProperty(xe.display, xe.requestor, xe.property, ATOM_BMP, 8, PropModeReplace, data, size);
                delete[] data;
            } /*else if (xe.target == XA_PIXMAP) {
                XWindowAttributes attr;
                XGetWindowAttributes(xe.display, xe.requestor, &attr);
                SDL_PixelFormat format;
                format.palette = NULL; format.BitsPerPixel = (attr.visual->bits_per_rgb * 3 / 8) * 8; format.BytesPerPixel = attr.visual->bits_per_rgb * 3 / 8;
                format.Rmask = attr.visual->red_mask; format.Gmask = attr.visual->green_mask; format.Bmask = attr.visual->blue_mask; format.Amask = 0;
                SDL_Surface * surf = SDL_ConvertSurface(temp, &format, 0);
                XImage* img = XCreateImage(xe.display, attr.visual, surf->format->BitsPerPixel, ZPixmap, 0, (char*)surf->pixels, surf->w, surf->h, 8, surf->pitch);
                Pixmap pm = XCreatePixmap(xe.display, xe.requestor, surf->w, surf->h, attr.depth);
                GC gc = XCreateGC(xe.display, pm, 0, NULL);
                XPutImage(xe.display, pm, gc, img, 0, 0, 0, 0, surf->w, surf->h);
                XFreeGC(xe.display, gc);
                //XDestroyImage(img);
                SDL_FreeSurface(surf);
                XChangeProperty(xe.display, xe.requestor, xe.property, XA_PIXMAP, 32, PropModeReplace, (unsigned char*)&pm, 1);
                // free pixmap?
            }*/ else {
                sev.property = None;
            }
            _XSendEvent(xe.display, xe.requestor, true, 0, (XEvent*)&sev);
        } else return 0;
    }
    return 1;
}
#endif

#ifdef SDL_VIDEO_DRIVER_WAYLAND
struct wl_interface {
	const char *name;
	int version;
	int method_count;
	const void *methods;
	int event_count;
	const void *events;
};
static void* wayland_client_handle = NULL;
static const struct wl_interface * _wl_data_device_manager_interface;
static const struct wl_interface * _wl_seat_interface;
static const struct wl_interface * _wl_registry_interface;
static const struct wl_interface * _wl_data_device_interface;
static const struct wl_interface * _wl_data_source_interface;
static struct wl_proxy * (*_wl_proxy_marshal_constructor)(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, ...);
static struct wl_proxy * (*_wl_proxy_marshal_constructor_versioned)(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version, ...);
static void (*_wl_proxy_marshal)(struct wl_proxy *p, uint32_t opcode, ...);
static void (*_wl_proxy_destroy)(struct wl_proxy *proxy);
static int (*_wl_proxy_add_listener)(struct wl_proxy *proxy, void (**implementation)(void), void *data);
static int (*_wl_display_roundtrip)(struct wl_display *display);
struct wl_registry_listener {
	void (*global)(void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version);
	void (*global_remove)(void *data, struct wl_registry *wl_registry, uint32_t name);
};
struct wl_data_source_listener {
	void (*target)(void *data, struct wl_data_source *wl_data_source, const char *mime_type);
	void (*send)(void *data, struct wl_data_source *wl_data_source, const char *mime_type, int32_t fd);
	void (*cancelled)(void *data, struct wl_data_source *wl_data_source);
	void (*dnd_drop_performed)(void *data, struct wl_data_source *wl_data_source);
	void (*dnd_finished)(void *data, struct wl_data_source *wl_data_source);
	void (*action)(void *data, struct wl_data_source *wl_data_source, uint32_t dnd_action);
};

static struct wl_data_device_manager *data_device_manager = NULL;
static struct wl_seat *seat = NULL;
static bool addedListener = false;

static void registry_handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
	printf("interface: '%s', version: %d, name: %d\n",
			interface, version, name);
    if (strcmp(interface, _wl_data_device_manager_interface->name) == 0) {
		data_device_manager = (struct wl_data_device_manager*)_wl_proxy_marshal_constructor_versioned((struct wl_proxy *)registry, 0, _wl_data_device_manager_interface, 3, name, interface, 3, NULL);
    } else if (strcmp(interface, _wl_seat_interface->name) == 0 && seat == NULL) {
        // We only bind to the first seat. Multi-seat support is an exercise
        // left to the reader.
        seat = (struct wl_seat*)_wl_proxy_marshal_constructor_versioned((struct wl_proxy *)registry, 0, _wl_seat_interface, 1, name, interface, 1, NULL);
    }
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
	// This space deliberately left blank
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove,
};

static void data_source_handle_send(void *data, struct wl_data_source *source, const char *mime_type, int fd) {
    LockGuard lock(copiedSurface);
    SDL_Surface* temp = *copiedSurface;
    if (temp == NULL) {
        close(fd);
        return;
    }
	// An application wants to paste the clipboard contents
    if (strcmp(mime_type, "image/bmp") == 0) {
        size_t size = temp->w * temp->h * 4 + 4096;
        unsigned char * data = new unsigned char[size]; // this should be enough for an image, right?
        SDL_RWops* rw = SDL_RWFromMem(data, size);
        SDL_SaveBMP_RW(temp, rw, false);
        size = SDL_RWtell(rw);
        SDL_RWclose(rw);
        write(fd, data, size);
        delete[] data;
#ifndef NO_WEBP
    } else if (strcmp(mime_type, "image/webp") == 0) {
		uint8_t * data = NULL;
        size_t size = WebPEncodeLosslessRGB((uint8_t*)temp->pixels, temp->w, temp->h, temp->pitch, &data);
        if (size) {
            write(fd, data, size);
            WebPFree(data);
        }
#endif
#ifndef NO_PNG
	} else if (strcmp(mime_type, "image/png") == 0) {
		png::solid_pixel_buffer<png::rgb_pixel> pixbuf(temp->w, temp->h);
        for (int i = 0; i < temp->h; i++)
            memcpy((void*)&pixbuf.get_bytes()[i * temp->w * 3], (char*)temp->pixels + (i * temp->pitch), temp->w * 3);
        png::image<png::rgb_pixel, png::solid_pixel_buffer<png::rgb_pixel> > img(temp->w, temp->h);
        img.set_pixbuf(pixbuf);
        std::stringstream out;
        img.write_stream(out);
        std::string data = out.str();
        write(fd, data.c_str(), data.size());
#endif
	} else {
		fprintf(stderr,
			"Destination client requested unsupported MIME type: %s\n",
			mime_type);
	}
	close(fd);
}

static void data_source_handle_cancelled(void *data, struct wl_data_source *source) {
	// An application has replaced the clipboard contents
    _wl_proxy_marshal((struct wl_proxy *)source, 1);
    _wl_proxy_destroy((struct wl_proxy *)source);
}

static const struct wl_data_source_listener data_source_listener = {
    NULL,
	data_source_handle_send,
	data_source_handle_cancelled,
};
#endif

void copyImage(SDL_Surface* surf, SDL_Window* win) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(win, &info);
    if (info.subsystem == SDL_SYSWM_X11) {
#ifdef _X11_XLIB_H_
        if (x11_handle == NULL) {
#ifdef SDL_VIDEO_DRIVER_X11_DYNAMIC
            x11_handle = SDL_LoadObject(SDL_VIDEO_DRIVER_X11_DYNAMIC);
#else
            x11_handle = SDL_LoadObject("libX11.so");
#endif
            if (x11_handle == NULL) {
                fprintf(stderr, "Could not load X11 library: %s. Copying is not available.\n", SDL_GetError());
                return;
            }
            _XChangeProperty = (int (*)(Display*, Window, Atom, Atom, int, int, _Xconst unsigned char*, int))SDL_LoadFunction(x11_handle, "XChangeProperty");
            _XSendEvent = (Status (*)(Display*, Window, Bool, long, XEvent*))SDL_LoadFunction(x11_handle, "XSendEvent");
            _XInternAtom = (Atom (*)(Display*, _Xconst char*, Bool))SDL_LoadFunction(x11_handle, "XInternAtom");
            _XSetSelectionOwner = (int (*)(Display*, Atom, Window, Time))SDL_LoadFunction(x11_handle, "XSetSelectionOwner");
        }
        LockGuard lock(copiedSurface);
        Display* d = info.info.x11.display;
        if (!didInitAtoms) {
            ATOM_CLIPBOARD = _XInternAtom(d, "CLIPBOARD", false);
            ATOM_TARGETS = _XInternAtom(d, "TARGETS", false);
            ATOM_MULTIPLE = _XInternAtom(d, "MULTIPLE", false);
            ATOM_WEBP = _XInternAtom(d, "image/webp", false);
            ATOM_PNG = _XInternAtom(d, "image/png", false);
            ATOM_BMP = _XInternAtom(d, "image/bmp", false);
            didInitAtoms = true;
        }
        SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
        SDL_SetEventFilter(eventFilter, NULL);
        if (*copiedSurface != NULL) SDL_FreeSurface(*copiedSurface);
        copiedSurface = SDL_CreateRGBSurfaceWithFormat(surf->flags, surf->w, surf->h, surf->format->BitsPerPixel, surf->format->format);
        SDL_BlitSurface(surf, NULL, *copiedSurface, NULL);
        _XSetSelectionOwner(d, ATOM_CLIPBOARD, info.info.x11.window, CurrentTime);
#endif
    } else if (info.subsystem == SDL_SYSWM_WAYLAND) {
#ifdef SDL_VIDEO_DRIVER_WAYLAND
        if (wayland_client_handle == NULL) {
#ifdef SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC
            wayland_client_handle = SDL_LoadObject(SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC);
#else
            wayland_client_handle = SDL_LoadObject("libwayland-client.so");
#endif
            if (wayland_client_handle == NULL) {
                fprintf(stderr, "Could not load Wayland client library: %s. Copying is not available.\n", SDL_GetError());
                return;
            }
            _wl_data_device_manager_interface = (struct wl_interface *)SDL_LoadFunction(wayland_client_handle, "wl_data_device_manager_interface");
            _wl_seat_interface = (struct wl_interface *)SDL_LoadFunction(wayland_client_handle, "wl_seat_interface");
            _wl_registry_interface = (struct wl_interface *)SDL_LoadFunction(wayland_client_handle, "wl_registry_interface");
            _wl_data_device_interface = (struct wl_interface *)SDL_LoadFunction(wayland_client_handle, "wl_data_device_interface");
            _wl_data_source_interface = (struct wl_interface *)SDL_LoadFunction(wayland_client_handle, "wl_data_source_interface");
            _wl_proxy_marshal_constructor = (struct wl_proxy * (*)(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, ...))SDL_LoadFunction(wayland_client_handle, "wl_proxy_marshal_constructor");
            _wl_proxy_marshal_constructor_versioned = (struct wl_proxy * (*)(struct wl_proxy *proxy, uint32_t opcode, const struct wl_interface *interface, uint32_t version, ...))SDL_LoadFunction(wayland_client_handle, "wl_proxy_marshal_constructor_versioned");
            _wl_proxy_marshal = (void (*)(struct wl_proxy *p, uint32_t opcode, ...))SDL_LoadFunction(wayland_client_handle, "wl_proxy_marshal");
            _wl_proxy_destroy = (void (*)(struct wl_proxy *proxy))SDL_LoadFunction(wayland_client_handle, "wl_proxy_destroy");
            _wl_proxy_add_listener = (int (*)(struct wl_proxy *proxy, void (**implementation)(void), void *data))SDL_LoadFunction(wayland_client_handle, "wl_proxy_add_listener");
            _wl_display_roundtrip = (int (*)(struct wl_display *display))SDL_LoadFunction(wayland_client_handle, "wl_display_roundtrip");
        }
        struct wl_registry *registry = (struct wl_registry *)_wl_proxy_marshal_constructor((struct wl_proxy *)info.info.wl.display, 1, _wl_registry_interface, NULL);
        while (data_device_manager == NULL || seat == NULL) {
            if (!addedListener) {
                _wl_proxy_add_listener((struct wl_proxy*)registry, (void (**)(void))&registry_listener, NULL);
                addedListener = true;
            }
            _wl_display_roundtrip(info.info.wl.display);
        }
        struct wl_data_device *data_device = (struct wl_data_device *)_wl_proxy_marshal_constructor((struct wl_proxy *)data_device_manager, 1, _wl_data_device_interface, NULL, seat);
        struct wl_data_source *source = (struct wl_data_source *)_wl_proxy_marshal_constructor((struct wl_proxy *) data_device_manager, 0, _wl_data_source_interface, NULL);
        _wl_proxy_add_listener((struct wl_proxy *)source, (void (**)(void))&data_source_listener, NULL);
#ifndef NO_WEBP
        _wl_proxy_marshal((struct wl_proxy *)source, 0, "image/webp");
#endif
#ifndef NO_PNG
        _wl_proxy_marshal((struct wl_proxy *)source, 0, "image/png");
#endif
        _wl_proxy_marshal((struct wl_proxy *)source, 0, "image/bmp");
        if (*copiedSurface != NULL) SDL_FreeSurface(*copiedSurface);
        copiedSurface = SDL_CreateRGBSurfaceWithFormat(surf->flags, surf->w, surf->h, surf->format->BitsPerPixel, surf->format->format);
        SDL_BlitSurface(surf, NULL, *copiedSurface, NULL);
        _wl_proxy_marshal((struct wl_proxy *)data_device, 1, source, 0);
        // TODO: figure out if this leaks memory/resources? (I can't test Wayland very easily on my system)
#endif
    } else if (info.subsystem == SDL_SYSWM_DIRECTFB) {
#ifdef __DIRECTFB_H__

#endif
    }
}

#if defined(__i386__) || defined(__x86_64__)

/* This structure mirrors the one found in /usr/include/asm/ucontext.h */
typedef struct _sig_ucontext {
    unsigned long     uc_flags;
    struct ucontext   *uc_link;
    stack_t           uc_stack;
    struct sigcontext uc_mcontext;
    sigset_t          uc_sigmask;
} sig_ucontext_t;

void crit_err_hdlr(int sig_num, siginfo_t * info, void * ucontext) {
    void *             array[25];
    void *             caller_address;
    char **            messages;
    int                size, i;
    sig_ucontext_t *   uc;

    uc = (sig_ucontext_t *)ucontext;

/* Get the address at the time the signal was raised */
#if defined(__i386__) // gcc specific
    caller_address = (void *) uc->uc_mcontext.eip; // EIP: x86 specific
#elif defined(__x86_64__) // gcc specific
    caller_address = (void *) uc->uc_mcontext.rip; // RIP: x86_64 specific
#else
#error Unsupported architecture. // TODO: Add support for other arch.
#endif
    if (!loadingPlugin.empty()) fprintf(stderr, "Uh oh, CraftOS-PC has crashed! Reason: %s. It appears the plugin \"%s\" may have been responsible for this. Please remove it and try again.\n", strsignal(sig_num), loadingPlugin.c_str());
    else fprintf(stderr, "Uh oh, CraftOS-PC has crashed! Reason: %s (%d). Please report this to https://www.craftos-pc.cc/bugreport. Paste the following text under the 'Screenshots' section:\n", strsignal(sig_num), sig_num);
    fprintf(stderr, "OS: Linux\nAddress is %p from %p\nLast C function: %s\n", info->si_addr, (void *)caller_address, lastCFunction);
    size = backtrace(array, 25);
    /* overwrite sigaction with caller's address */
    array[1] = caller_address;
    messages = backtrace_symbols(array, size);
    /* skip first stack frame (points here) */
    for (i = 1; i < size && messages != NULL; ++i) 
        fprintf(stderr, "[bt]: (%d) %s\n", i, messages[i]);
    free(messages);
    signal(sig_num, NULL);
}

#define setSignalHandler(type) if (sigaction(type, &sigact, (struct sigaction *)NULL) != 0) \
        fprintf(stderr, "Error setting signal handler for %d (%s), continuing.\n", type, strsignal(type));

void setupCrashHandler() {
    struct sigaction sigact;
    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_sigaction = crit_err_hdlr;
    sigact.sa_flags = SA_RESTART | SA_SIGINFO;
    setSignalHandler(SIGSEGV);
    setSignalHandler(SIGILL);
    setSignalHandler(SIGBUS);
    setSignalHandler(SIGTRAP);
    setSignalHandler(SIGABRT);
}

#else
void setupCrashHandler() {}
#endif

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
    event.xclient.message_type = _XInternAtom (display, "_NET_WM_STATE", False);
    event.xclient.format = 32;

    event.xclient.data.l[0] = state ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
    event.xclient.data.l[1] = _XInternAtom (display, "_NET_WM_STATE_ABOVE", False);
    event.xclient.data.l[2] = 0; //unused.
    event.xclient.data.l[3] = 0;
    event.xclient.data.l[4] = 0;

    return _XSendEvent (display, DefaultRootWindow(display), False,
                       SubstructureRedirectMask|SubstructureNotifyMask, &event);
}
#endif

void setFloating(SDL_Window* win, bool state) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(win, &info);
    if (info.subsystem == SDL_SYSWM_X11) {
#ifdef _X11_XLIB_H_
        x11_window_set_on_top(info.info.x11.display, info.info.x11.window, state);
#endif
    } else if (info.subsystem == SDL_SYSWM_WAYLAND) {
        // Wayland doesn't support this :|
        fprintf(stderr, "Warning: Wayland does not support keeping windows on top.\n");
    } else if (info.subsystem == SDL_SYSWM_DIRECTFB) {
#ifdef __DIRECTFB_H__
        SetStackingClass(info.info.dfb.window, state ? DWSC_UPPER : DWSC_MIDDLE);
#endif
    }
}

void platformExit() {
#ifdef _X11_XLIB_H_
    if (x11_handle != NULL) SDL_UnloadObject(x11_handle);
#endif
#ifdef SDL_VIDEO_DRIVER_WAYLAND
    if (wayland_client_handle != NULL) SDL_UnloadObject(wayland_client_handle);
#endif
}

#endif // __INTELLISENSE__