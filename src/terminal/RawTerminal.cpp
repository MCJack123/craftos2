/*
 * RawTerminal.cpp
 * CraftOS-PC 2
 * 
 * This file implements the RawTerminal class.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include "RawTerminal.hpp"
#include "SDLTerminal.hpp"
#include "../lib.hpp"
#include "../term.hpp"
#include "../peripheral/monitor.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <Poco/Checksum.h>

/* Data spec
  
* Common Header

  Offset     Bytes      Purpose
  0x00       4          Header ("!CPC")
  0x04       4          Size (hex string)
  ===================== Base64 payload
  0x00       1          Frame type ID
  0x01       1          ID

* Type 0: Terminal contents (server -> client)

  Offset     Bytes      Purpose
  0x02       1          Graphics mode
  0x03       1          Cursor showing?
  0x04       2          Width
  0x06       2          Height
  0x08       2          Cursor X
  0x0A       2          Cursor Y
  0x0C       4          Reserved
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
  0x03       1          Reserved
  0x04       2          Width
  0x06       2          Height
  0x08       *x*        Title (NUL-terminated) (ignored when sending client -> server)

* Type 5: Show message (server -> client)
  
  Offset     Bytes      Purpose
  0x02       4          Flags from SDL message event
  0x06       *x*        Title (NUL-terminated)
  0x06+x     *y*        Message (NUL-terminated)

* Common Footer

  ===================== End Base64 payload
  END-4      8          CRC32 of payload (hex string)
  END        1          Newline

*/

extern bool rawClient;
extern bool exiting;
extern std::thread * renderThread;
extern void termRenderLoop();
std::set<unsigned> RawTerminal::currentIDs;
std::thread * inputThread;

enum {
	CCPC_RAW_TERMINAL_DATA = 0,
	CCPC_RAW_KEY_DATA,
	CCPC_RAW_MOUSE_DATA,
	CCPC_RAW_EVENT_DATA,
	CCPC_RAW_TERMINAL_CHANGE,
	CCPC_RAW_MESSAGE_DATA
};

void sendRawData(uint8_t type, uint8_t id, std::function<void(std::ostream&)> callback) {
	std::stringstream output;
	output.put(type);
	output.put(id);
	callback(output);
	std::string str = b64encode(output.str());
	str.erase(std::remove_if(str.begin(), str.end(), [](char c)->bool {return c == '\n' || c == '\r'; }), str.end());
	Poco::Checksum chk;
	chk.update(str);
	uint32_t sum = chk.checksum();
	std::cout << "!CPC" << std::hex << std::setfill('0') << std::setw(4) << str.length() << std::dec;
	std::cout << str << std::hex << std::setfill('0') << std::setw(8) << sum << "\n";
    std::cout.flush();
}

