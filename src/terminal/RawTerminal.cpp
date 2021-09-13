/*
 * terminal/RawTerminal.cpp
 * CraftOS-PC 2
 * 
 * This file implements the RawTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <Poco/Checksum.h>
#include "RawTerminal.hpp"
#include "SDLTerminal.hpp"
#include "../apis.hpp"
#include "../main.hpp"
#include "../runtime.hpp"
#include "../termsupport.hpp"

/* Data spec

  See https://www.craftos-pc.cc/docs/rawmode for the full specification sheet.
  This file only contains a basic overview of the format.
  
* Common Header

  Offset     Bytes      Purpose
  0x00       4          Header ("!CPC")
  0x04       4          Size (hex string)
  ===================== Base64 payload
  0x00       1          Frame type ID
  0x01       1          Window ID

Note: As of version 1.1, headers for payloads >65535 bytes in size will have the following format instead:

  Offset     Bytes      Purpose
  0x00       4          Header ("!CPD")
  0x04       12         Size (hex string)

* Type 0: Terminal contents (server -> client)

  Offset     Bytes      Purpose
  0x02       1          Graphics mode
  0x03       1          Cursor blinking? (previously showing)
  0x04       2          Width
  0x06       2          Height
  0x08       2          Cursor X
  0x0A       2          Cursor Y
  0x0C       1          Grayscale? (1 = grayscale, 0 = color) -- if grayscale, render colors with (r + g + b) / 3
  0x0D       3          Reserved
  ===================== Screen data
  --------------------- Text mode (mode 0)
  0x10       *x*        RLE-encoded text (length of expanded RLE = width * height)
  0x10 + x   *y*        RLE-encoded background pairs (high nybble = BG, low nybble = FG)
  --------------------- Graphics modes (modes 1/2)
  0x10       *x*        RLE-encoded pixel data (length of expanded RLE = width * height * 54)
  ===================== End screen data
  ===================== Palette
  --------------------- Text mode / 16 color GFX mode (modes 0/1)
  0x10+x[+y] 48         RGB palette x16
  --------------------- 256 color GFX mode (mode 2)
  0x10+x     768        RGB palette x256
  ===================== End palette

* Type 1: Key event data (client -> server)

  Offset     Bytes      Purpose
  0x02       1          Key ID (as in keys API) or character (depending on bit 3 of flags)
  0x03       1          Bit 0 = key (1) or key_up (0), bit 1 = is_held, bit 2 = control held, bit 3 = character (1) or key (0)

* Type 2: Mouse event data (client -> server)

  Offset     Bytes      Purpose
  0x02       1          Event type: 0 = mouse_click, 1 = mouse_up, 2 = mouse_scroll, 3 = mouse_drag
  0x03       1          Button ID, or scroll direction (0 = up, 1 = down)
  0x04       4          X position (character in mode 0, pixel in mode 1/2)
  0x08       4          Y position

* Type 3: Generic event data (client -> server)
  
  Offset     Bytes      Purpose
  0x02       1          Number of parameters
  0x03       *x*        Event name (NUL-terminated)
  ===================== Event parameters
  0x00       1          Data type: 0 = 32-bit integer, 1 = double, 2 = boolean, 3 = string, 4 = table, 5 = nil (no data after type)
    0x01     4          Type 0: Integer data
    0x01     8          Type 1: Double data
    0x01     1          Type 2: Boolean data
    0x01     *y*        Type 3: String data (NUL-terminated)
    =================== Type 4: Table data
    0x01     1          Number of entries
    0x02     *a*        Key data (same as parameter data)
    0x02+a   *b*        Value data (same as parameter data)
    =================== End type 4: table data
  ===================== End event parameters

* Type 4: Terminal change (use this to detect new windows) (either -> either)
    
  Offset     Bytes      Purpose
  0x02       1          Set to 1 when closing window or 2 when quitting program (if so, other fields may be any value)
  0x03       1          If opening a window and this is > 0, provides the ID + 1 of the computer this references (v2.5.6+)
                        If opening a window and this is = 0, either the window is a monitor or this field is not supported
                        If not opening a window, this field is reserved
  0x04       2          Width
  0x06       2          Height
  0x08       *x*        Title (NUL-terminated) (ignored when sending client -> server)

* Type 5: Show message (server -> client)
  
  Offset     Bytes      Purpose
  0x02       4          Flags from SDL message event
  0x06       *x*        Title (NUL-terminated)
  0x06+x     *y*        Message (NUL-terminated)

* Type 6: Version support flags (either -> either)
  
  Offset     Bytes      Purpose
  0x02       2          Standard flags as a bitfield - each bit represents a supported feature
                        0: Binary data CRC-32 checksum support (as opposed to checksumming the Base64 data)
                        1: Filesystem support extension
                        2-14: Currently reserved, set to 0
                        15: Set if the extended flags are present
  [0x04]    [4]         Extended flags as a bitfield - only available if bit 15 of standard flags is set
                        0-31: Currently reserved, set to 0

When a client supporting Type 6 packets connects to a server, it SHOULD send a Type 6 packet with its capabilities.
If a server receives a Type 6 packet and knows about its existence, it MUST send back a packet specifying its capabilities.
If the client doesn't send or receive a Type 6 packet, both parties MUST assume none of the above capabilities are supported.
Once the server sends back a response, both the server and client MUST communicate using the common capabilities between the two.
(A way to determine common capabilities is through a bitwise AND of the support flags.)

== Filesystem support extension ==

* Type 7: File request (client -> server)

  Offset    Bytes       Purpose
  0x02      1           Request type: 0 = exists, 1 = isDir, 2 = isReadOnly, 3 = getSize, 
                            4 = getDrive, 5 = getCapacity, 6 = getFreeSpace, 7 = list,
                            8 = attributes, 9 = find, 10 = makeDir, 11 = delete,
                            12 = copy, 13 = move, 16 = file read/write
                        If type is 16 (file read/write), the low 3 bits are used as follows:
                            Bit 0: read (0)/write (1)
                            Bit 1: append? (ignored on read)
                            Bit 2: binary?
  0x03      1           Unique request ID to be sent back to the client
  0x04      *x*         Path to the file to check (NUL-terminated)
  0x04+x    *y*         For types 12 and 13, the second path (NUL-terminated)

  Note: If the packet is a write request, this packet type MUST be followed by a Type 9 File data packet.
  To write to a file, first send a Type 7 packet specifying the open mode and path; then send a Type 9 packet with the data to write.
  If the request is a read operation, the next packet will be a Type 9 packet - no Type 8 packet will be sent.

* Type 8: File response (server -> client)

  Offset    Bytes       Purpose
  0x02      1           Request type
  0x03      1           Unique request ID this response belongs to
  ===================== Response data
  --------------------- Non-returning operations (makeDir, delete, copy, move, write operations)
  0x04      *x*         0 if success, otherwise a NUL-terminated string containing an error message
  --------------------- Boolean operations (exists, isDir, isReadOnly)
  0x04      1           0 = false, 1 = true, 2 = error
  --------------------- Integer operations (getSize, getCapacity, getFreeSpace)
  0x04      4           The size reported from the function (0xFFFFFFFF = error)
  --------------------- String operations (getDrive)
  0x04      *x*         The path/name of the drive (NUL-terminated, "" = error)
  --------------------- List results (list, find)
  0x04      4           Number of entries in the list (0xFFFFFFFF = error)
  0x08      *y*         List of files, with each string NUL-terminated
  --------------------- Attributes (attributes)
  0x04      4           File size
  0x08      8           Creation time (in milliseconds; integer)
  0x10      8           Modification time (in milliseconds; integer)
  0x18      1           Is directory?
  0x19      1           Is read only?
  0x1A      1           Error flag (0 = ok, 1 = does not exist, 2 = error)
  0x1B      1           Reserved
  ===================== End response data

* Type 8: File data (either -> either)

  Offset    Bytes       Purpose
  0x02      1           Set to 1 if an error occurred while accessing the file
                        An error message will be sent as file data if set
  0x03      1           Unique request ID this response belongs to
  0x04      4           Size of the data, in bytes
  0x08      *x*         File data

== End filesystem support extension ==

* Common Footer

  ===================== End Base64 payload
  END-4      8          CRC32 of payload (hex string)
  END        1          Newline

*/

