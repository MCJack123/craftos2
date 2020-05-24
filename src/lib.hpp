/*
 * lib.hpp
 * CraftOS-PC 2
 * 
 * This file defines the library structure and some convenience functions for
 * libraries (APIs).
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef LIB_HPP
#define LIB_HPP
#include <functional>
#include <vector>
#include <string>
extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

#define CRAFTOSPC_VERSION "v2.3.2"
#define CRAFTOSPC_INDEV   false

struct Computer;
typedef struct library {
    const char * name;
    int count;
    const char ** keys;
    lua_CFunction * values;
    std::function<void(Computer*)> init;
    std::function<void(Computer*)> deinit;
    ~library() {}
} library_t;

template<typename T>
inline T min(T a, T b) { return a < b ? a : b; }
template<typename T>
inline T max(T a, T b) { return a > b ? a : b; }

#include "Computer.hpp"

extern char computer_key;
extern void load_library(Computer *comp, lua_State *L, library_t lib);
extern void bad_argument(lua_State *L, const char * type, int pos);
extern std::string b64encode(std::string orig);
extern std::string b64decode(std::string orig);
extern std::vector<std::string> split(std::string strToSplit, char delimeter);

#ifdef CRAFTOSPC_INTERNAL // so plugins won't need Poco

#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Parser.h>

class Value {
    Poco::Dynamic::Var obj;
    Value* parent = NULL;
    std::string key;
    Value(Poco::Dynamic::Var o, Value* p, std::string k): obj(o), parent(p), key(k) {}
    void updateParent() {
        if (parent == NULL) return;
        Poco::JSON::Object o(parent->obj.extract<Poco::JSON::Object>());
        o.set(key, obj);
        parent->obj = o;
    }
public:
    Value() {obj = Poco::JSON::Object();}
    Value(Poco::Dynamic::Var o): obj(o) {}
    Value operator[](std::string key) { return Value(obj.extract<Poco::JSON::Object>().get(key), this, key); }
    void operator=(int v) { obj = v; updateParent(); }
    void operator=(bool v) { obj = v; updateParent(); }
    void operator=(std::string v) { obj = v; updateParent(); }
    bool asBool() { return obj.convert<bool>(); }
    int asInt() { return obj.convert<int>(); }
    float asFloat() { return obj.convert<float>(); }
    std::string asString() { return obj.toString(); }
    const char * asCString() { return obj.toString().c_str(); }
    bool isArray() {return obj.isArray();}
    bool isBoolean() {return obj.isBoolean();}
    bool isInt() {return obj.isInteger();}
    bool isString() {return obj.isString();}
    bool isObject() {try {obj.extract<Poco::JSON::Object>(); return true;} catch (Poco::BadCastException &e) {return false;}}
    bool isMember(std::string key) { return obj.extract<Poco::JSON::Object>().has(key); }
    Poco::JSON::Object::Ptr parse(std::istream& in) { Poco::JSON::Object::Ptr p = Poco::JSON::Parser().parse(in).extract<Poco::JSON::Object::Ptr>(); obj = *p; return p; }
    friend std::ostream& operator<<(std::ostream &out, Value &v) { v.obj.extract<Poco::JSON::Object>().stringify(out, 4, -1); return out; }
    //friend std::istream& operator>>(std::istream &in, Value &v) {v.obj = Parser().parse(in).extract<Object::Ptr>(); return in; }
    Poco::JSON::Array::ConstIterator arrayBegin() {return obj.extract<Poco::JSON::Array>().begin();}
    Poco::JSON::Array::ConstIterator arrayEnd() {return obj.extract<Poco::JSON::Array>().end();}
    Poco::JSON::Object::ConstIterator begin() { return obj.extract<Poco::JSON::Object>().begin(); }
    Poco::JSON::Object::ConstIterator end() { return obj.extract<Poco::JSON::Object>().end(); }
};

#endif

#ifdef CRAFTOSPC_INTERNAL
extern void* getCompCache_glob;
extern Computer * getCompCache_comp;
extern Computer * _get_comp(lua_State *L);
#define get_comp(L) (*(void**)(((ptrdiff_t)L) + sizeof(int) + sizeof(void*)*3 + 4) == getCompCache_glob ? getCompCache_comp : _get_comp(L))
#else
inline Computer * get_comp(lua_State *L) {
    //lua_pushlightuserdata(L, &computer_key);
    lua_pushinteger(L, 1);
    lua_gettable(L, LUA_REGISTRYINDEX);
    void * retval = lua_touserdata(L, -1);
    lua_pop(L, 1);
    return (Computer*)retval;
}
#endif
#endif