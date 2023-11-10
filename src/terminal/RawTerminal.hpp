/*
 * terminal/RawTerminal.hpp
 * CraftOS-PC 2
 * 
 * This file defines the RawTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2023 JackMacWindows.
 */

#ifndef TERMINAL_RAWTERMINAL_HPP
#define TERMINAL_RAWTERMINAL_HPP
#include <set>
#include <SDL2/SDL.h>
#include <Terminal.hpp>
#include "../runtime.hpp"

enum {
    CCPC_RAW_TERMINAL_DATA = 0,
    CCPC_RAW_KEY_DATA,
    CCPC_RAW_MOUSE_DATA,
    CCPC_RAW_EVENT_DATA,
    CCPC_RAW_TERMINAL_CHANGE,
    CCPC_RAW_MESSAGE_DATA,
    CCPC_RAW_FEATURE_FLAGS,
    CCPC_RAW_FILE_REQUEST,
    CCPC_RAW_FILE_RESPONSE,
    CCPC_RAW_FILE_DATA
};

enum {
    CCPC_RAW_FILE_REQUEST_EXISTS = 0,
    CCPC_RAW_FILE_REQUEST_ISDIR,
    CCPC_RAW_FILE_REQUEST_ISREADONLY,
    CCPC_RAW_FILE_REQUEST_GETSIZE,
    CCPC_RAW_FILE_REQUEST_GETDRIVE,
    CCPC_RAW_FILE_REQUEST_GETCAPACITY,
    CCPC_RAW_FILE_REQUEST_GETFREESPACE,
    CCPC_RAW_FILE_REQUEST_LIST,
    CCPC_RAW_FILE_REQUEST_ATTRIBUTES,
    CCPC_RAW_FILE_REQUEST_FIND,
    CCPC_RAW_FILE_REQUEST_MAKEDIR,
    CCPC_RAW_FILE_REQUEST_DELETE,
    CCPC_RAW_FILE_REQUEST_COPY,
    CCPC_RAW_FILE_REQUEST_MOVE
};

#define CCPC_RAW_FILE_REQUEST_OPEN         0x10
#define CCPC_RAW_FILE_REQUEST_OPEN_WRITE   0x01
#define CCPC_RAW_FILE_REQUEST_OPEN_APPEND  0x02
#define CCPC_RAW_FILE_REQUEST_OPEN_BINARY  0x04

#define CCPC_RAW_FEATURE_FLAG_BINARY_CHECKSUM        0x0001
#define CCPC_RAW_FEATURE_FLAG_FILESYSTEM_SUPPORT     0x0002
#define CCPC_RAW_FEATURE_FLAG_SEND_ALL_WINDOWS       0x0004
#define CCPC_RAW_FEATURE_FLAG_HAS_EXTENDED_FEATURES  0x8000

class RawTerminal: public Terminal {
public:
    static uint16_t supportedFeatures;
    static uint32_t supportedExtendedFeatures;
    uint8_t computerID;
    static void init();
    static void quit();
    static void pollEvents() {defaultPollEvents();}
    static void showGlobalMessage(uint32_t flags, const char * title, const char * message);
    static void initClient(uint16_t flags, uint32_t extflags = 0);
    RawTerminal(std::string title, uint8_t computerID = 0);
    ~RawTerminal() override;
    void render() override;
    bool resize(unsigned w, unsigned h) override;
    void showMessage(uint32_t flags, const char * title, const char * message) override;
    void setLabel(std::string label) override;
    void onActivate() override {}
};

extern void sendRawEvent(SDL_Event e);

#endif