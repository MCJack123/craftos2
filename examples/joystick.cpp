/*
 * joystick.cpp
 * CraftOS-PC 2
 * 
 * This plugin adds support for joystick input to CraftOS-PC.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include <CraftOS-PC.hpp>
#include <SDL2/SDL.h>
#include <unordered_map>
#include <unordered_set>
#include <list>

static const PluginFunctions * functions;
static PluginInfo info("joystick");
static std::list<SDL_Joystick*> joys;
static std::unordered_map<int, std::unordered_set<int> > joysById;
struct joy_data {Uint8 id; SDL_JoystickID joy; bool up; Sint16 x; Sint16 y; Uint8 hat;};

struct JoystickLock {
    JoystickLock() {SDL_LockJoysticks();}
    ~JoystickLock() {SDL_UnlockJoysticks();}
};

static std::string joystick_attach(lua_State *L, void* userdata) {
    auto * jdata = (joy_data*)userdata;
    lua_pushinteger(L, jdata->joy);
    std::string retval = jdata->up ? "joystick_detach" : "joystick";
    delete jdata;
    return retval;
}

static std::string joystick_press(lua_State *L, void* userdata) {
    auto * jdata = (joy_data*)userdata;
    lua_pushinteger(L, jdata->joy);
    lua_pushinteger(L, jdata->id);
    std::string retval = jdata->up ? "joystick_up" : "joystick_press";
    delete jdata;
    return retval;
}

static std::string joystick_axis(lua_State *L, void* userdata) {
    auto * jdata = (joy_data*)userdata;
    lua_pushinteger(L, jdata->joy);
    lua_pushinteger(L, jdata->id);
    lua_pushnumber(L, (lua_Number)jdata->x / (jdata->x < 0 ? 32768.0 : 32767.0));
    delete jdata;
    return "joystick_axis";
}

static std::string joystick_ball(lua_State *L, void* userdata) {
    auto * jdata = (joy_data*)userdata;
    lua_pushinteger(L, jdata->joy);
    lua_pushinteger(L, jdata->id);
    lua_pushinteger(L, jdata->x);
    lua_pushinteger(L, jdata->y);
    delete jdata;
    return "joystick_ball";
}

static std::string joystick_hat(lua_State *L, void* userdata) {
    auto * jdata = (joy_data*)userdata;
    lua_pushinteger(L, jdata->joy);
    lua_pushinteger(L, jdata->id);
    if (jdata->hat & SDL_HAT_LEFT) lua_pushinteger(L, -1);
    else if (jdata->hat & SDL_HAT_RIGHT) lua_pushinteger(L, 1);
    else lua_pushinteger(L, 0);
    if (jdata->hat & SDL_HAT_DOWN) lua_pushinteger(L, -1);
    else if (jdata->hat & SDL_HAT_UP) lua_pushinteger(L, 1);
    else lua_pushinteger(L, 0);
    delete jdata;
    return "joystick_hat";
}

static bool axisChange(SDL_Event * e, Computer * comp, Terminal * term, void* userdata) {
    if (comp != NULL && joysById[e->jaxis.which].find(comp->id) != joysById[e->jaxis.which].end()) {
        auto * d = new joy_data;
        d->id = e->jaxis.axis;
        d->joy = e->jaxis.which;
        d->x = e->jaxis.value;
        functions->queueEvent(comp, joystick_axis, d);
    }
    return true;
}

static bool ballChange(SDL_Event * e, Computer * comp, Terminal * term, void* userdata) {
    if (comp != NULL && joysById[e->jball.which].find(comp->id) != joysById[e->jball.which].end()) {
        auto * d = new joy_data;
        d->id = e->jball.ball;
        d->joy = e->jball.which;
        d->x = e->jball.xrel;
        d->y = e->jball.yrel;
        functions->queueEvent(comp, joystick_ball, d);
    }
    return true;
}

static bool buttonChange(SDL_Event * e, Computer * comp, Terminal * term, void* userdata) {
    if (comp != NULL && joysById[e->jbutton.which].find(comp->id) != joysById[e->jbutton.which].end()) {
        auto * d = new joy_data;
        d->id = e->jbutton.button;
        d->joy = e->jbutton.which;
        d->up = e->type == SDL_JOYBUTTONUP;
        functions->queueEvent(comp, joystick_press, d);
    }
    return true;
}

static bool hatChange(SDL_Event * e, Computer * comp, Terminal * term, void* userdata) {
    if (comp != NULL && joysById[e->jhat.which].find(comp->id) != joysById[e->jhat.which].end()) {
        auto * d = new joy_data;
        d->id = e->jhat.hat;
        d->joy = e->jhat.which;
        d->hat = e->jhat.value;
        functions->queueEvent(comp, joystick_hat, d);
    }
    return true;
}

static bool deviceChange(SDL_Event * e, Computer * comp, Terminal * term, void* userdata) {
    if (comp != NULL) {
        auto * d = new joy_data;
        d->joy = e->jdevice.which;
        d->up = e->type == SDL_JOYDEVICEREMOVED;
        functions->queueEvent(comp, joystick_attach, d);
    }
    return true;
}

static int joystick_handle_getAxis(lua_State *L) {
    SDL_Joystick * joy = (SDL_Joystick*)lua_touserdata(L, lua_upvalueindex(1));
    JoystickLock l;
    if (!SDL_JoystickGetAttached(joy)) return luaL_error(L, "joystick is already closed");
    const int id = luaL_checkinteger(L, 1);
    if (id < 0 || id >= SDL_JoystickNumAxes(joy)) luaL_argerror(L, 1, "axis out of range");
    const Sint16 pos = SDL_JoystickGetAxis(joy, id);
    lua_pushnumber(L, (lua_Number)pos / (pos < 0 ? 32768.0 : 32767.0));
    return 1;
}

static int joystick_handle_getBall(lua_State *L) {
    SDL_Joystick * joy = (SDL_Joystick*)lua_touserdata(L, lua_upvalueindex(1));
    JoystickLock l;
    if (!SDL_JoystickGetAttached(joy)) return luaL_error(L, "joystick is already closed");
    const int id = luaL_checkinteger(L, 1);
    if (id < 0 || id >= SDL_JoystickNumBalls(joy)) luaL_argerror(L, 1, "ball out of range");
    int dx = 0, dy = 0;
    SDL_JoystickGetBall(joy, id, &dx, &dy);
    lua_pushinteger(L, dx);
    lua_pushinteger(L, dy);
    return 2;
}

static int joystick_handle_getButton(lua_State *L) {
    SDL_Joystick * joy = (SDL_Joystick*)lua_touserdata(L, lua_upvalueindex(1));
    JoystickLock l;
    if (!SDL_JoystickGetAttached(joy)) return luaL_error(L, "joystick is already closed");
    const int id = luaL_checkinteger(L, 1);
    if (id < 0 || id >= SDL_JoystickNumButtons(joy)) luaL_argerror(L, 1, "button out of range");
    lua_pushboolean(L, SDL_JoystickGetButton(joy, id));
    return 1;
}

static int joystick_handle_getHat(lua_State *L) {
    SDL_Joystick * joy = (SDL_Joystick*)lua_touserdata(L, lua_upvalueindex(1));
    JoystickLock l;
    if (!SDL_JoystickGetAttached(joy)) return luaL_error(L, "joystick is already closed");
    const int id = luaL_checkinteger(L, 1);
    if (id < 0 || id >= SDL_JoystickNumHats(joy)) luaL_argerror(L, 1, "hat out of range");
    const Uint8 pos = SDL_JoystickGetHat(joy, id);
    if (pos & SDL_HAT_LEFT) lua_pushinteger(L, -1);
    else if (pos & SDL_HAT_RIGHT) lua_pushinteger(L, 1);
    else lua_pushinteger(L, 0);
    if (pos & SDL_HAT_DOWN) lua_pushinteger(L, -1);
    else if (pos & SDL_HAT_UP) lua_pushinteger(L, 1);
    else lua_pushinteger(L, 0);
    return 2;
}

static int joystick_handle_getPowerLevel(lua_State *L) {
    SDL_Joystick * joy = (SDL_Joystick*)lua_touserdata(L, lua_upvalueindex(1));
    JoystickLock l;
    if (!SDL_JoystickGetAttached(joy)) return luaL_error(L, "joystick is already closed");
    const SDL_JoystickPowerLevel level = SDL_JoystickCurrentPowerLevel(joy);
    switch (level) {
    case SDL_JOYSTICK_POWER_UNKNOWN: lua_pushnil(L); break;
    case SDL_JOYSTICK_POWER_EMPTY: lua_pushstring(L, "empty"); break;
    case SDL_JOYSTICK_POWER_LOW: lua_pushstring(L, "low"); break;
    case SDL_JOYSTICK_POWER_MEDIUM: lua_pushstring(L, "medium"); break;
    case SDL_JOYSTICK_POWER_FULL: lua_pushstring(L, "full"); break;
    case SDL_JOYSTICK_POWER_WIRED: lua_pushstring(L, "wired"); break;
    case SDL_JOYSTICK_POWER_MAX: lua_pushstring(L, "max"); break;
    }
    return 1;
}

#if SDL_VERSION_ATLEAST(2, 0, 9)
static int joystick_handle_rumble(lua_State *L) {
    SDL_Joystick * joy = (SDL_Joystick*)lua_touserdata(L, lua_upvalueindex(1));
    JoystickLock l;
    if (!SDL_JoystickGetAttached(joy)) return luaL_error(L, "joystick is already closed");
    lua_pushboolean(L, SDL_JoystickRumble(joy, luaL_checknumber(L, 2) * 65535, luaL_checknumber(L, 3) * 65535, luaL_checknumber(L, 1) * 1000) == 0);
    return 1;
}
#endif

static int joystick_handle_close(lua_State *L) {
    SDL_Joystick * joy = (SDL_Joystick*)lua_touserdata(L, lua_upvalueindex(1));
    JoystickLock l;
    if (!SDL_JoystickGetAttached(joy)) return luaL_error(L, "joystick is already closed");
    joysById[SDL_JoystickInstanceID(joy)].erase(get_comp(L)->id);
    joys.remove(joy);
    SDL_JoystickClose(joy);
    return 0;
}

static int joystick_count(lua_State *L) {
    lua_pushinteger(L, SDL_NumJoysticks());
    return 1;
}

static int joystick_open(lua_State *L) {
    int id = luaL_checkinteger(L, 1);
    if (id < 0 || id >= SDL_NumJoysticks()) {
        lua_pushnil(L);
        lua_pushstring(L, "ID out of range");
        return 2;
    }
    JoystickLock l;
    SDL_Joystick * joy = SDL_JoystickOpen(id);
    if (joy == NULL) {
        lua_pushnil(L);
        lua_pushstring(L, SDL_GetError());
        return 2;
    }
    joys.push_back(joy);
    if (joysById.find(SDL_JoystickInstanceID(joy)) == joysById.end()) joysById.insert(std::make_pair(SDL_JoystickInstanceID(joy), std::unordered_set<int>()));
    joysById[SDL_JoystickInstanceID(joy)].insert(get_comp(L)->id);
    lua_newtable(L);

    lua_pushinteger(L, SDL_JoystickInstanceID(joy));
    lua_setfield(L, -2, "id");
    lua_pushstring(L, SDL_JoystickName(joy));
    lua_setfield(L, -2, "name");
    lua_pushinteger(L, SDL_JoystickNumAxes(joy));
    lua_setfield(L, -2, "axes");
    lua_pushinteger(L, SDL_JoystickNumBalls(joy));
    lua_setfield(L, -2, "balls");
    lua_pushinteger(L, SDL_JoystickNumButtons(joy));
    lua_setfield(L, -2, "buttons");
    lua_pushinteger(L, SDL_JoystickNumHats(joy));
    lua_setfield(L, -2, "hats");
#if SDL_VERSION_ATLEAST(2, 0, 9)
    int playeridx = SDL_JoystickGetPlayerIndex(joy);
    if (playeridx != -1) {
        lua_pushinteger(L, playeridx);
        lua_setfield(L, -2, "player");
    }
    lua_pushlightuserdata(L, joy);
    lua_pushcclosure(L, joystick_handle_rumble, 1);
    lua_setfield(L, -2, "rumble");
#endif

    lua_pushlightuserdata(L, joy);
    lua_pushcclosure(L, joystick_handle_getAxis, 1);
    lua_setfield(L, -2, "getAxis");
    lua_pushlightuserdata(L, joy);
    lua_pushcclosure(L, joystick_handle_getBall, 1);
    lua_setfield(L, -2, "getBall");
    lua_pushlightuserdata(L, joy);
    lua_pushcclosure(L, joystick_handle_getButton, 1);
    lua_setfield(L, -2, "getButton");
    lua_pushlightuserdata(L, joy);
    lua_pushcclosure(L, joystick_handle_getHat, 1);
    lua_setfield(L, -2, "getHat");
    lua_pushlightuserdata(L, joy);
    lua_pushcclosure(L, joystick_handle_getPowerLevel, 1);
    lua_setfield(L, -2, "getPowerLevel");
    lua_pushlightuserdata(L, joy);
    lua_pushcclosure(L, joystick_handle_close, 1);
    lua_setfield(L, -2, "close");

    return 1;
}

static luaL_Reg reg[] = {
    {"count", joystick_count},
    {"open", joystick_open},
    {NULL, NULL}
};

extern "C" {
__declspec(dllexport) PluginInfo * plugin_init(const PluginFunctions * func, const path_t& path) {
    functions = func;
    func->registerSDLEvent(SDL_JOYAXISMOTION, axisChange, NULL);
    func->registerSDLEvent(SDL_JOYBALLMOTION, ballChange, NULL);
    func->registerSDLEvent(SDL_JOYBUTTONDOWN, buttonChange, NULL);
    func->registerSDLEvent(SDL_JOYBUTTONUP, buttonChange, NULL);
    func->registerSDLEvent(SDL_JOYHATMOTION, hatChange, NULL);
    func->registerSDLEvent(SDL_JOYDEVICEADDED, deviceChange, NULL);
    func->registerSDLEvent(SDL_JOYDEVICEREMOVED, deviceChange, NULL);
    SDL_JoystickEventState(true);
    for (int i = 0; i < SDL_NumJoysticks(); i++) joys.push_back(SDL_JoystickOpen(0));
    return &info;
}

__declspec(dllexport) int luaopen_joystick(lua_State *L) {
    luaL_register(L, "joystick", reg);
    return 1;
}

__declspec(dllexport) void plugin_deinit(PluginInfo * info) {for (SDL_Joystick * j : joys) SDL_JoystickClose(j);}
}