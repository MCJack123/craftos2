/*
 * util.hpp
 * CraftOS-PC 2
 *
 * This file defines some common functions used by various parts of the program.
 *
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2021 JackMacWindows.
 */

#ifndef UTIL_HPP
#define UTIL_HPP

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#include <functional>
#include <mutex>
#include <string>
#include <vector>
#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Parser.h>
#include <Computer.hpp>
#include <Terminal.hpp>

#define CRAFTOSPC_VERSION    "v2.6"
#define CRAFTOSPC_CC_VERSION "2.0.0"
#define CRAFTOSPC_INDEV      true

// for some reason Clang complains if this isn't present
#ifdef __clang__
template<> class std::hash<SDL_EventType>: public std::hash<unsigned short> {};
#endif

template<typename T>
class ProtectedObject {
    friend class LockGuard;
    T obj;
    std::mutex mutex;
    bool isLocked;
public:
    void lock() { mutex.lock(); isLocked = true; }
    void unlock() { mutex.unlock(); isLocked = false; }
    bool locked() { return isLocked; }
    std::mutex& getMutex() { return mutex; }
    T& operator*() { return obj; }
    T* operator->() { return &obj; }
};

class LockGuard : public std::lock_guard<std::mutex> {
    bool * isLocked = NULL;
public:
    template<typename T>
    LockGuard(ProtectedObject<T> &obj) : std::lock_guard<std::mutex>(obj.mutex) { obj.isLocked = true; isLocked = &obj.isLocked; }
    LockGuard(std::mutex mtx) : std::lock_guard<std::mutex>(mtx) {}
    ~LockGuard() { if (isLocked != NULL) *isLocked = false; }
};

template<typename I>
class Range {
    I front;
    I back;
public:
    Range(std::pair<I, I> it): front(it.first), back(it.second) {}
    I begin() const {return front;}
    I end() const {return back;}
};

class Value {
    Poco::Dynamic::Var obj;
    Value* parent = NULL;
    std::string key;
    Value(Poco::Dynamic::Var o, Value* p, std::string k) : obj(o), parent(p), key(k) {}
    void updateParent() {
        if (parent == NULL) return;
        Poco::JSON::Object o(parent->obj.extract<Poco::JSON::Object>());
        o.set(key, obj);
        parent->obj = o;
    }
public:
    Value() { obj = Poco::Dynamic::Var(Poco::JSON::Object()); }
    Value(Poco::Dynamic::Var o) : obj(o) {}
    Value operator[](std::string key) { try { return Value(obj.extract<Poco::JSON::Object>().get(key), this, key); } catch (Poco::BadCastException &e) { return Value(obj.extract<Poco::JSON::Object::Ptr>()->get(key), this, key); } }
    void operator=(int v) { obj = v; updateParent(); }
    void operator=(bool v) { obj = v; updateParent(); }
    void operator=(const char * v) { obj = std::string(v); updateParent(); }
    void operator=(std::string v) { obj = v; updateParent(); }
    void operator=(Poco::Dynamic::Var v) { obj = v; updateParent(); }
    void operator=(const Value& v) { obj = v.obj; updateParent(); }
    bool asBool() { return obj.convert<bool>(); }
    int asInt() { return obj.convert<int>(); }
    float asFloat() { return obj.convert<float>(); }
    std::string asString() { return obj.toString(); }
    Poco::JSON::Array asArray() { try {return obj.extract<Poco::JSON::Array>();} catch (Poco::BadCastException &e) {return *obj.extract<Poco::JSON::Array::Ptr>();} }
    //const char * asCString() { return obj.toString().c_str(); }
    bool isArray() { return obj.isArray(); }
    bool isBoolean() { return obj.isBoolean(); }
    bool isInt() { return obj.isInteger(); }
    bool isString() { return obj.isString(); }
    bool isObject() { try { obj.extract<Poco::JSON::Object>(); return true; } catch (Poco::BadCastException &e) { try { obj.extract<Poco::JSON::Object::Ptr>(); return true; } catch (Poco::BadCastException &e2) { return false; } } }
    bool isMember(std::string key) { try { return obj.extract<Poco::JSON::Object>().has(key); } catch (Poco::BadCastException &e) { return obj.extract<Poco::JSON::Object::Ptr>()->has(key); } }
    Poco::JSON::Object::Ptr parse(std::istream& in) { Poco::JSON::Object::Ptr p = Poco::JSON::Parser().parse(in).extract<Poco::JSON::Object::Ptr>(); obj = *p; return p; }
    friend std::ostream& operator<<(std::ostream &out, Value &v) { v.obj.extract<Poco::JSON::Object>().stringify(out, 4, -1); return out; }
    //friend std::istream& operator>>(std::istream &in, Value &v) {v.obj = Parser().parse(in).extract<Object::Ptr>(); return in; }
    Poco::JSON::Array::ConstIterator arrayBegin() { try { return obj.extract<Poco::JSON::Array>().begin(); } catch (Poco::BadCastException &e) { return obj.extract<Poco::JSON::Array::Ptr>()->begin(); } }
    Poco::JSON::Array::ConstIterator arrayEnd() { try { return obj.extract<Poco::JSON::Array>().end(); } catch (Poco::BadCastException &e) { return obj.extract<Poco::JSON::Array::Ptr>()->end(); } }
    Poco::JSON::Object::ConstIterator begin() { try { return obj.extract<Poco::JSON::Object>().begin(); } catch (Poco::BadCastException &e) { return obj.extract<Poco::JSON::Object::Ptr>()->begin(); } }
    Poco::JSON::Object::ConstIterator end() { try { return obj.extract<Poco::JSON::Object>().end(); } catch (Poco::BadCastException &e) { return obj.extract<Poco::JSON::Object::Ptr>()->end(); } }
};