void parseIBTTag(std::istream& in, lua_State *L) {
	char type = in.get();
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
		while ((c = in.get())) str += c;
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

extern int selectedRenderer;
extern std::unordered_map<int, unsigned char> keymap;
extern std::map<uint8_t, Terminal*> rawClientTerminals;
extern std::unordered_map<unsigned, uint8_t> rawClientTerminalIDs;
extern int buttonConvert(Uint8 button);
extern int buttonConvert2(Uint32 state);
extern int convertX(SDLTerminal *term, int x);
extern int convertY(SDLTerminal *term, int y);

void sendRawEvent(SDL_Event e) {
	if ((e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) && (selectedRenderer != 0 || keymap.find(e.key.keysym.scancode) != keymap.end())) 
		sendRawData(CCPC_RAW_KEY_DATA, rawClientTerminalIDs[e.key.windowID], [e](std::ostream& output) {
			if (selectedRenderer == 0) output.put(keymap.at(e.key.keysym.scancode));
			else output.put(e.key.keysym.scancode);
			output.put((e.type == SDL_KEYUP) | ((0) << 1) | (((e.key.keysym.mod & KMOD_CTRL) != 0) << 2) | ((0) << 3));
		});
	else if (e.type == SDL_TEXTINPUT)
		sendRawData(CCPC_RAW_KEY_DATA, rawClientTerminalIDs[e.text.windowID], [e](std::ostream& output) {
			output.put(e.text.text[0]);
			output.put((0) | ((0) << 1) | ((0) << 2) | ((1) << 3));
		});
	else if ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) && rawClientTerminals.find(e.button.windowID) != rawClientTerminals.end())
		sendRawData(CCPC_RAW_MOUSE_DATA, rawClientTerminalIDs[e.button.windowID], [e](std::ostream &output) {
			output.put(e.type == SDL_MOUSEBUTTONUP);
			output.put(buttonConvert(e.button.button));
			SDLTerminal * sdlterm = dynamic_cast<SDLTerminal*>(rawClientTerminals[e.button.windowID]);
			uint16_t x, y;
			if (sdlterm != NULL) {
				x = convertX(sdlterm, e.button.x);
				y = convertY(sdlterm, e.button.y);
			} else {
				x = e.button.x;
				y = e.button.y;
			}
			output.write((char*)&x, 2);
			output.write((char*)&y, 2);
		});
	else if (e.type == SDL_MOUSEWHEEL && rawClientTerminals.find(e.button.windowID) != rawClientTerminals.end() && selectedRenderer == 0)
		sendRawData(CCPC_RAW_MOUSE_DATA, rawClientTerminalIDs[e.wheel.windowID], [e](std::ostream &output) {
			output.put(2);
			output.put(max(min(e.wheel.y * (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? 1 : -1), 1), -1));
			SDLTerminal * sdlterm = dynamic_cast<SDLTerminal*>(rawClientTerminals[e.button.windowID]);
			uint16_t x, y;
			if (sdlterm != NULL) {
				int tx = 0, ty = 0;
				sdlterm->getMouse(&tx, &ty);
				x = convertX(sdlterm, tx);
				y = convertY(sdlterm, ty);
			} else {
				// ???
			}
			output.write((char*)&x, 2);
			output.write((char*)&y, 2);
		});
	else if (e.type == SDL_MOUSEMOTION && e.motion.state && rawClientTerminals.find(e.button.windowID) != rawClientTerminals.end())
		sendRawData(CCPC_RAW_MOUSE_DATA, rawClientTerminalIDs[e.motion.windowID], [e](std::ostream &output) {
			output.put(3);
			output.put(buttonConvert2(e.motion.state));
			SDLTerminal * sdlterm = dynamic_cast<SDLTerminal*>(rawClientTerminals[e.button.windowID]);
			uint16_t x, y;
			if (sdlterm != NULL) {
				x = convertX(sdlterm, e.button.x);
				y = convertY(sdlterm, e.button.y);
			} else {
				x = e.button.x;
				y = e.button.y;
			}
			output.write((char*)&x, 2);
			output.write((char*)&y, 2);
		});
	else if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE)
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

extern monitor * findMonitorFromWindowID(Computer *comp, unsigned id, std::string& sideReturn);

#ifdef __EMSCRIPTEN__
#define checkWindowID(c, wid) (c->term == *TerminalWindow::renderTarget || findMonitorFromWindowID(c, (*TerminalWindow::renderTarget)->id, tmps) != NULL)
#else
#define checkWindowID(c, wid) (wid == c->term->id || findMonitorFromWindowID(c, wid, tmps) != NULL)
#endif

struct rawMouseProviderData {
	uint8_t evtype;
	uint8_t button;
	uint32_t x;
	uint32_t y;
};

const char * rawMouseProvider(lua_State *L, void* data) {
	struct rawMouseProviderData * d = (struct rawMouseProviderData*)data;
	lua_pushinteger(L, d->button);
	lua_pushinteger(L, d->x);
	lua_pushinteger(L, d->y);
	const char * retval = NULL;
	if (d->evtype == 0) retval = "mouse_click";
	else if (d->evtype == 1) retval = "mouse_up";
	else if (d->evtype == 2) retval = "mouse_scroll";
	else if (d->evtype == 3) retval = "mouse_drag";
	delete d;
	return retval;
}

