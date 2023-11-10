#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstdint>
#include <cstring>
#include "../src/terminal/RawTerminal.hpp"

/*
   base64.cpp and base64.h
   base64 encoding and decoding with C++.
   Version: 1.01.00
   Copyright (C) 2004-2017 Ren� Nyffenegger
   This source code is provided 'as-is', without any express or implied
   warranty. In no event will the author be held liable for any damages
   arising from the use of this software.
   Permission is granted to anyone to use this software for any purpose,
   including commercial applications, and to alter it and redistribute it
   freely, subject to the following restrictions:
   1. The origin of this source code must not be misrepresented; you must not
	  claim that you wrote the original source code. If you use this source code
	  in a product, an acknowledgment in the product documentation would be
	  appreciated but is not required.
   2. Altered source versions must be plainly marked as such, and must not be
	  misrepresented as being the original source code.
   3. This notice may not be removed or altered from any source distribution.
   Ren� Nyffenegger rene.nyffenegger@adp-gmbh.ch
*/

static const std::string base64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";


static inline bool is_base64(unsigned char c) {
	return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
	std::string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for (i = 0; (i < 4); i++)
				ret += base64_chars[char_array_4[i]];
			i = 0;
		}
	}

	if (i) {
		for (j = i; j < 3; j++)
			char_array_3[j] = '\0';

		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

		for (j = 0; (j < i + 1); j++)
			ret += base64_chars[char_array_4[j]];

		while ((i++ < 3))
			ret += '=';

	}

	return ret;

}

std::string base64_decode(std::string const& encoded_string) {
	size_t in_len = encoded_string.size();
	int i = 0;
	int j = 0;
	int in_ = 0;
	unsigned char char_array_4[4], char_array_3[3];
	std::string ret;

	while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_])) {
		char_array_4[i++] = encoded_string[in_]; in_++;
		if (i == 4) {
			for (i = 0; i < 4; i++)
				char_array_4[i] = base64_chars.find(char_array_4[i]) & 0xff;

			char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
			char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
			char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

			for (i = 0; (i < 3); i++)
				ret += char_array_3[i];
			i = 0;
		}
	}

	if (i) {
		for (j = 0; j < i; j++)
			char_array_4[j] = base64_chars.find(char_array_4[j]) & 0xff;

		char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
		char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

		for (j = 0; (j < i - 1); j++) ret += char_array_3[j];
	}

	return ret;
}

uint32_t
rc_crc32(uint32_t crc, const char *buf, size_t len) {
	static uint32_t table[256];
	static int have_table = 0;
	uint32_t rem;
	uint8_t octet;
	int i, j;
	const char *p, *q;

	/* This check is not thread safe; there is no mutex. */
	if (have_table == 0) {
		/* Calculate CRC table. */
		for (i = 0; i < 256; i++) {
			rem = i;  /* remainder from polynomial division */
			for (j = 0; j < 8; j++) {
				if (rem & 1) {
					rem >>= 1;
					rem ^= 0xedb88320;
				} else
					rem >>= 1;
			}
			table[i] = rem;
		}
		have_table = 1;
	}

	crc = ~crc;
	q = buf + len;
	for (p = buf; p < q; p++) {
		octet = *p;  /* Cast to unsigned octet. */
		crc = (crc >> 8) ^ table[(crc & 0xff) ^ octet];
	}
	return ~crc;
}

void parseIBTTag(std::istream& in, int level = 0) {
	char type = in.get();
	if (type == 0) {
		uint32_t num = 0;
		in.read((char*)&num, 4);
		std::cout << num;
	} else if (type == 1) {
		double num = 0;
		in.read((char*)&num, sizeof(double));
		std::cout << num;
	} else if (type == 2) {
		std::cout << (in.get() ? "true" : "false");
	} else if (type == 3) {
		std::string str;
		char c;
		while ((c = in.get())) str += c;
		std::cout << str;
	} else if (type == 4) {
		std::cout << "{\n";
		for (int i = 0; i < level * 2; i++) std::cout << " ";
		for (uint8_t items = in.get(); items; items--) {
			parseIBTTag(in, level+1);
			std::cout << " = ";
			parseIBTTag(in, level+1);
			std::cout << ",\n";
			for (int i = 0; i < level * 2; i++) std::cout << " ";
		}
	} else {
		std::cout << "nil";
	}
}