uint16_t RawTerminal::supportedFeatures = 0;
uint32_t RawTerminal::supportedExtendedFeatures = 0;
std::function<void(const std::string&)> rawWriter = [](const std::string& data){
    std::cout << data;
    std::cout.flush();
};
static std::thread * inputThread;
static bool isVersion1_1 = false;
static std::string fileWriteRequests[256];

static void sendRawData(const uint8_t type, const uint8_t id, const std::function<void(std::ostream&)>& callback) {
    std::stringstream output;
    output.put(type);
    output.put(id);
    callback(output);
    std::string str = b64encode(output.str());
    str.erase(std::remove_if(str.begin(), str.end(), [](char c)->bool {return c == '\n' || c == '\r'; }), str.end());
    Poco::Checksum chk;
    if (type != CCPC_RAW_FEATURE_FLAGS && (RawTerminal::supportedFeatures & CCPC_RAW_FEATURE_FLAG_BINARY_CHECKSUM)) chk.update(output.str());
    else chk.update(str);
    const uint32_t sum = chk.checksum();
    char tmpdata[21];
    if (str.length() > 65535) {
        if (isVersion1_1) {
            snprintf(tmpdata, 21, "%012zX%08x", str.length(), sum);
            rawWriter("!CPD" + std::string(tmpdata, 12) + str + std::string(tmpdata + 12, 8) + "\n");
        } else fprintf(stderr, "Attempted to send raw packet that's too large to a client that doesn't support large packets (%zu bytes); dropping packet.", str.length());
    } else {
        snprintf(tmpdata, 13, "%04X%08x", (unsigned)str.length(), sum);
        rawWriter("!CPC" + std::string(tmpdata, 4) + str + std::string(tmpdata + 4, 8) + "\n");
    }
}

static void parseIBTTag(std::istream& in, lua_State *L) {
    const char type = (char)in.get();
    if (type == 0) {
        uint32_t num = 0;
        in.read((char*)&num, 4);
        lua_pushinteger(L, num);
    } else if (type == 1) {
        double num = 0;
        in.read((char*)&num, sizeof(double));
        lua_pushnumber(L, num);
    } else if (type == 2) {
        lua_pushboolean(L, in.get());
    } else if (type == 3) {
        std::string str;
        char c;
        while ((c = (char)in.get())) str += c;
        lua_pushstring(L, str.c_str());
    } else if (type == 4) {
        lua_newtable(L);
        for (uint8_t items = in.get(); items; items--) {
            parseIBTTag(in, L);
            parseIBTTag(in, L);
            lua_settable(L, -3);
        }
    } else {
        lua_pushnil(L);
    }
}