const char * rawEventProvider(lua_State *L, void* data) {
	std::stringstream& in = *(std::stringstream*)data;
	uint8_t paramCount = in.get();
	char c;
	std::string name;
	while ((c = in.get())) name += c;
	for (int i = 0; i < paramCount; i++) parseIBTTag(in, L);
	delete (std::stringstream*)data;
	return name.c_str();
}

void rawInputLoop() {
	while (!exiting) {
		unsigned char c = std::cin.get();
		if (c == '!' && std::cin.get() == 'C' && std::cin.get() == 'P' && std::cin.get() == 'C') {
			char size[5];
			std::cin.read(size, 4);
			long sizen = strtol(size, NULL, 16);
			char * tmp = new char[sizen + 1];
			tmp[sizen] = 0;
			std::cin.read(tmp, sizen);
			Poco::Checksum chk;
			chk.update(tmp, sizen);
			char hexstr[9];
			std::cin.read(hexstr, 8);
			hexstr[8] = 0;
			if (chk.checksum() != strtoul(hexstr, NULL, 16)) {
				fprintf(stderr, "Invalid checksum: expected %08X, got %08lX\n", chk.checksum(), strtoul(hexstr, NULL, 16));
				continue;
			}
			std::stringstream in(b64decode(tmp));
			delete[] tmp;

			SDL_Event e;
			memset(&e, 0, sizeof(SDL_Event));
			std::string tmps;
			uint8_t type = in.get();
			uint8_t id = in.get();
			if (type == CCPC_RAW_KEY_DATA) {
				uint8_t key = in.get();
				uint8_t flags = in.get();
				if (flags & 8) {
					e.type = SDL_TEXTINPUT;
					e.text.text[0] = key;
					e.text.text[1] = '\0';
					for (Computer * c : computers) {
						if (checkWindowID(c, e.key.windowID)) {
							e.text.windowID = c->term->id;
							c->termEventQueue.push(e);
							c->event_lock.notify_all();
						}
					}
				} else if ((flags & 9) == 1) {
					e.type = SDL_KEYUP;
					e.key.keysym.scancode = (SDL_Scancode)key;
					if (flags & 4) e.key.keysym.mod = KMOD_CTRL;
					for (Computer * c : computers) {
						if (checkWindowID(c, e.key.windowID)) {
							e.key.windowID = c->term->id;
							c->termEventQueue.push(e);
							c->event_lock.notify_all();
						}
					}
				} else {
					e.type = SDL_KEYDOWN;
					e.key.keysym.scancode = (SDL_Scancode)key;
					if (flags & 4) e.key.keysym.mod = KMOD_CTRL;
					for (Computer * c : computers) {
						if (checkWindowID(c, e.key.windowID)) {
							e.key.windowID = c->term->id;
							c->termEventQueue.push(e);
							c->event_lock.notify_all();
						}
					}
				}
			} else if (type == CCPC_RAW_MOUSE_DATA) {
				uint8_t evtype = in.get();
				uint8_t button = in.get();
				uint32_t x = 0, y = 0;
				in.read((char*)&x, 4);
				in.read((char*)&y, 4);
				for (Computer * c : computers) {
					if (checkWindowID(c, id)) {
						struct rawMouseProviderData * d = new struct rawMouseProviderData;
						d->evtype = evtype;
						d->button = button;
						d->x = x;
						d->y = y;
						termQueueProvider(c, rawMouseProvider, d);
					}
				}
			} else if (type == CCPC_RAW_EVENT_DATA) {
				for (Computer * c : computers) {
					if (checkWindowID(c, id)) {
						std::stringstream * ss = new std::stringstream(in.str().substr(2));
						termQueueProvider(c, rawEventProvider, ss);
					}
				}
			} else if (type == CCPC_RAW_TERMINAL_CHANGE) {
				int isClosing = in.get();
				if (isClosing == 1) {
					e.type = SDL_WINDOWEVENT;
					e.window.event = SDL_WINDOWEVENT_CLOSE;
					for (Computer * c : computers) {
						if (checkWindowID(c, e.window.windowID)) {
							e.window.windowID = c->term->id;
							c->termEventQueue.push(e);
							c->event_lock.notify_all();
						}
					}
				} else if (isClosing == 2) {
					e.type = SDL_QUIT;
					for (Computer * c : computers) {
						c->termEventQueue.push(e);
						c->event_lock.notify_all();
					}
				} else {
					in.get(); // reserved
					uint16_t w = 0, h = 0;
					in.read((char*)&w, 2);
					in.read((char*)&h, 2);
					e.type = SDL_WINDOWEVENT;
					e.window.event = SDL_WINDOWEVENT_RESIZED;
					e.window.data1 = w;
					e.window.data2 = h;
					for (Computer * c : computers) {
						if (checkWindowID(c, e.window.windowID)) {
							c->termEventQueue.push(e);
							c->event_lock.notify_all();
						}
					}
				}
			}
		}
	}
}

