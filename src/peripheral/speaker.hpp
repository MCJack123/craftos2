/*
 * peripheral/speaker.hpp
 * CraftOS-PC 2
 * 
 * This file defines the class for the speaker peripheral.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#ifndef NO_MIXER
#ifndef PERIPHERAL_SPEAKER_HPP
#define PERIPHERAL_SPEAKER_HPP
#include <chrono>
#include <peripheral.hpp>

static void audioEffect(int chan, void *stream, int len, void *udata);
static Uint32 audioTimer(Uint32 interval, void* param);
static Uint32 speaker_audio_empty_timer(Uint32 interval, void* param);
class speaker: public peripheral {
    friend void speakerInit();
    friend void audioEffect(int chan, void *stream, int len, void *udata);
    friend Uint32 audioTimer(Uint32 interval, void* param);
    friend Uint32 speaker_audio_empty_timer(Uint32 interval, void* param);
    static library_t methods;
    static int nextChannelGroup;
    static int sampleSize;
    std::chrono::system_clock::time_point lastTickReset = std::chrono::system_clock::now();
    unsigned int noteCount = 0;
    int channelGroup;
    int audioChannel;
    ProtectedObject<std::queue<void*>> audioQueue;
    std::queue<double> volumeQueue;
    int audioQueueEnd = 0;
    uint8_t * delayedBuffer;
    int delayedBufferPos = 0;
    SDL_TimerID delayedBufferTimer = 0;
    Computer * comp;
    std::string side;
    int playNote(lua_State *L);
    int playSound(lua_State *L);
    int playAudio(lua_State *L);
    int playLocalMusic(lua_State *L);
    int listSounds(lua_State *L);
    int setSoundFont(lua_State *L);
    int stop(lua_State *L);
    int setPosition(lua_State *L);
public:
    static peripheral * init(lua_State *L, const char * side) {return new speaker(L, side);}
    static void deinit(peripheral * p) {delete (speaker*)p;}
    speaker(lua_State *L, const char * side);
    ~speaker();
    destructor getDestructor() const override {return deinit;}
    int call(lua_State *L, const char * method) override;
    library_t getMethods() const override {return methods;}
};

extern void speakerInit();
extern void speakerQuit();

#endif
#endif