void sendRawEvent(SDL_Event e) {
    if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && (selectedRenderer != 0 || keymap.find(e.key.keysym.sym) != keymap.end())) 
        sendRawData(CCPC_RAW_KEY_DATA, rawClientTerminalIDs[e.key.windowID], [e](std::ostream& output) {
            if (selectedRenderer == 0) output.put(keymap.at(e.key.keysym.sym));
            else output.put((char)e.key.keysym.sym);
            output.put((e.type == SDL_KEYUP) | (((e.key.keysym.mod & KMOD_CTRL) != 0) << 2));
        });
    else if (e.type == SDL_TEXTINPUT)
        sendRawData(CCPC_RAW_KEY_DATA, rawClientTerminalIDs[e.text.windowID], [e](std::ostream& output) {
            output.put(e.text.text[0]);
            output.put(0x08);
        });
    else if (e.type == SDL_MOUSEBUTTONDOWN) {
        Terminal * term = rawClientTerminals[rawClientTerminalIDs[e.key.windowID]];
        int x = 1, y = 1;
        if (selectedRenderer >= 2 && selectedRenderer <= 4) {
            x = e.button.x; y = e.button.y;
        } else if (dynamic_cast<SDLTerminal*>(term) != NULL) {
            x = convertX(dynamic_cast<SDLTerminal*>(term), e.button.x); y = convertY(dynamic_cast<SDLTerminal*>(term), e.button.y);
        }
        if (term->lastMouse.x == x && term->lastMouse.y == y && term->lastMouse.button == e.button.button && term->lastMouse.event == 0) return;
        int button;
        switch (e.button.button) {
            case SDL_BUTTON_LEFT: button = 1; break;
            case SDL_BUTTON_RIGHT: button = 2; break;
            case SDL_BUTTON_MIDDLE: button = 3; break;
            default:
                if (config.standardsMode) return;
                else button = e.button.button;
                break;
        }
        term->lastMouse = {x, y, e.button.button, 0, ""};
        term->mouseButtonOrder.push_back(e.button.button);
        sendRawData(CCPC_RAW_MOUSE_DATA, rawClientTerminalIDs[e.window.windowID], [button, x, y](std::ostream &output) {
            output.put(0);
            output.put(button);
            output.write((const char*)&x, 4);
            output.write((const char*)&y, 4);
        });
    } else if (e.type == SDL_MOUSEBUTTONUP) {
        Terminal * term = rawClientTerminals[rawClientTerminalIDs[e.key.windowID]];
        int x = 1, y = 1;
        if (selectedRenderer >= 2 && selectedRenderer <= 4) {
            x = e.button.x; y = e.button.y;
        } else if (dynamic_cast<SDLTerminal*>(term) != NULL) {
            x = convertX(dynamic_cast<SDLTerminal*>(term), e.button.x); y = convertY(dynamic_cast<SDLTerminal*>(term), e.button.y);
        }
        if (term->lastMouse.x == x && term->lastMouse.y == y && term->lastMouse.button == e.button.button && term->lastMouse.event == 1) return;
        int button;
        switch (e.button.button) {
            case SDL_BUTTON_LEFT: button = 1; break;
            case SDL_BUTTON_RIGHT: button = 2; break;
            case SDL_BUTTON_MIDDLE: button = 3; break;
            default:
                if (config.standardsMode) return;
                else button = e.button.button;
                break;
        }
        term->lastMouse = {x, y, e.button.button, 1, ""};
        term->mouseButtonOrder.remove(e.button.button);
        sendRawData(CCPC_RAW_MOUSE_DATA, rawClientTerminalIDs[e.window.windowID], [button, x, y](std::ostream &output) {
            output.put(1);
            output.put(button);
            output.write((const char*)&x, 4);
            output.write((const char*)&y, 4);
        });
    } else if (e.type == SDL_MOUSEWHEEL) {
        SDLTerminal * term = dynamic_cast<SDLTerminal*>(rawClientTerminals[rawClientTerminalIDs[e.key.windowID]]);
        if (term == NULL) {
            return;
        } else {
            int x = 0, y = 0;
            term->getMouse(&x, &y);
            x = convertX(term, x);
            y = convertY(term, y);
            sendRawData(CCPC_RAW_MOUSE_DATA, rawClientTerminalIDs[e.window.windowID], [e, x, y](std::ostream &output) {
                output.put(2);
                output.put(max(min(-e.wheel.y, 1), -1));
                output.write((const char*)&x, 4);
                output.write((const char*)&y, 4);
            });
        }
    } else if (e.type == SDL_MOUSEMOTION && e.motion.state) {
        SDLTerminal * term = dynamic_cast<SDLTerminal*>(rawClientTerminals[rawClientTerminalIDs[e.key.windowID]]);
        if (term == NULL) return;
        int x = 1, y = 1;
        if (selectedRenderer >= 2 && selectedRenderer <= 4) {
            x = e.motion.x; y = e.motion.y;
        } else if (term != NULL) {
            x = convertX(term, e.motion.x); y = convertY(dynamic_cast<SDLTerminal*>(term), e.motion.y);
        }
        std::list<Uint8> used_buttons;
        for (Uint8 i = 0; i < 32; i++) if (e.motion.state & (1 << i)) used_buttons.push_back(i + 1);
        for (auto it = term->mouseButtonOrder.begin(); it != term->mouseButtonOrder.end();) {
            auto pos = std::find(used_buttons.begin(), used_buttons.end(), *it);
            if (pos == used_buttons.end()) it = term->mouseButtonOrder.erase(it);
            else ++it;
        }
        Uint8 button = used_buttons.back();
        if (!term->mouseButtonOrder.empty()) button = term->mouseButtonOrder.back();
        if (button == SDL_BUTTON_MIDDLE) button = 3;
        else if (button == SDL_BUTTON_RIGHT) button = 2;
        if ((term->lastMouse.x == x && term->lastMouse.y == y && term->lastMouse.button == button && term->lastMouse.event == 2) || (config.standardsMode && button > 3)) return;
        term->lastMouse = {x, y, button, 2, ""};
        sendRawData(CCPC_RAW_MOUSE_DATA, rawClientTerminalIDs[e.window.windowID], [button, x, y](std::ostream &output) {
            output.put(3);
            output.put(button);
            output.write((const char*)&x, 4);
            output.write((const char*)&y, 4);
        });
    } else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE)
        sendRawData(CCPC_RAW_TERMINAL_CHANGE, rawClientTerminalIDs[e.window.windowID], [](std::ostream &output) {
            output.put(1);
            for (int i = 0; i < 6; i++) output.put(0);
        });
    else if (e.type == SDL_QUIT)
        sendRawData(CCPC_RAW_TERMINAL_CHANGE, rawClientTerminalIDs[e.window.windowID], [](std::ostream &output) {
            output.put(2);
            for (int i = 0; i < 6; i++) output.put(0);
        });
}

struct rawMouseProviderData {
    uint8_t evtype;
    uint8_t button;
    uint32_t x;
    uint32_t y;
};

static std::string rawMouseProvider(lua_State *L, void* data) {
    struct rawMouseProviderData * d = (rawMouseProviderData*)data;
    if (config.standardsMode && d->button > 3) {
        delete d;
        return "";
    }
    lua_pushinteger(L, d->button);
    lua_pushinteger(L, d->x);
    lua_pushinteger(L, d->y);
    std::string retval;
    if (d->evtype == 0) retval = "mouse_click";
    else if (d->evtype == 1) retval = "mouse_up";
    else if (d->evtype == 2) retval = "mouse_scroll";
    else if (d->evtype == 3) retval = "mouse_drag";
    delete d;
    return retval;
}

static std::string rawEventProvider(lua_State *L, void* data) {
    std::stringstream& in = *(std::stringstream*)data;
    const uint8_t paramCount = in.get();
    char c;
    std::string name;
    while ((c = in.get())) name += c;
    for (int i = 0; i < paramCount; i++) parseIBTTag(in, L);
    delete (std::stringstream*)data;
    return name;
}

static lua_CFunction findLibraryFunction(luaL_Reg * lib, const char * name) {
    for (; lib->name != NULL; lib++)
        if (strcmp(lib->name, name) == 0)
            return lib->func;
    return NULL;
}