void RawTerminal::init() {
    SDL_Init(SDL_INIT_TIMER);
    renderThread = new std::thread(termRenderLoop);
	inputThread = new std::thread(rawInputLoop);
}

void RawTerminal::quit() {
	sendRawData(CCPC_RAW_TERMINAL_CHANGE, 0, [](std::ostream& output) {
		output.put(2);
		for (int i = 0; i < 6; i++) output.put(0);
	});
    renderThread->join();
    delete renderThread;
    SDL_Quit();
}

RawTerminal::RawTerminal(std::string title) : Terminal(51, 19) {
	this->title = title;
	for (id = 0; currentIDs.find(id) != currentIDs.end(); id++);
	sendRawData(CCPC_RAW_TERMINAL_CHANGE, id, [this](std::ostream& output) {
		output.put(0);
		output.put(0);
		output.write((char*)&width, 2);
		output.write((char*)&height, 2);
		output.write(this->title.c_str(), strlen(this->title.c_str()));
		output.put(0);
	});
    renderTargets.push_back(this);
}

RawTerminal::~RawTerminal() {
	sendRawData(CCPC_RAW_TERMINAL_CHANGE, id, [](std::ostream& output) {
		output.put(1);
		for (int i = 0; i < 6; i++) output.put(0);
	});
	auto pos = currentIDs.find(id);
	if (pos != currentIDs.end()) currentIDs.erase(pos);
    Terminal::renderTargetsLock.lock();
    std::lock_guard<std::mutex> locked_g(locked);
    for (auto it = renderTargets.begin(); it != renderTargets.end(); it++) {
        if (*it == this)
            it = renderTargets.erase(it);
        if (it == renderTargets.end()) break;
    }
    Terminal::renderTargetsLock.unlock();
}

void RawTerminal::render() {
    if (!changed) return;
    changed = false;
	sendRawData(CCPC_RAW_TERMINAL_DATA, (uint8_t)id, [this](std::ostream& output) {
		output.put((char)mode);
		output.put((char)blink);
		output.write((char*)&width, 2);
		output.write((char*)&height, 2);
		output.write((char*)&blinkX, 2);
		output.write((char*)&blinkY, 2);
        for (int i = 0; i < 4; i++) output.put(0);
		if (mode == 0) {
			unsigned char c = screen[0][0];
			unsigned char n = 0;
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
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
			for (int y = 0; y < height; y++) {
				for (int x = 0; x < width; x++) {
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
			for (int y = 0; y < height * 9; y++) {
				for (int x = 0; x < width * 6; x++) {
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
		output.write((char*)&flags, 4);
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
		output.put(0);
		output.write((char*)&width, 2);
		output.write((char*)&height, 2);
		output.write(title.c_str(), strlen(title.c_str()));
		output.put(0);
	});
}

bool RawTerminal::resize(int w, int h) {
    newWidth = w;
    newHeight = h;
    gotResizeEvent = (newWidth != width || newHeight != height);
    if (!gotResizeEvent) return false;
    while (gotResizeEvent) std::this_thread::yield();
    return true;
}