bool noterm = false;

static const char * fileoptypes[] = {
	"exists",
	"isDir",
	"isReadOnly",
	"getSize",
	"getDrive",
	"getCapacity",
	"getFreeSpace",
	"list",
	"attributes",
	"find",
	"makeDir",
	"delete",
	"copy",
	"move",
	"",
	"",
	"open('r')",
	"open('w')",
	"open('r') (append)",
	"open('a')",
	"open('rb')",
	"open('wb')",
	"open('r') (append)",
	"open('ab')"
};

int main(int argc, const char * argv[]) {
	if (argc > 1 && std::string(argv[1]) == "--noterm") noterm = true;
	std::cout << "Listening for data...\n";
	bool useBinaryChecksum = false;
	while (true) {
		unsigned char c = std::cin.get();
		if (c == '!' && std::cin.get() == 'C' && std::cin.get() == 'P') {
			char mode = std::cin.get();
			char size[13];
			if (mode == 'C') std::cin.read(size, 4);
			else if (mode == 'D') std::cin.read(size, 12);
			else {std::cout << "Unknown frame type '" << mode << "'!\n"; continue;}
			long sizen = strtol(size, NULL, 16);
			std::cout << "Got frame of size " << sizen << "\n";
			char * tmp = new char[sizen + 1];
			tmp[sizen] = 0;
			std::cin.read(tmp, sizen);
			std::string ddata = base64_decode(tmp);
			uint32_t sum = useBinaryChecksum ? rc_crc32(0, ddata.c_str(), ddata.size()) : rc_crc32(0, tmp, sizen);
			delete[] tmp;
			uint32_t getsum = 0;
			scanf("%08x", &getsum);
			if (sum == getsum) printf("> Checksums match (%08X)\n", getsum);
			else printf("\n> Checksums don't match! (%08X vs. expected %08X)\n", sum, getsum);
			std::stringstream in(ddata);
			uint8_t type = in.get();
			std::cout << "> Frame is of type " << (int)type << " for window ID " << (int)in.get() << "\n";
			switch (type) {
			case CCPC_RAW_TERMINAL_DATA: {
				uint16_t width = 0, height = 0, cursorX = 0, cursorY = 0;
				uint8_t mode = in.get();
				std::cout << "> Graphics mode: " << (int)mode << "\n> Cursor showing/blinking? " << (in.get() ? "Yes\n" : "No\n");
				in.read((char*)&width, 2);
				in.read((char*)&height, 2);
				std::cout << "> Width: " << width << ", height: " << height << "\n";
				in.read((char*)&cursorX, 2);
				in.read((char*)&cursorY, 2);
				std::cout << "> Cursor X: " << cursorX << ", Y: " << cursorY << "\n";
				std::cout << "> Grayscale? " << (in.get() ? "Yes\n" : "No\n");
				in.seekg((long)in.tellg() + 3);
				int i = 0;
				if (mode == 0) {
					if (!noterm) std::cout << "> Terminal contents:\n";
					while (i < width * height) {
						char c = in.get();
						int len = in.get();
						if (!noterm) {
							for (int j = 0; j < len; j++) {
								if (i + j > 0 && (i + j) % width == 0) std::cout << "\n";
								std::cout << c;
							}
						}
						i += len;
					}
					i = 0;
					while (i < width * height) {
						in.get();
						int len = in.get();
						i += len;
					}
					if (!noterm) std::cout << "\n";
				} else {
					while (i < width * height * 56) {
						in.get();
						int len = in.get();
						i += len;
					}
				}
				std::cout << "> Palette contents: ";
				uint8_t r, g, b;
				for (int i = 0; i < (mode == 2 ? 256 : 16); i++) {
					r = in.get();
					g = in.get();
					b = in.get();
					printf("%s#%02x%02x%02x", (i == 0 ? "" : ", "), r, g, b);
				}
				std::cout << "\n";
				break;
			} case CCPC_RAW_KEY_DATA: {
				std::cout << "> Key ID or character: " << (int)in.get();
				std::cout << "\n> Flags: " << std::hex << in.get() << "\n";
				break;
			} case CCPC_RAW_MOUSE_DATA: {
				std::cout << "> Mouse event type: " << (int)in.get() << "\n> Button: " << (int)in.get() << "\n";
				uint32_t x = 0, y = 0;
				in.read((char*)&x, 4);
				in.read((char*)&y, 4);
				std::cout << "> Button X: " << x << ", Y: " << y << "\n";
				break;
			} case CCPC_RAW_EVENT_DATA: {
				uint8_t paramCount = in.get();
				std::cout << "> Event parameter count: " << (int)paramCount << "\n";
				std::string str;
				char c;
				while ((c = in.get())) str += c;
				std::cout << "> Event name: " << str << "\n> Parameters:\n";
				for (int i = 0; i < paramCount; i++) {
					std::cout << "> ";
					parseIBTTag(in, 1);
					std::cout << "\n";
				}
				break;
			} case CCPC_RAW_TERMINAL_CHANGE: {
				std::cout << "> Closing window? " << (in.get() ? "Yes" : "No") << "\n";
				in.get();
				uint16_t width = 0, height = 0;
				in.read((char*)&width, 2);
				in.read((char*)&height, 2);
				std::cout << "> Width: " << width << ", height: " << height << "\n";
				std::string str;
				char c;
				while ((c = in.get())) str += c;
				std::cout << "> Title: " << str << "\n";
				break;
			} case CCPC_RAW_MESSAGE_DATA: {
				uint32_t flags = 0;
				in.read((char*)&flags, 4);
				std::cout << "> Flags: 0x" << std::hex << std::setw(8) << std::setfill('0') << flags << "\n";
				std::string str;
				char c;
				while ((c = in.get())) str += c;
				std::cout << "> Title: " << str << "\n";
				str = "";
				while ((c = in.get())) str += c;
				std::cout << "> Message: " << str << "\n";
				break;
			} case CCPC_RAW_FEATURE_FLAGS: {
				uint16_t flags = 0;
				uint32_t eflags = 0;
				in.read((char*)&flags, 2);
				if (flags & CCPC_RAW_FEATURE_FLAG_HAS_EXTENDED_FEATURES) in.read((char*)&eflags, 4);
				std::cout << "> Supported features:\n";
				if (flags & CCPC_RAW_FEATURE_FLAG_BINARY_CHECKSUM) {std::cout << "  * Binary checksums\n"; useBinaryChecksum = true;}
				if (flags & CCPC_RAW_FEATURE_FLAG_FILESYSTEM_SUPPORT) std::cout << "  * Filesystem extension\n";
				if (flags & CCPC_RAW_FEATURE_FLAG_SEND_ALL_WINDOWS) std::cout << "  * Send all windows\n";
				if (flags & CCPC_RAW_FEATURE_FLAG_HAS_EXTENDED_FEATURES) {
					std::cout << "  * Extended flags:\n";
					// if (eflags && CCPC_RAW_FEATURE_FLAG_EXTENDED_) std::cout << "    * \n";
					// none present
				}
				break;
			} case CCPC_RAW_FILE_REQUEST: {
				uint8_t reqtype = in.get();
				uint8_t reqid = in.get();
				std::cout << "> File operation type: " << fileoptypes[reqtype] << "\n> Request ID: " << (int)reqid << "\n";
				std::string path;
				char c;
				while ((c = in.get())) path += c;
				std::cout << "> Path: " << path << "\n";
				if (reqtype == CCPC_RAW_FILE_REQUEST_COPY || reqtype == CCPC_RAW_FILE_REQUEST_MOVE) {
					path = "";
					while ((c = in.get())) path += c;
					std::cout << "> Second path: " << path << "\n";
				}
				break;
			} case CCPC_RAW_FILE_RESPONSE: {
				uint8_t reqtype = in.get();
				uint8_t reqid = in.get();
				std::cout << "> File operation type: " << fileoptypes[reqtype] << "\n> Request ID: " << (int)reqid << "\n";
				std::string str;
				char c;
				uint32_t t32 = 0;
				uint64_t t64 = 0;
				switch (reqtype) {
				case CCPC_RAW_FILE_REQUEST_MAKEDIR:
				case CCPC_RAW_FILE_REQUEST_DELETE:
				case CCPC_RAW_FILE_REQUEST_COPY:
				case CCPC_RAW_FILE_REQUEST_MOVE:
				case CCPC_RAW_FILE_REQUEST_OPEN | CCPC_RAW_FILE_REQUEST_OPEN_WRITE:
				case CCPC_RAW_FILE_REQUEST_OPEN | CCPC_RAW_FILE_REQUEST_OPEN_WRITE | CCPC_RAW_FILE_REQUEST_OPEN_APPEND:
				case CCPC_RAW_FILE_REQUEST_OPEN | CCPC_RAW_FILE_REQUEST_OPEN_WRITE | CCPC_RAW_FILE_REQUEST_OPEN_BINARY:
				case CCPC_RAW_FILE_REQUEST_OPEN | CCPC_RAW_FILE_REQUEST_OPEN_WRITE | CCPC_RAW_FILE_REQUEST_OPEN_APPEND | CCPC_RAW_FILE_REQUEST_OPEN_BINARY:
					while ((c = in.get())) str += c;
					if (str.empty()) std::cout << "> Operation succeeded\n";
					else std::cout << "> Operation failed: " << str << "\n";
					break;
				case CCPC_RAW_FILE_REQUEST_EXISTS:
				case CCPC_RAW_FILE_REQUEST_ISDIR:
				case CCPC_RAW_FILE_REQUEST_ISREADONLY:
					c = in.get();
					if (c == 0) std::cout << "> Operation reported 'false'\n";
					else if (c == 1) std::cout << "> Operation reported 'true'\n";
					else std::cout << "> Operation failed\n";
					break;
				case CCPC_RAW_FILE_REQUEST_GETSIZE:
				case CCPC_RAW_FILE_REQUEST_GETCAPACITY:
				case CCPC_RAW_FILE_REQUEST_GETFREESPACE:
					in.read((char*)&t32, 4);
					if (t32 == 0xFFFFFFFF) std::cout << "> Operation failed\n";
					else std::cout << "> Requested size: " << t32 << "\n";
					break;
				case CCPC_RAW_FILE_REQUEST_LIST:
				case CCPC_RAW_FILE_REQUEST_FIND:
					in.read((char*)&t32, 4);
					if (t32 == 0xFFFFFFFF) std::cout << "> Operation failed\n";
					else {
						std::cout << "> Results:\n";
						for (int i = 0; i < t32; i++) {
							str = "";
							while ((c = in.get())) str += c;
							std::cout << "  " << str << "\n";
						}
					}
					break;
				case CCPC_RAW_FILE_REQUEST_GETDRIVE:
					while ((c = in.get())) str += c;
					if (str.empty()) std::cout << "> Operation failed\n";
					else std::cout << "> Drive path: " << str << "\n";
					break;
				case CCPC_RAW_FILE_REQUEST_ATTRIBUTES:
					in.read((char*)&t32, 4);
					std::cout << "> File size: " << t32 << "\n";
					in.read((char*)&t64, 8);
					t64 /= 1000;
					std::cout << "> Creation date: " << ctime((time_t*)&t64) << "\n";
					in.read((char*)&t64, 8);
					t64 /= 1000;
					std::cout << "> Modification date: " << ctime((time_t*)&t64) << "\n";
					c = in.get();
					std::cout << "> Is " << (c ? "" : "not ") << "a directory\n";
					c = in.get();
					std::cout << "> Is " << (c ? "" : "not ") << "read-only\n";
					c = in.get();
					if (c == 1) std::cout << "> Path does not exist\n";
					else if (c == 2) std::cout << "> Operation failed\n";
					break;
				}
				break;
			} case CCPC_RAW_FILE_DATA: {
				uint8_t err = in.get();
				uint8_t reqid = in.get();
				std::cout << "> Request ID: " << (int)reqid << "\n";
				uint32_t size = 0;
				in.read((char*)&size, 4);
				char * data = new char[size];
				in.read(data, size);
				std::string str(data, size);
				delete[] data;
				if (err) std::cout << "> Operation failed: " << str << "\n";
				else if (noterm) std::cout << "> Data size: " << size << "\n";
				else std::cout << "> Data:\n" << str << "\n";
				break;
			}}
		}
	}
}