static void rawInputLoop() {
    while (!exiting) {
        unsigned char cc = std::cin.get();
        if (cc == '!' && std::cin.get() == 'C' && std::cin.get() == 'P') {
            char protocol_type = std::cin.get();
            size_t sizen;
            if (protocol_type == 'C') {
                char size[5];
                std::cin.read(size, 4);
                sizen = strtoul(size, NULL, 16);
            } else if (isVersion1_1 && protocol_type == 'D') {
                char size[13];
                std::cin.read(size, 12);
                sizen = strtoul(size, NULL, 16);
            } else continue;
            char * tmp = new char[sizen+1];
            tmp[sizen] = 0;
            std::cin.read(tmp, sizen);
            std::string ddata;
            try {
                ddata = b64decode(std::string(tmp, sizen));
            } catch (std::exception &e) {
                fprintf(stderr, "Could not decode Base64: %s\n", e.what());
                delete[] tmp;
                continue;
            } 
            Poco::Checksum chk;
            if (RawTerminal::supportedFeatures & CCPC_RAW_FEATURE_FLAG_BINARY_CHECKSUM) chk.update(ddata);
            else chk.update(tmp, sizen);
            delete[] tmp;
            char hexstr[9];
            std::cin.read(hexstr, 8);
            hexstr[8] = 0;
            if (chk.checksum() != strtoul(hexstr, NULL, 16)) {
                fprintf(stderr, "Invalid checksum: expected %08X, got %08lX\n", chk.checksum(), strtoul(hexstr, NULL, 16));
                continue;
            }
            std::stringstream in(ddata);

            SDL_Event e;
            memset(&e, 0, sizeof(SDL_Event));
            std::string tmps;
            uint8_t type = in.get();
            uint8_t id = in.get();
            switch (type) {
            case CCPC_RAW_KEY_DATA: {
                uint8_t key = in.get();
                uint8_t flags = in.get();
                if (flags & 8) {
                    e.type = SDL_TEXTINPUT;
                    e.text.windowID = id;
                    e.text.text[0] = key;
                    e.text.text[1] = '\0';
                    LockGuard lockc(computers);
                    for (Computer * c : *computers) {
                        if (checkWindowID(c, e.key.windowID)) {
                            std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
                            e.text.windowID = c->term->id;
                            c->termEventQueue.push(e);
                            c->event_lock.notify_all();
                        }
                    }
                } else if ((flags & 9) == 1) {
                    e.type = SDL_KEYUP;
                    e.key.windowID = id;
                    e.key.keysym.sym = (SDL_Keycode)key;
                    if (flags & 4) e.key.keysym.mod = KMOD_CTRL;
                    LockGuard lockc(computers);
                    for (Computer * c : *computers) {
                        if (checkWindowID(c, e.key.windowID)) {
                            std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
                            e.key.windowID = c->term->id;
                            c->termEventQueue.push(e);
                            c->event_lock.notify_all();
                        }
                    }
                } else {
                    e.type = SDL_KEYDOWN;
                    e.key.windowID = id;
                    e.key.keysym.sym = (SDL_Keycode)key;
                    if (flags & 4) e.key.keysym.mod = KMOD_CTRL;
                    LockGuard lockc(computers);
                    for (Computer * c : *computers) {
                        if (checkWindowID(c, e.key.windowID)) {
                            std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
                            e.key.windowID = c->term->id;
                            c->termEventQueue.push(e);
                            c->event_lock.notify_all();
                        }
                    }
                }
                break;
            } case CCPC_RAW_MOUSE_DATA: {
                uint8_t evtype = in.get();
                uint8_t button = in.get();
                uint32_t x = 0, y = 0;
                in.read((char*)&x, 4);
                in.read((char*)&y, 4);
                LockGuard lockc(computers);
                for (Computer * c : *computers) {
                    if (checkWindowID(c, id)) {
                        struct rawMouseProviderData * d = new struct rawMouseProviderData;
                        d->evtype = evtype;
                        d->button = button;
                        d->x = x;
                        d->y = y;
                        queueEvent(c, rawMouseProvider, d);
                    }
                }
                break;
            } case CCPC_RAW_EVENT_DATA: {
                LockGuard lockc(computers);
                for (Computer * c : *computers) {
                    if (checkWindowID(c, id)) {
                        std::stringstream * ss = new std::stringstream(in.str().substr(2));
                        queueEvent(c, rawEventProvider, ss);
                    }
                }
                break;
            } case CCPC_RAW_TERMINAL_CHANGE: {
                int isClosing = in.get();
                if (isClosing == 1) {
                    e.type = SDL_WINDOWEVENT;
                    e.window.event = SDL_WINDOWEVENT_CLOSE;
                    LockGuard lockc(computers);
                    for (Computer * c : *computers) {
                        if (checkWindowID(c, id)) {
                            std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
                            e.window.windowID = id;
                            c->termEventQueue.push(e);
                            c->event_lock.notify_all();
                        }
                    }
                    for (Terminal * t : orphanedTerminals) {
                        if (t->id == id) {
                            orphanedTerminals.erase(t);
                            delete t;
                            break;
                        }
                    }
                } else if (isClosing == 2) {
                    e.type = SDL_QUIT;
                    LockGuard lockc(computers);
                    for (Computer * c : *computers) {
                        std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
                        c->termEventQueue.push(e);
                        c->event_lock.notify_all();
                    }
                } else {
                    in.get(); // reserved
                    uint16_t w = 0, h = 0;
                    in.read((char*)&w, 2);
                    in.read((char*)&h, 2);
                    e.type = SDL_WINDOWEVENT;
                    e.window.windowID = id;
                    e.window.event = SDL_WINDOWEVENT_RESIZED;
                    e.window.data1 = w;
                    e.window.data2 = h;
                    LockGuard lockc(computers);
                    for (Computer * c : *computers) {
                        if (checkWindowID(c, e.window.windowID)) {
                            std::lock_guard<std::mutex> lock(c->termEventQueueMutex);
                            c->termEventQueue.push(e);
                            c->event_lock.notify_all();
                        }
                    }
                }
                break;
            } case CCPC_RAW_FEATURE_FLAGS: {
                isVersion1_1 = true;
                in.read((char*)&RawTerminal::supportedFeatures, 2);
                RawTerminal::supportedFeatures &= CCPC_RAW_FEATURE_FLAG_BINARY_CHECKSUM | CCPC_RAW_FEATURE_FLAG_FILESYSTEM_SUPPORT | CCPC_RAW_FEATURE_FLAG_SEND_ALL_WINDOWS;
                if (RawTerminal::supportedFeatures & CCPC_RAW_FEATURE_FLAG_HAS_EXTENDED_FEATURES) {
                    in.read((char*)&RawTerminal::supportedExtendedFeatures, 4);
                    RawTerminal::supportedExtendedFeatures &= 0x00000000;
                }
                sendRawData(CCPC_RAW_FEATURE_FLAGS, id, [](std::ostream& out) {
                    out.write((char*)&RawTerminal::supportedFeatures, 2);
                    if (RawTerminal::supportedFeatures & CCPC_RAW_FEATURE_FLAG_HAS_EXTENDED_FEATURES) out.write((char*)&RawTerminal::supportedExtendedFeatures, 4);
                });
                if (RawTerminal::supportedFeatures & CCPC_RAW_FEATURE_FLAG_SEND_ALL_WINDOWS) {
                    std::lock_guard<std::mutex> rlock(renderTargetsLock);
                    for (Terminal * t : renderTargets) {
                        RawTerminal * term = dynamic_cast<RawTerminal*>(t);
                        if (term != NULL) {
                            sendRawData(CCPC_RAW_TERMINAL_CHANGE, id, [term](std::ostream& output) {
                                output.put(0);
                                output.put(term->computerID);
                                output.write((char*)&term->width, 2);
                                output.write((char*)&term->height, 2);
                                output.write(term->title.c_str(), term->title.size());
                                output.put(0);
                            });
                        }
                    }
                }
                break;
            } case CCPC_RAW_FILE_REQUEST: {
                if (!(RawTerminal::supportedFeatures & CCPC_RAW_FEATURE_FLAG_FILESYSTEM_SUPPORT)) break;
                uint8_t reqtype = in.get();
                uint8_t reqid = in.get();
                std::string path, path2;
                char c;
                while ((c = (char)in.get())) path += c;
                if (reqtype == CCPC_RAW_FILE_REQUEST_COPY || reqtype == CCPC_RAW_FILE_REQUEST_MOVE) while ((c = (char)in.get())) path2 += c;
                Computer * comp = NULL;
                LockGuard lockc(computers);
                for (Computer * c : *computers) {
                    if (checkWindowID(c, id)) {
                        comp = c;
                        break;
                    }
                }
                if (comp == NULL || comp->rawFileStack == NULL) {
                    if ((reqtype & 0xF0) == CCPC_RAW_FILE_REQUEST_OPEN && !(reqtype & CCPC_RAW_FILE_REQUEST_OPEN_WRITE)) sendRawData(CCPC_RAW_FILE_DATA, id, [reqid](std::ostream& out) {
                        out.put(1);
                        out.put(reqid);
                        out.put(39); out.put(0); out.put(0); out.put(0);
                        out.write("Could not find computer for this window", 39);
                    }); else sendRawData(CCPC_RAW_FILE_RESPONSE, id, [reqtype, reqid](std::ostream& out) {
                        out.put(reqtype);
                        out.put(reqid);
                        switch (reqtype) {
                            case CCPC_RAW_FILE_REQUEST_MAKEDIR:
                            case CCPC_RAW_FILE_REQUEST_DELETE:
                            case CCPC_RAW_FILE_REQUEST_COPY:
                            case CCPC_RAW_FILE_REQUEST_MOVE:
                            case CCPC_RAW_FILE_REQUEST_OPEN | CCPC_RAW_FILE_REQUEST_OPEN_WRITE:
                            case CCPC_RAW_FILE_REQUEST_OPEN | CCPC_RAW_FILE_REQUEST_OPEN_WRITE | CCPC_RAW_FILE_REQUEST_OPEN_APPEND:
                            case CCPC_RAW_FILE_REQUEST_OPEN | CCPC_RAW_FILE_REQUEST_OPEN_WRITE | CCPC_RAW_FILE_REQUEST_OPEN_BINARY:
                            case CCPC_RAW_FILE_REQUEST_OPEN | CCPC_RAW_FILE_REQUEST_OPEN_WRITE | CCPC_RAW_FILE_REQUEST_OPEN_APPEND | CCPC_RAW_FILE_REQUEST_OPEN_BINARY:
                                out.write("Could not find computer for this window", 40);
                                break;
                            case CCPC_RAW_FILE_REQUEST_EXISTS:
                            case CCPC_RAW_FILE_REQUEST_ISDIR:
                            case CCPC_RAW_FILE_REQUEST_ISREADONLY:
                                out.put(2);
                                break;
                            case CCPC_RAW_FILE_REQUEST_GETSIZE:
                            case CCPC_RAW_FILE_REQUEST_GETCAPACITY:
                            case CCPC_RAW_FILE_REQUEST_GETFREESPACE:
                            case CCPC_RAW_FILE_REQUEST_LIST:
                            case CCPC_RAW_FILE_REQUEST_FIND:
                                out.put(0xFF); out.put(0xFF); out.put(0xFF); out.put(0xFF);
                                break;
                            case CCPC_RAW_FILE_REQUEST_GETDRIVE:
                                out.put(0);
                                break;
                            case CCPC_RAW_FILE_REQUEST_ATTRIBUTES:
                                for (int i = 0; i < 22; i++) out.put(0);
                                out.put(2);
                                out.put(0);
                                break;
                        }
                    });
                    break;
                }
                std::lock_guard<std::mutex> lock(comp->rawFileStackMutex);
                if ((reqtype & 0xF0) == CCPC_RAW_FILE_REQUEST_OPEN) {
                    if (!(reqtype & CCPC_RAW_FILE_REQUEST_OPEN_WRITE)) sendRawData(CCPC_RAW_FILE_DATA, id, [reqtype, reqid, comp, &path](std::ostream &out) {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "open"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        lua_pushstring(comp->rawFileStack, (reqtype & CCPC_RAW_FILE_REQUEST_OPEN_BINARY) ? "rb" : "r");
                        std::string data;
                        bool err = false;
                        if (lua_pcall(comp->rawFileStack, 2, 2, 0)) {
                            err = true;
                            data = lua_tostring(comp->rawFileStack, -1);
                            lua_pop(comp->rawFileStack, 1);
                        } else if (lua_isnil(comp->rawFileStack, -2)) {
                            err = true;
                            data = lua_tostring(comp->rawFileStack, -1);
                            lua_pop(comp->rawFileStack, 2);
                        } else {
                            lua_pop(comp->rawFileStack, 1);
                            lua_getfield(comp->rawFileStack, -1, "readAll");
                            lua_call(comp->rawFileStack, 0, 1);
                            if (lua_isnil(comp->rawFileStack, -1)) data = ""; // shouldn't happen
                            else data = std::string(lua_tostring(comp->rawFileStack, -1), lua_strlen(comp->rawFileStack, -1));
                            lua_pop(comp->rawFileStack, 1);
                            lua_getfield(comp->rawFileStack, -1, "close");
                            lua_call(comp->rawFileStack, 0, 0);
                            lua_pop(comp->rawFileStack, 1);
                        }
                        out.put(err);
                        out.put(reqid);
                        uint32_t size = data.size();
                        out.write((char*)&size, 4);
                        out.write(data.c_str(), size);
                    }); else fileWriteRequests[reqid] = (char)reqtype + path;
                } else sendRawData(CCPC_RAW_FILE_RESPONSE, id, [reqtype, reqid, comp, &path, &path2](std::ostream& out) {
                    out.put(reqtype);
                    out.put(reqid);
                    switch (reqtype) {
                    case CCPC_RAW_FILE_REQUEST_EXISTS: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "exists"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        if (lua_pcall(comp->rawFileStack, 1, 1, 0)) out.put(2);
                        else out.put(lua_toboolean(comp->rawFileStack, -1));
                        lua_pop(comp->rawFileStack, 1);
                        break;
                    } case CCPC_RAW_FILE_REQUEST_ISDIR: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "isDir"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        if (lua_pcall(comp->rawFileStack, 1, 1, 0)) out.put(2);
                        else out.put(lua_toboolean(comp->rawFileStack, -1));
                        lua_pop(comp->rawFileStack, 1);
                        break;
                    } case CCPC_RAW_FILE_REQUEST_ISREADONLY: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "isReadOnly"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        if (lua_pcall(comp->rawFileStack, 1, 1, 0)) out.put(2);
                        else out.put(lua_toboolean(comp->rawFileStack, -1));
                        lua_pop(comp->rawFileStack, 1);
                        break;
                    } case CCPC_RAW_FILE_REQUEST_GETSIZE: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "getSize"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        uint32_t size;
                        if (lua_pcall(comp->rawFileStack, 1, 1, 0)) size = 0xFFFFFFFF;
                        else size = lua_tointeger(comp->rawFileStack, -1);
                        out.write((char*)&size, 4);
                        lua_pop(comp->rawFileStack, 1);
                        break;
                    } case CCPC_RAW_FILE_REQUEST_GETDRIVE: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "getDrive"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        std::string str;
                        if (lua_pcall(comp->rawFileStack, 1, 1, 0)) str = "";
                        else str = lua_tostring(comp->rawFileStack, -1);
                        out.write(str.c_str(), str.size());
                        out.put(0);
                        lua_pop(comp->rawFileStack, 1);
                        break;
                    } case CCPC_RAW_FILE_REQUEST_GETCAPACITY: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "getCapacity"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        uint32_t size;
                        if (lua_pcall(comp->rawFileStack, 1, 1, 0)) size = 0xFFFFFFFF;
                        else size = lua_tointeger(comp->rawFileStack, -1);
                        out.write((char*)&size, 4);
                        lua_pop(comp->rawFileStack, 1);
                        break;
                    } case CCPC_RAW_FILE_REQUEST_GETFREESPACE: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "getFreeSpace"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        uint32_t size;
                        if (lua_pcall(comp->rawFileStack, 1, 1, 0)) size = 0xFFFFFFFF;
                        else size = lua_tointeger(comp->rawFileStack, -1);
                        out.write((char*)&size, 4);
                        lua_pop(comp->rawFileStack, 1);
                        break;
                    } case CCPC_RAW_FILE_REQUEST_LIST: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "list"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        uint32_t size;
                        if (lua_pcall(comp->rawFileStack, 1, 1, 0)) size = 0xFFFFFFFF;
                        else size = lua_objlen(comp->rawFileStack, -1);
                        out.write((char*)&size, 4);
                        if (size != 0xFFFFFFFF) for (int i = 0; i < size; i++) {
                            lua_rawgeti(comp->rawFileStack, -1, i + 1);
                            out.write(lua_tostring(comp->rawFileStack, -1), lua_strlen(comp->rawFileStack, -1));
                            out.put(0);
                            lua_pop(comp->rawFileStack, 1);
                        }
                        lua_pop(comp->rawFileStack, 1);
                        break;
                    } case CCPC_RAW_FILE_REQUEST_ATTRIBUTES: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "attributes"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        if (lua_pcall(comp->rawFileStack, 1, 1, 0)) {
                            for (int i = 0; i < 22; i++) out.put(0);
                            out.put(2);
                            out.put(0);
                        } else if (lua_isnil(comp->rawFileStack, -1)) {
                            for (int i = 0; i < 22; i++) out.put(0);
                            out.put(1);
                            out.put(0);
                        } else {
                            uint32_t t32;
                            uint64_t t64;
                            lua_getfield(comp->rawFileStack, -1, "size");
                            t32 = lua_tointeger(comp->rawFileStack, -1);
                            out.write((char*)&t32, 4);
                            lua_pop(comp->rawFileStack, 1);
                            lua_getfield(comp->rawFileStack, -1, "created");
                            t64 = lua_tointeger(comp->rawFileStack, -1);
                            out.write((char*)&t64, 8);
                            lua_pop(comp->rawFileStack, 1);
                            lua_getfield(comp->rawFileStack, -1, "modified");
                            t64 = lua_tointeger(comp->rawFileStack, -1);
                            out.write((char*)&t64, 8);
                            lua_pop(comp->rawFileStack, 1);
                            lua_getfield(comp->rawFileStack, -1, "isDir");
                            out.put(lua_toboolean(comp->rawFileStack, -1));
                            lua_pop(comp->rawFileStack, 1);
                            lua_getfield(comp->rawFileStack, -1, "isReadOnly");
                            out.put(lua_toboolean(comp->rawFileStack, -1));
                            lua_pop(comp->rawFileStack, 1);
                            out.put(0); out.put(0);
                        }
                        lua_pop(comp->rawFileStack, 1);
                        break;
                    } case CCPC_RAW_FILE_REQUEST_FIND: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "find"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        uint32_t size;
                        if (lua_pcall(comp->rawFileStack, 1, 1, 0)) size = 0xFFFFFFFF;
                        else size = lua_objlen(comp->rawFileStack, -1);
                        out.write((char*)&size, 4);
                        if (size != 0xFFFFFFFF) for (int i = 0; i < size; i++) {
                            lua_rawgeti(comp->rawFileStack, -1, i + 1);
                            out.write(lua_tostring(comp->rawFileStack, -1), lua_strlen(comp->rawFileStack, -1));
                            out.put(0);
                            lua_pop(comp->rawFileStack, 1);
                        }
                        lua_pop(comp->rawFileStack, 1);
                        break;
                    } case CCPC_RAW_FILE_REQUEST_MAKEDIR: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "makeDir"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        if (lua_pcall(comp->rawFileStack, 1, 0, 0)) {
                            out.write(lua_tostring(comp->rawFileStack, -1), lua_strlen(comp->rawFileStack, -1));
                            lua_pop(comp->rawFileStack, 1);
                        }
                        out.put(0);
                        break;
                    } case CCPC_RAW_FILE_REQUEST_DELETE: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "delete"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        if (lua_pcall(comp->rawFileStack, 1, 0, 0)) {
                            out.write(lua_tostring(comp->rawFileStack, -1), lua_strlen(comp->rawFileStack, -1));
                            lua_pop(comp->rawFileStack, 1);
                        }
                        out.put(0);
                        break;
                    } case CCPC_RAW_FILE_REQUEST_COPY: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "copy"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        lua_pushstring(comp->rawFileStack, path2.c_str());
                        if (lua_pcall(comp->rawFileStack, 2, 0, 0)) {
                            out.write(lua_tostring(comp->rawFileStack, -1), lua_strlen(comp->rawFileStack, -1));
                            lua_pop(comp->rawFileStack, 1);
                        }
                        out.put(0);
                        break;
                    } case CCPC_RAW_FILE_REQUEST_MOVE: {
                        lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "move"));
                        lua_pushstring(comp->rawFileStack, path.c_str());
                        lua_pushstring(comp->rawFileStack, path2.c_str());
                        if (lua_pcall(comp->rawFileStack, 2, 0, 0)) {
                            out.write(lua_tostring(comp->rawFileStack, -1), lua_strlen(comp->rawFileStack, -1));
                            lua_pop(comp->rawFileStack, 1);
                        }
                        out.put(0);
                        break;
                    }}
                });
                break;
            } case CCPC_RAW_FILE_DATA: {
                if (!(RawTerminal::supportedFeatures & CCPC_RAW_FEATURE_FLAG_FILESYSTEM_SUPPORT)) break;
                in.get();
                uint8_t reqid = in.get();
                if (fileWriteRequests[reqid].empty()) {
                    sendRawData(CCPC_RAW_FILE_RESPONSE, id, [reqid](std::ostream &out) {
                        out.put(CCPC_RAW_FILE_REQUEST_OPEN | CCPC_RAW_FILE_REQUEST_OPEN_WRITE);
                        out.put(reqid);
                        out.write("Could not find request for given ID", 36);
                    });
                    break;
                }
                uint8_t reqtype = fileWriteRequests[reqid][0];
                std::string path = fileWriteRequests[reqid].substr(1);
                fileWriteRequests[reqid] = "";
                Computer * comp = NULL;
                LockGuard lockc(computers);
                for (Computer * c : *computers) {
                    if (checkWindowID(c, id)) {
                        comp = c;
                        break;
                    }
                }
                if (comp == NULL || comp->rawFileStack == NULL) {
                    sendRawData(CCPC_RAW_FILE_RESPONSE, id, [reqtype, reqid](std::ostream &out) {
                        out.put(reqtype);
                        out.put(reqid);
                        out.write("Could not find computer for this window", 40);
                    });
                    break;
                }
                std::lock_guard<std::mutex> lock(comp->rawFileStackMutex);
                sendRawData(CCPC_RAW_FILE_RESPONSE, id, [reqtype, reqid, comp, &path, &in](std::ostream& out) {
                    out.put(reqtype);
                    out.put(reqid);
                    lua_pushcfunction(comp->rawFileStack, findLibraryFunction(fs_lib.functions, "open"));
                    lua_pushstring(comp->rawFileStack, path.c_str());
                    lua_pushstring(comp->rawFileStack, (std::string((reqtype & CCPC_RAW_FILE_REQUEST_OPEN_APPEND) ? "a" : "w") + ((reqtype & CCPC_RAW_FILE_REQUEST_OPEN_BINARY) ? "b" : "")).c_str());
                    if (lua_pcall(comp->rawFileStack, 2, 2, 0)) {
                        out.write(lua_tostring(comp->rawFileStack, -1), lua_strlen(comp->rawFileStack, -1) + 1);
                        lua_pop(comp->rawFileStack, 1);
                    } else if (lua_isnil(comp->rawFileStack, -2)) {
                        out.write(lua_tostring(comp->rawFileStack, -1), lua_strlen(comp->rawFileStack, -1) + 1);
                        lua_pop(comp->rawFileStack, 2);
                    } else {
                        lua_pop(comp->rawFileStack, 1);
                        uint32_t size = 0;
                        in.read((char*)&size, 4);
                        char * data = new char[size];
                        in.read(data, size);
                        lua_getfield(comp->rawFileStack, -1, "write");
                        lua_pushlstring(comp->rawFileStack, data, size);
                        delete[] data;
                        lua_call(comp->rawFileStack, 1, 0);
                        lua_getfield(comp->rawFileStack, -1, "close");
                        lua_call(comp->rawFileStack, 0, 0);
                        lua_pop(comp->rawFileStack, 1);
                        out.put(0);
                    }
                });
                break;
            }}
        }
    }
}

