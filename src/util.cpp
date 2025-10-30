/*
 * util.cpp
 * CraftOS-PC 2
 * 
 * This file implements some commonly-used functions.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#include <atomic>
#include <sstream>
#include <Computer.hpp>
#include <dirent.h>
#include <Poco/Base64Decoder.h>
#include <Poco/Base64Encoder.h>
#include <sys/stat.h>
#include <FileEntry.hpp>
#include "platform.hpp"
#include "runtime.hpp"
#include "terminal/SDLTerminal.hpp"
#include "util.hpp"
#ifndef WIN32
#include <libgen.h>
#endif

#ifdef STANDALONE_ROM
extern FileEntry standaloneROM;
extern FileEntry standaloneDebug;
extern std::string standaloneBIOS;
#endif

const char * lastCFunction = "(none!)";
char computer_key = 'C';
static ProtectedObject<std::unordered_map<void*, Computer*> > getCompCache;

const wchar_t charsetConversion[256] = {
    // lower CP437 characters
    0x0000, 0x263A, 0x263B, 0x2665, 0x2666, 0x2663, 0x2660, 0x25CF, 0x25CB, 0x0009, 0x000A, 0x2642, 0x2640, 0x000D, 0x266A, 0x266C,
    0x25B6, 0x25C0, 0x2195, 0x203C, 0x00B6, 0x00A7, 0x25AC, 0x21A8, 0x2B06, 0x2B07, 0x27A1, 0x2B05, 0x221F, 0x29FA, 0x25B2, 0x25BC,
    // ASCII characters
    0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002C, 0x002D, 0x002E, 0x002F,
    0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003A, 0x003B, 0x003C, 0x003D, 0x003E, 0x003F,
    0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004A, 0x004B, 0x004C, 0x004D, 0x004E, 0x004F,
    0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005A, 0x005B, 0x005C, 0x005D, 0x005E, 0x005F,
    0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006A, 0x006B, 0x006C, 0x006D, 0x006E, 0x006F,
    0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007A, 0x007B, 0x007C, 0x007D, 0x007E, 0x1FB99,
    // drawing characters
    0x0020, 0x1FB00, 0x1FB01, 0x1FB02, 0x1FB03, 0x1FB04, 0x1FB05, 0x1FB06, 0x1FB07, 0x1FB08, 0x1FB09, 0x1FB0A, 0x1FB0B, 0x1FB0C, 0x1FB0D, 0x1FB0E,
    0x1FB0F, 0x1FB10, 0x1FB11, 0x1FB12, 0x1FB13, 0x258C, 0x1FB14, 0x1FB15, 0x1FB16, 0x1FB17, 0x1FB18, 0x1FB19, 0x1FB1A, 0x1FB1B, 0x1FB1C, 0x1FB1D,
    // ISO-8859-1 characters
    0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7, 0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
    0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7, 0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
    0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7, 0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
    0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7, 0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7, 0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
    0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7, 0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF
};

Computer * get_comp(lua_State *L) {
    try {
        return getCompCache->at(L->l_G);
    } catch (std::out_of_range &e) {
        LockGuard lock(getCompCache);
        lua_rawgeti(L, LUA_REGISTRYINDEX, 1);
        Computer * retval = (Computer*)lua_touserdata(L, -1);
        lua_pop(L, 1);
        getCompCache->insert(std::make_pair(L->l_G, retval));
        return retval;
    }
}

void uncache_state(lua_State *L) {
    LockGuard lock(getCompCache);
    getCompCache->erase(L->l_G);
}

void load_library(Computer *comp, lua_State *L, const library_t& lib) {
    lua_newtable(L);
    luaL_setfuncs(L, lib.functions, 0);
    lua_setglobal(L, lib.name);
    if (lib.init != NULL) lib.init(comp);
}

std::string b64encode(const std::string& orig) {
    std::stringstream ss;
    Poco::Base64Encoder enc(ss);
    enc.write(orig.c_str(), orig.size());
    enc.close();
    return ss.str();
}

std::string b64decode(const std::string& orig) {
    std::stringstream ss;
    std::stringstream out(orig);
    Poco::Base64Decoder dec(out);
    std::copy(std::istreambuf_iterator<char>(dec), std::istreambuf_iterator<char>(), std::ostreambuf_iterator<char>(ss));
    return ss.str();
}

std::vector<std::string> split(const std::string& strToSplit, const char * delims) {
    std::vector<std::string> retval;
    size_t pos = strToSplit.find_first_not_of(delims);
    while (pos != std::string::npos) {
        const size_t end = strToSplit.find_first_of(delims, pos);
        retval.push_back(strToSplit.substr(pos, end - pos));
        pos = strToSplit.find_first_not_of(delims, end);
    }
    return retval;
}

std::vector<std::wstring> split(const std::wstring& strToSplit, const wchar_t * delims) {
    std::vector<std::wstring> retval;
    size_t pos = strToSplit.find_first_not_of(delims);
    while (pos != std::string::npos) {
        const size_t end = strToSplit.find_first_of(delims, pos);
        retval.push_back(strToSplit.substr(pos, end - pos));
        pos = strToSplit.find_first_not_of(delims, end);
    }
    return retval;
}

std::vector<path_t> split(const path_t& strToSplit, const path_t::value_type * delims) {
    std::vector<path_t> retval;
    path_t::string_type str = strToSplit.native();
    size_t pos = str.find_first_not_of(delims);
    while (pos != std::string::npos) {
        const size_t end = str.find_first_of(delims, pos);
        retval.push_back(str.substr(pos, end - pos));
        pos = str.find_first_not_of(delims, end);
    }
    return retval;
}

static std::string concat(const std::list<std::string> &c, char sep) {
    std::stringstream ss;
    bool started = false;
    for (const std::string& s : c) {
        if (started) ss << sep;
        ss << s;
        started = true;
    }
    return ss.str();
}

static std::list<std::string> split_list(const std::string& strToSplit, const char * delims) {
    std::list<std::string> retval;
    size_t pos = strToSplit.find_first_not_of(delims);
    while (pos != std::string::npos) {
        size_t end = strToSplit.find_first_of(delims, pos);
        retval.push_back(strToSplit.substr(pos, end - pos));
        pos = strToSplit.find_first_not_of(delims, end);
    }
    return retval;
}

path_t fixpath_mkdir(Computer * comp, const std::string& path, bool md, std::string * mountPath) {
    if (md && fixpath_ro(comp, path)) return path_t();
    path_t firstTest = fixpath(comp, path, true, true, mountPath);
    if (!firstTest.empty()) return firstTest;
    std::list<std::string> components = split_list(path, "/\\");
    while (!components.empty() && components.front().empty()) components.pop_front();
    if (components.empty()) return fixpath(comp, "", true);
    components.pop_back();
    std::list<std::string> append;
    path_t maxPath = fixpath(comp, concat(components, '/'), false, true, mountPath);
    while (maxPath.empty()) {
        append.push_front(components.back());
        components.pop_back();
        if (components.empty()) return path_t();
        maxPath = fixpath(comp, concat(components, '/'), false, true, mountPath);
    }
    if (!md) return maxPath;
    for (const std::string& s : append) maxPath /= s;
    std::error_code e;
    fs::create_directories(maxPath, e);
    if (e) return path_t();
    return fixpath(comp, path, false, true, mountPath);
}

static bool _nothrow(std::function<void()> f) { try { f(); return true; } catch (...) { return false; } }
#define nothrow(expr) _nothrow([&](){ expr ;})

inline bool isVFSPath(path_t path) {
    if (!std::isdigit(path.native()[0])) return false;
    for (const path_t::value_type& c : path.native()) {
        if (c == ':') return true;
        else if (!std::isdigit(c)) return false;
    }
    return false;
}

path_t fixpath(Computer *comp, std::string path, bool exists, bool addExt, std::string * mountPath, bool * isRoot) {
    path.erase(std::remove_if(path.begin(), path.end(), [](char c)->bool {return c == '"' || c == '*' || c == ':' || c == '<' || c == '>' || c == '?' || c == '|' || c < 32; }), path.end());
    std::vector<std::string> elems = split(path, "/\\");
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") {
            if (pathc.empty() && addExt) return path_t();
            else if (pathc.empty()) pathc.push_back("..");
            else pathc.pop_back();
        } else if (!s.empty() && s.find_first_not_of(' ') != std::string::npos && !std::all_of(s.begin(), s.end(), [](const char c)->bool{return c == '.';})) {
            s = s.substr(s.find_first_not_of(' '), s.find_last_not_of(' ') - s.find_first_not_of(' ') + 1);
            pathc.push_back(s);
        }
    }
    while (!pathc.empty() && pathc.front().empty()) pathc.pop_front();
    if (!pathc.empty() && pathc.back().size() > 255) {
        std::string s = pathc.back().substr(0, 255);
        pathc.pop_back();
        s = s.substr(0, s.find_last_not_of(' '));
        pathc.push_back(s);
    }
    if (comp->isDebugger && addExt && pathc.size() == 1 && pathc.front() == "bios.lua")
#ifdef STANDALONE_ROM
        return path_t(":bios.lua", path_t::format::generic_format);
#else
        return getROMPath()/"bios.lua";
#endif
    path_t ss;
    std::error_code e;
    if (addExt) {
        std::pair<size_t, std::vector<_path_t> > max_path = std::make_pair(0, std::vector<_path_t>(1, comp->dataDir));
        std::list<std::string> * mount_list = NULL;
        for (auto& m : comp->mounts) {
            std::list<std::string> &pathlist = std::get<0>(m);
            if (pathc.size() >= pathlist.size() && std::equal(pathlist.begin(), pathlist.end(), pathc.begin())) {
                if (pathlist.size() > max_path.first) {
                    max_path = std::make_pair(pathlist.size(), std::vector<_path_t>(1, std::get<1>(m)));
                    mount_list = &pathlist;
                } else if (pathlist.size() == max_path.first) {
                    max_path.second.push_back(std::get<1>(m));
                }
            }
        }
        for (size_t i = 0; i < max_path.first; i++) pathc.pop_front();
        if (isRoot != NULL) *isRoot = pathc.empty();
        if (exists) {
            bool found = false;
            for (const _path_t& p : max_path.second) {
                path_t sstmp = p;
                for (const std::string& s : pathc) sstmp /= s;
                e.clear();
                if ((isVFSPath(p) && nothrow(comp->virtualMounts[(unsigned)std::stoul(p.substr(0, p.size()-1))]->path(sstmp))) || (fs::exists(sstmp, e))) {
                    ss /= sstmp;
                    found = true;
                    break;
                }
            }
            if (!found) return path_t();
        } else if (pathc.size() > 1) {
            bool found = false;
            std::stack<std::string> oldback;
            while (!found && !pathc.empty()) {
                found = false;
                std::string back = pathc.back();
                pathc.pop_back();
                for (const _path_t& p : max_path.second) {
                    path_t sstmp = p;
                    for (const std::string& s : pathc) sstmp /= s;
                    e.clear();
                    if (
                        (isVFSPath(p) && (nothrow(comp->virtualMounts[(unsigned)std::stoul(p.substr(0, p.size()-1))]->path(ss/back)) ||
                        (nothrow(comp->virtualMounts[(unsigned)std::stoul(p.substr(0, p.size()-1))]->path(sstmp)) && comp->virtualMounts[(unsigned)std::stoul(p.substr(0, p.size()-1))]->path(sstmp).isDir))) ||
                        (fs::exists(sstmp/back, e)) || (fs::is_directory(sstmp, e))) {
                        ss /= sstmp/back;
                        while (!oldback.empty()) {
                            ss /= oldback.top();
                            oldback.pop();
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) oldback.push(back);
            }
            if (!found) return path_t();
        } else {
            ss /= max_path.second.front();
            for (const std::string& s : pathc) ss /= s;
        }
        if (mountPath != NULL) {
            if (mount_list == NULL) *mountPath = "hdd";
            else {
                std::stringstream ss2;
                for (auto it = mount_list->begin(); it != mount_list->end(); ++it) {
                    if (it != mount_list->begin()) ss2 << "/";
                    ss2 << *it;
                }
                *mountPath = ss2.str();
            }
        }
    } else for (const std::string& s : pathc) ss /= s;
    if (path_t::preferred_separator != (path_t::value_type)'/' && (!addExt || isVFSPath(ss))) {
        path_t::string_type str = ss.native();
        std::replace(str.begin(), str.end(), path_t::preferred_separator, (path_t::value_type)'/');
        ss = path_t(str);
    }
    return ss;
}

bool fixpath_ro(Computer *comp, std::string path) {
    path.erase(std::remove_if(path.begin(), path.end(), [](char c)->bool {return c == '"' || c == '*' || c == ':' || c == '<' || c == '>' || c == '?' || c == '|' || c < 32; }), path.end());
    std::vector<std::string> elems = split(path, "/\\");
    std::list<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") { if (pathc.empty()) return false; else pathc.pop_back(); }
        else if (!s.empty() && !std::all_of(s.begin(), s.end(), [](const char c)->bool{return c == '.';})) {
            s = s.substr(s.find_first_not_of(' '), s.find_last_not_of(' ') - s.find_first_not_of(' ') + 1);
            pathc.push_back(s);
        }
    }
    while (!pathc.empty() && pathc.front().empty()) pathc.pop_front();
    if (!pathc.empty() && pathc.back().size() > 255) {
        std::string s = pathc.back().substr(0, 255);
        pathc.pop_back();
        s = s.substr(0, s.find_last_not_of(' '));
        pathc.push_back(s);
    }
    std::pair<size_t, bool> max_path = std::make_pair(0, false);
    for (const auto& m : comp->mounts)
        if (pathc.size() >= std::get<0>(m).size() && std::get<0>(m).size() > max_path.first && std::equal(std::get<0>(m).begin(), std::get<0>(m).end(), pathc.begin()))
            max_path = std::make_pair(std::get<0>(m).size(), std::get<2>(m));
    return max_path.second;
}

std::set<std::string> getMounts(Computer * computer, std::string comp_path) {
    comp_path.erase(std::remove_if(comp_path.begin(), comp_path.end(), [](char c)->bool {return c == '"' || c == '*' || c == ':' || c == '<' || c == '>' || c == '?' || c == '|' || c < 32; }), comp_path.end());
    std::vector<std::string> elems = split(comp_path, "/\\");
    std::list<std::string> pathc;
    std::set<std::string> retval;
    for (std::string s : elems) {
        if (s == "..") { if (pathc.empty()) return retval; else pathc.pop_back(); }
        else if (!s.empty() && !std::all_of(s.begin(), s.end(), [](const char c)->bool{return c == '.';})) {
            pathc.push_back(s);
        }
    }
    for (const auto& m : computer->mounts)
        if (pathc.size() + 1 == std::get<0>(m).size() && std::equal(pathc.begin(), pathc.end(), std::get<0>(m).begin()))
            retval.insert(std::get<0>(m).back());
    return retval;
}

static void xcopy_internal(lua_State *from, lua_State *to, int n, int copies_slot) {
    for (int i = n - 1; i >= 0; i--) {
        size_t sz = 0;
        switch (lua_type(from, -1-i)) {
            case LUA_TNIL: case LUA_TNONE: lua_pushnil(to); break;
            case LUA_TBOOLEAN: lua_pushboolean(to, lua_toboolean(from, -1-i)); break;
            case LUA_TNUMBER: lua_pushnumber(to, lua_tonumber(from, -1-i)); break;
            case LUA_TSTRING: {
                const char * str = lua_tolstring(from, -1-i, &sz);
                lua_pushlstring(to, str, sz); break;
            } case LUA_TTABLE: {
                const void* ptr = lua_topointer(from, -1-i);
                lua_rawgeti(to, copies_slot, (ptrdiff_t)ptr);
                if (!lua_isnil(to, -1)) continue;
                lua_pop(to, 1);
                lua_newtable(to);
                lua_pushvalue(to, -1);
                lua_rawseti(to, copies_slot, (ptrdiff_t)ptr);
                lua_pushnil(from);
                while (lua_next(from, -2-i) != 0) {
                    xcopy_internal(from, to, 2, copies_slot);
                    lua_settable(to, -3);
                    lua_pop(from, 1);
                }
                break;
            }
            default: {
                if (luaL_callmeta(from, -1-i, "__tostring")) {
                    lua_pushlstring(to, lua_tostring(from, -1), lua_rawlen(from, -1));
                    lua_pop(from, 1);
                } else lua_pushfstring(to, "<%s: %p>", lua_typename(from, lua_type(from, -1-i)), lua_topointer(from, -1-i));
                break;
            }
        }
    }
}

void xcopy(lua_State *from, lua_State *to, int n) {
    lua_newtable(to);
    int cslot = lua_gettop(to);
    xcopy_internal(from, to, n, cslot);
    lua_remove(to, cslot);
}

// Deprecated as of CCPC v2.8; text mode no longer exists
std::string makeASCIISafe(const char * retval, size_t len) {
    return std::string(retval, len);
}

struct IPv6 {uint16_t a, b, c, d, e, f, g, h;};

static constexpr uint32_t makeIP(int a, int b, int c, int d) {return (a << 24) | (b << 16) | (c << 8) | d;}

static std::vector<std::pair<uint32_t, uint8_t> > reservedIPv4s = {
    {makeIP(0, 0, 0, 0), 32},
    {makeIP(10, 0, 0, 0), 8},
    {makeIP(100, 64, 0, 0), 10},
    {makeIP(127, 0, 0, 0), 8},
    {makeIP(169, 254, 0, 0), 16},
    {makeIP(172, 16, 0, 0), 12},
    {makeIP(192, 0, 0, 0), 24},
    {makeIP(192, 0, 2, 0), 24},
    {makeIP(192, 168, 0, 0), 16},
    {makeIP(198, 18, 0, 0), 15},
    {makeIP(224, 0, 0, 0), 4},
    {makeIP(255, 255, 255, 255), 32}
};

static std::vector<std::pair<IPv6, uint8_t> > reservedIPv6s = {
    {{0, 0, 0, 0, 0, 0, 0, 0}, 128},
    {{0, 0, 0, 0, 0, 0, 0, 1}, 128},
    {{0xfc00, 0, 0, 0, 0, 0, 0, 0}, 7},
    {{0xfd00, 0, 0, 0, 0, 0, 0, 0}, 8},
    {{0xfe80, 0, 0, 0, 0, 0, 0, 0}, 10},
    {{0xfec0, 0, 0, 0, 0, 0, 0, 0}, 10},
    {{0xff00, 0, 0, 0, 0, 0, 0, 0}, 8}
};

static std::atomic_bool didAddIPv4IPs(false);

bool matchIPClass(const std::string& address, const std::string& pattern) {
    static const std::regex ipv4_regex("(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)");
    static const std::regex ipv6_regex("");
    static const std::regex ipv4_class_regex("(\\d+)\\.(\\d+)\\.(\\d+)\\.(\\d+)/(\\d+)");
    static const std::regex regex_escape("[\\^\\$\\\\\\.\\+\\?\\(\\)\\[\\]\\{\\}\\|]");
    static const std::regex regex_wildcard("\\*");
    if (!didAddIPv4IPs.load()) {
        didAddIPv4IPs.store(true);
        reservedIPv6s.reserve(reservedIPv6s.size() + reservedIPv4s.size());
        for (const auto& cl : reservedIPv4s)
            reservedIPv6s.push_back(std::make_pair<IPv6, uint8_t>({0, 0, 0, 0, 0, 0xffff, (uint16_t)(cl.first >> 16), (uint16_t)(cl.first & 0xFFFF)}, cl.second + 96));
    }
    std::smatch pmatch, amatch;
    const std::regex patreg(std::regex_replace(std::regex_replace(pattern, regex_escape, "\\$&"), regex_wildcard, ".*"));
    if ((pattern == "$private" && address == "localhost") || std::regex_match(address, patreg)) return true;
    else if (std::regex_match(address, amatch, ipv4_regex)) {
        const int a1 = std::stoi(amatch[1]), a2 = std::stoi(amatch[2]), a3 = std::stoi(amatch[3]), a4 = std::stoi(amatch[4]);
        const uint32_t ip = makeIP(a1, a2, a3, a4);
        if (std::regex_match(pattern, pmatch, ipv4_class_regex)) {
            const int b1 = std::stoi(pmatch[1]), b2 = std::stoi(pmatch[2]), b3 = std::stoi(pmatch[3]), b4 = std::stoi(pmatch[4]);
            const uint32_t pattern_ip = makeIP(b1, b2, b3, b4);
            const uint32_t netmask = 0xFFFFFFFFu << std::stoi(pmatch[5]);
            return (pattern_ip & netmask) == (ip & netmask);
        } else if (pattern == "$private")
            for (const auto& cl : reservedIPv4s)
                if ((ip & (0xFFFFFFFFu << cl.second)) == cl.first) return true;
    } // check IPv6 addresses
    return false;
}