inline int log2i(int num) {
    if (num == 0) return 0;
    int retval;
    for (retval = 0; (num & 1) == 0; retval++) num = num >> 1;
    return retval;
}

inline unsigned char htoi(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    else if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

inline std::string asciify(std::string str) {
    std::string retval;
    for (char c : str) { if (c < 32 || c > 127) retval += '?'; else retval += c; }
    return retval;
}

#ifdef WIN32
#define PATH_SEP L"\\"
#define PATH_SEPC '\\'
#else
#define PATH_SEP "/"
#define PATH_SEPC '/'
#endif

extern struct configuration config;
extern std::unordered_map<std::string, std::pair<int, int> > configSettings;
extern std::unordered_map<std::string, std::tuple<int, std::function<int(const std::string&, void*)>, void*> > userConfig;
extern std::list<Terminal*> renderTargets;
extern std::mutex renderTargetsLock;
#ifdef __EMSCRIPTEN__
extern std::list<Terminal*>::iterator renderTarget;
#endif

extern std::string loadingPlugin;
extern const char * lastCFunction;
extern void* getCompCache_glob;
extern Computer * getCompCache_comp;
extern Computer * _get_comp(lua_State *L);
#define get_comp(L) (*(void**)(((ptrdiff_t)L) + sizeof(void*)*3 + 3 + alignof(void*) - ((sizeof(void*)*3 + 3) % alignof(void*))) == getCompCache_glob ? getCompCache_comp : _get_comp(L))

template<typename T>
inline T min(T a, T b) { return a < b ? a : b; }
template<typename T>
inline T max(T a, T b) { return a > b ? a : b; }
extern std::string b64encode(const std::string& orig);
extern std::string b64decode(const std::string& orig);
extern std::vector<std::string> split(const std::string& strToSplit, const char * delimeter);
extern std::vector<std::wstring> split(const std::wstring& strToSplit, const wchar_t * delimeter);
extern void load_library(Computer *comp, lua_State *L, const library_t& lib);
extern void HTTPDownload(const std::string& url, const std::function<void(std::istream*, Poco::Exception*)>& callback);
extern path_t fixpath(Computer *comp, const char * path, bool exists, bool addExt = true, std::string * mountPath = NULL, bool getAllResults = false, bool * isRoot = NULL);
extern bool fixpath_ro(Computer *comp, const char * path);
extern path_t fixpath_mkdir(Computer * comp, const std::string& path, bool md = true, std::string * mountPath = NULL);
extern std::set<std::string> getMounts(Computer * computer, const char * comp_path);
extern void peripheral_update(Computer *comp);
extern struct computer_configuration getComputerConfig(int id);
extern void setComputerConfig(int id, const computer_configuration& cfg);
extern void config_init();
extern void config_save();
extern void xcopy(lua_State *from, lua_State *to, int n);
extern std::pair<int, std::string> recursiveCopy(const path_t& fromPath, const path_t& toPath, std::list<path_t> * failures = NULL);
extern std::string makeASCIISafe(const char * retval, size_t len);

#endif