void RawTerminal::init() {
    SDL_Init(SDL_INIT_TIMER);
    renderThread = new std::thread(termRenderLoop);
    inputThread = new std::thread(rawInputLoop);
    setThreadName(*renderThread, "Render Thread");
}

void RawTerminal::quit() {
    sendRawData(CCPC_RAW_TERMINAL_CHANGE, 0, [](std::ostream& output) {
        output.put(2);
        for (int i = 0; i < 6; i++) output.put(0);
    });
    renderThread->join();
    delete renderThread;
    inputThread->join();
    delete inputThread;
    SDL_Quit();
}

void RawTerminal::initClient(uint16_t flags, uint32_t extflags) {
    sendRawData(CCPC_RAW_FEATURE_FLAGS, 0, [flags, extflags](std::ostream& out) {
        out.write((char*)&flags, 2);
        if (flags & CCPC_RAW_FEATURE_FLAG_HAS_EXTENDED_FEATURES) out.write((char*)&extflags, 4);
    });
}

void RawTerminal::showGlobalMessage(uint32_t flags, const char * title, const char * message) {
    sendRawData(CCPC_RAW_MESSAGE_DATA, 0, [flags, title, message](std::ostream& output) {
        output.write((const char*)&flags, 4);
        output.write(title, strlen(title));
        output.put(0);
        output.write(message, strlen(message));
        output.put(0);
    });
}

