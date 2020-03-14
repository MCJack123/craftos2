/*
 * peripheral/speaker.hpp
 * CraftOS-PC 2
 * 
 * This file defines the class for the speaker peripheral.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifndef NO_MIXER
#ifndef PERIPHERAL_SPEAKER_HPP
#define PERIPHERAL_SPEAKER_HPP
#include "peripheral.hpp"

class speaker: public peripheral {
private:
    static library_t methods;
    int playNote(lua_State *L);
    int playSound(lua_State *L);
public:
    static peripheral * init(lua_State *L, const char * side) {return new speaker(L, side);}
    static void deinit(peripheral * p) {delete (speaker*)p;}
    speaker(lua_State *L, const char * side);
    ~speaker();
    destructor getDestructor() {return deinit;}
    int call(lua_State *L, const char * method);
    void update() {}
    library_t getMethods() {return methods;}
};

extern void speakerInit();
extern void speakerQuit();

#endif
#endif