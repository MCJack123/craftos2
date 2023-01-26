/*
 * platform/android.cpp
 * CraftOS-PC 2
 * 
 * This file implements functions specific to Android.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2023 JackMacWindows.
 */

#ifdef __ANDROID__ // disable error checking on Windows
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
#include <jni.h>
#include <libgen.h>
#include <pthread.h>
#include <SDL2/SDL_syswm.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <ucontext.h>
#include <unistd.h>
#include "../platform.hpp"
#include "../runtime.hpp"
#include "../termsupport.hpp"
#include "../util.hpp"
#include "../terminal/SDLTerminal.hpp"

path_t base_path;
path_t rom_path;

void setBasePath(path_t path) {
    base_path = path;
}

void setROMPath(path_t path) {
    rom_path = path;
}

path_t getBasePath() {
    if (base_path.empty()) {
        if (SDL_AndroidGetExternalStorageState() & (SDL_ANDROID_EXTERNAL_STORAGE_READ | SDL_ANDROID_EXTERNAL_STORAGE_WRITE))
            base_path = path_t(SDL_AndroidGetExternalStoragePath());
        else base_path = path_t(SDL_AndroidGetInternalStoragePath());
    }
    return base_path;
}

path_t getROMPath() {
    if (rom_path.empty()) {
        rom_path = path_t(SDL_AndroidGetInternalStoragePath()).parent_path() / "cache";
    }
    return rom_path;
}
path_t getPlugInPath() {
    if (rom_path.empty()) {
        rom_path = path_t(SDL_AndroidGetInternalStoragePath()).parent_path() / "cache";
    }
    return rom_path / "plugins";
}

path_t getMCSavePath() {
    return "";
}

void setThreadName(std::thread &t, const std::string& name) {
    pthread_setname_np(t.native_handle(), name.c_str());
}

void updateNow(const std::string& tag_name, const Poco::JSON::Object::Ptr root) {}

void migrateOldData() {}

void copyImage(SDL_Surface* surf, SDL_Window* win) {
    fprintf(stderr, "Warning: Android does not support taking screenshots to the clipboard.\n");
}

static std::vector<Poco::Crypto::X509Certificate> certificateStore;

void setupCrashHandler() {
    DIR *d = opendir("/system/etc/security/cacerts_google");
    if (d) {
        struct dirent *p;
        while ((p=readdir(d))) {
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;
            certificateStore.push_back(Poco::Crypto::X509Certificate("/system/etc/security/cacerts_google/" + std::string(p->d_name)));
        }
        closedir(d);
    }
    d = opendir("/system/etc/security/cacerts");
    if (d) {
        struct dirent *p;
        while ((p=readdir(d))) {
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;
            certificateStore.push_back(Poco::Crypto::X509Certificate("/system/etc/security/cacerts/" + std::string(p->d_name)));
        }
        closedir(d);
    }
    // TODO: add user certs
}

void setFloating(SDL_Window* win, bool state) {}

void platformExit() {}

void addSystemCertificates(Poco::Net::Context::Ptr context) {
    for (Poco::Crypto::X509Certificate& cert : certificateStore) context->addCertificateAuthority(cert);
}

void unblockInput() {}

void mobileResetModifiers() {
    JNIEnv * env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    jobject activity = (jobject)SDL_AndroidGetActivity();
    jclass activity_class = env->GetObjectClass(activity);
    jmethodID resetModifiers = env->GetMethodID(activity_class, "resetModifiers", "()V");
    env->CallVoidMethod(activity, resetModifiers);
}

#ifdef __INTELLISENSE__
#region Mobile API
#endif

std::string mobile_keyboard_open(lua_State *L, void* ud) {
    SDLTerminal * sdlterm = (SDLTerminal*)get_comp(L)->term;
    if (sdlterm == NULL || (sdlterm->charHeight*sdlterm->dpiScale) == 0) return "";
    int size = ((int)(ptrdiff_t)ud - 4*sdlterm->charScale*sdlterm->dpiScale) / (sdlterm->charHeight*sdlterm->dpiScale);
    if (size >= sdlterm->height) return "_CCPC_mobile_keyboard_close";
    lua_pushinteger(L, size);
    return "_CCPC_mobile_keyboard_open";
}

extern "C" {
JNIEXPORT void JNICALL Java_cc_craftospc_CraftOSPC_MainActivity_sendKeyboardUpdate(JNIEnv *env, jclass klass, int size) {
    LockGuard lock(computers);
    if (!computers->empty()) queueEvent(computers->front(), mobile_keyboard_open, (void*)(ptrdiff_t)size);
}

JNIEXPORT void JNICALL Java_cc_craftospc_CraftOSPC_MainActivity_sendCloseEvent(JNIEnv *env, jclass klass) {
    if (renderTargets.size() < 2) return;
    SDL_Event e;
    e.type = SDL_WINDOWEVENT;
    e.window.timestamp = time(0);
    e.window.windowID = SDL_GetWindowID(((SDLTerminal*)computers->front()->term)->win);
    e.window.event = SDL_WINDOWEVENT_CLOSE;
    e.window.data1 = 0;
    e.window.data2 = 0;
    SDL_PushEvent(&e);
}
}

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

static luaL_Reg android_reg[] = {
    {NULL, NULL}
};

int mobile_luaopen(lua_State *L) {
    luaL_register(L, "mobile", mobile_reg);
    /*lua_pushstring(L, "android");
    lua_newtable(L);
    for (luaL_Reg* r = android_reg; r->name && r->func; r++) {
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

#endif // __INTELLISENSE__