RawTerminal::RawTerminal(std::string title, uint8_t cid) : Terminal(config.defaultWidth, config.defaultHeight), computerID(cid) {
    this->title.reserve(title.size());
    std::move(title.begin(), title.end(), this->title.begin());
    if (!singleWindowMode) {
        for (id = 0; currentWindowIDs.find(id) != currentWindowIDs.end(); id++) {}
        currentWindowIDs.insert(id);
    }
    if (!singleWindowMode || renderTargets.empty()) sendRawData(CCPC_RAW_TERMINAL_CHANGE, id, [this, title](std::ostream& output) {
        output.put(0);
        output.put(computerID);
        output.write((char*)&width, 2);
        output.write((char*)&height, 2);
        output.write(title.c_str(), title.size());
        output.put(0);
    });
    std::lock_guard<std::mutex> rlock(renderTargetsLock);
    renderTargets.push_back(this);
    renderTarget = --renderTargets.end();
    onActivate();
}

RawTerminal::~RawTerminal() {
    if (!singleWindowMode || renderTargets.size() == 1) sendRawData(CCPC_RAW_TERMINAL_CHANGE, id, [](std::ostream& output) {
        output.put(1);
        for (int i = 0; i < 6; i++) output.put(0);
    });
    const auto pos = currentWindowIDs.find(id);
    if (pos != currentWindowIDs.end()) currentWindowIDs.erase(pos);
    std::lock_guard<std::mutex> rtlock(renderTargetsLock);
    std::lock_guard<std::mutex> locked_g(locked);
    if (*renderTarget == this) previousRenderTarget();
    for (auto it = renderTargets.begin(); it != renderTargets.end(); ++it) {
        if (*it == this)
            it = renderTargets.erase(it);
        if (it == renderTargets.end()) break;
    }
}

