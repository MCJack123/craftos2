/*
 * util.hpp
 * CraftOS-PC 2
 *
 * This file defines some common functions used by various parts of the program.
 *
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
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
#include <Poco/Net/HTTPResponse.h>
#include <Computer.hpp>
#include <Terminal.hpp>

#define CRAFTOSPC_VERSION    "v2.6.5.1"
#define CRAFTOSPC_CC_VERSION "1.100.2"
#define CRAFTOSPC_INDEV      false

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
    ProtectedObject() {}
    ProtectedObject(T o): obj(o) {}
    void lock() { mutex.lock(); isLocked = true; }
    void unlock() { mutex.unlock(); isLocked = false; }
    bool locked() { return isLocked; }
    std::mutex& getMutex() { return mutex; }
    T& operator*() { return obj; }
    T* operator->() { return &obj; }
    ProtectedObject<T>& operator=(const T& rhs) {obj = rhs; return *this;}
    template<typename U>
    U block(const std::function<U()>& func) { std::lock_guard<std::mutex> lock(mutex); return func(); }
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
    template<typename T>
    Value(const std::vector<T>& arr) {
        Poco::JSON::Array jarr;
        for (const T& val : arr) jarr.add(val);
        obj = Poco::Dynamic::Var(jarr);
    }
    Value operator[](const std::string& key) { try { return Value(obj.extract<Poco::JSON::Object>().get(key), this, key); } catch (Poco::BadCastException &e) { return Value(obj.extract<Poco::JSON::Object::Ptr>()->get(key), this, key); } }
    void operator=(int v) { obj = v; updateParent(); }
    void operator=(bool v) { obj = v; updateParent(); }
    void operator=(const char * v) { obj = std::string(v); updateParent(); }
    void operator=(const std::string& v) { obj = v; updateParent(); }
    template<typename T>
    void operator=(const std::vector<T>& v) {
        Poco::JSON::Array jarr;
        for (const T& val : v) jarr.add(val);
        obj = Poco::Dynamic::Var(jarr);
        updateParent();
    }
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

// For get_comp
typedef union {
  void *gc;
  void *p;
  lua_Number n;
  int b;
} lua_Value;

typedef struct lua_TValue {
  lua_Value value; int tt;
} TValue;

struct lua_State {
	void *next;
  unsigned char tt;
  unsigned char marked;
	unsigned char status;
	void* top;  /* first free slot in the stack */
	void* base;  /* base of current function */
	void *l_G;
	void *ci;  /* call info for current function */
	void* ctx;  /* `savedpc' of current function, or context */
	void* stack_last;  /* last free slot in the stack */
	void* stack;  /* stack base */
	void *end_ci;  /* points after end of ci array*/
	void *base_ci;  /* array of CallInfo's */
	int stacksize;
	int size_ci;  /* size of array `base_ci' */
	unsigned short nCcalls;  /* number of nested C calls */
	unsigned short baseCcalls;  /* nested C calls when resuming coroutine */
	unsigned char hookmask;
	unsigned char allowhook;
	int basehookcount;
	int hookcount;
	lua_Hook hook;
	TValue l_gt;  /* table of globals */
	TValue env;  /* temporary place for environments */
	void *openupval;  /* list of open upvalues in this stack */
	void *gclist;
	struct lua_longjmp *errorJmp;  /* current error recover point */
	ptrdiff_t errfunc;  /* current error handling function (stack index) */
};

inline int log2i(int num) {
    if (num == 0) return 0;
    int retval;
    for (retval = 0; (num & 1) == 0; retval++) num = num >> 1;
    return retval;
}

inline unsigned char htoi(char c, unsigned char def) {
    if (c >= '0' && c <= '9') return c - '0';
    else if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return def;
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

extern std::string loadingPlugin;
extern const char * lastCFunction;
extern Computer * get_comp(lua_State *L);
extern void uncache_state(lua_State *L);

template<typename T>
inline T min(T a, T b) { return a < b ? a : b; }
template<typename T>
inline T max(T a, T b) { return a > b ? a : b; }
extern std::string b64encode(const std::string& orig);
extern std::string b64decode(const std::string& orig);
extern std::vector<std::string> split(const std::string& strToSplit, const char * delimeter);
extern std::vector<std::wstring> split(const std::wstring& strToSplit, const wchar_t * delimeter);
extern void load_library(Computer *comp, lua_State *L, const library_t& lib);
extern void HTTPDownload(const std::string& url, const std::function<void(std::istream*, Poco::Exception*, Poco::Net::HTTPResponse*)>& callback);
extern path_t fixpath(Computer *comp, const std::string& path, bool exists, bool addExt = true, std::string * mountPath = NULL, bool getAllResults = false, bool * isRoot = NULL);
extern bool fixpath_ro(Computer *comp, const std::string& path);
extern path_t fixpath_mkdir(Computer * comp, const std::string& path, bool md = true, std::string * mountPath = NULL);
extern std::set<std::string> getMounts(Computer * computer, const std::string& comp_path);
extern void peripheral_update(Computer *comp);
extern struct computer_configuration getComputerConfig(int id);
extern void setComputerConfig(int id, const computer_configuration& cfg);
extern void config_init();
extern void config_save();
extern void xcopy(lua_State *from, lua_State *to, int n);
extern std::pair<int, std::string> recursiveCopy(const path_t& fromPath, const path_t& toPath, std::list<path_t> * failures = NULL);
extern std::string makeASCIISafe(const char * retval, size_t len);
extern bool matchIPClass(const std::string& address, const std::string& pattern);
inline std::string checkstring(lua_State *L, int idx) {
    size_t sz = 0;
    const char * str = luaL_checklstring(L, idx, &sz);
    return std::string(str, sz);
}
inline std::string tostring(lua_State *L, int idx, const std::string& def = "") {
    size_t sz = 0;
    const char * str = lua_tolstring(L, idx, &sz);
    if (str == NULL) return def;
    return std::string(str, sz);
}

#endif