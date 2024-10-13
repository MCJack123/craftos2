# SPDX-FileCopyrightText: 2019-2024 JackMacWindows
#
# SPDX-License-Identifier: MIT

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include

# Add your application source files here...
LOCAL_SRC_FILES := YourSourceHere.c

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_mixer PocoFoundation PocoNet PocoNetSSL ssl crypto PocoJSON PocoXML

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog

include $(BUILD_SHARED_LIBRARY)