void RawTerminal::render() {
    std::lock_guard<std::mutex> lock(locked);
    if (gotResizeEvent) {
        gotResizeEvent = false;
        this->screen.resize(newWidth, newHeight, ' ');
        this->colors.resize(newWidth, newHeight, 0xF0);
        this->pixels.resize(newWidth * fontWidth, newHeight * fontHeight, 0x0F);
        this->width = newWidth;
        this->height = newHeight;
        changed = true;
    }
    if (!changed) return;
    changed = false;
    sendRawData(CCPC_RAW_TERMINAL_DATA, (uint8_t)id, [this](std::ostream& output) {
        output.put((char)mode);
        output.put((char)canBlink);
        output.write((char*)&width, 2);
        output.write((char*)&height, 2);
        output.write((char*)&blinkX, 2);
        output.write((char*)&blinkY, 2);
        output.put(grayscale ? 1 : 0);
        for (int i = 0; i < 3; i++) output.put(0);
        if (mode == 0) {
            unsigned char c = screen[0][0];
            unsigned char n = 0;
            for (unsigned y = 0; y < height; y++) {
                for (unsigned x = 0; x < width; x++) {
                    if (screen[y][x] != c || n == 255) {
                        output.put(c);
                        output.put(n);
                        c = screen[y][x];
                        n = 0;
                    }
                    n++;
                }
            }
            if (n > 0) {
                output.put(c);
                output.put(n);
            }
            c = colors[0][0];
            n = 0;
            for (unsigned y = 0; y < height; y++) {
                for (unsigned x = 0; x < width; x++) {
                    if (colors[y][x] != c || n == 255) {
                        output.put(c);
                        output.put(n);
                        c = colors[y][x];
                        n = 0;
                    }
                    n++;
                }
            }
            if (n > 0) {
                output.put(c);
                output.put(n);
            }
        } else {
            unsigned char c = pixels[0][0];
            unsigned char n = 0;
            for (unsigned y = 0; y < height * 9; y++) {
                for (unsigned x = 0; x < width * 6; x++) {
                    if (pixels[y][x] != c || n == 255) {
                        output.put(c);
                        output.put(n);
                        c = pixels[y][x];
                        n = 0;
                    }
                    n++;
                }
            }
            if (n > 0) {
                output.put(c);
                output.put(n);
            }
        }
        for (int i = 0; i < (mode == 2 ? 256 : 16); i++) {
            output.put(palette[i].r);
            output.put(palette[i].g);
            output.put(palette[i].b);
        }
    });
}

void RawTerminal::showMessage(uint32_t flags, const char * title, const char * message) {
    sendRawData(CCPC_RAW_MESSAGE_DATA, (uint8_t)id, [flags, title, message](std::ostream& output) {
        output.write((const char*)&flags, 4);
        output.write(title, strlen(title));
        output.put(0);
        output.write(message, strlen(message));
        output.put(0);
    });
}

void RawTerminal::setLabel(std::string label) {
    title = label;
    sendRawData(CCPC_RAW_TERMINAL_CHANGE, (uint8_t)id, [this](std::ostream& output) {
        output.put(0);
        output.put(computerID);
        output.write((const char*)&width, 2);
        output.write((const char*)&height, 2);
        output.write(title.c_str(), title.size());
        output.put(0);
    });
}

bool RawTerminal::resize(unsigned w, unsigned h) {
    newWidth = w;
    newHeight = h;
    gotResizeEvent = (newWidth != width || newHeight != height);
    if (!gotResizeEvent) return false;
    while (gotResizeEvent) std::this_thread::yield();
    return true;
}