/*
 * peripheral/modem.hpp
 * CraftOS-PC 2
 * 
 * This file defines the class for the modem peripheral.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2022 JackMacWindows. 
 */

#ifndef PERIPHERAL_MODEM_HPP
#define PERIPHERAL_MODEM_HPP
#include <mutex>
#include <unordered_set>
#include <peripheral.hpp>

class modem: public peripheral {
private:
    friend std::string modem_message(lua_State *, void*);
    std::unordered_set<uint16_t> openPorts;
    Computer * comp;
    int eventQueuePosition;
    lua_State * eventQueue;
    std::mutex eventQueueMutex;
    std::unordered_set<int> idsToDelete;
    std::string side;
    int netID = 0;
    int isOpen(lua_State *L);
    int open(lua_State *L);
    int close(lua_State *L);
    int closeAll(lua_State *L);
    int transmit(lua_State *L);
    int isWireless(lua_State *L);
    int getNamesRemote(lua_State *L);
    int getTypeRemote(lua_State *L);
    int isPresentRemote(lua_State *L);
    int getMethodsRemote(lua_State *L);
    int callRemote(lua_State *L);
    int hasTypeRemote(lua_State *L);
    void receive(lua_State *data, uint16_t port, uint16_t replyPort, modem * sender);
public:
    static library_t methods;
    static peripheral * init(lua_State *L, const char * side) {return new modem(L, side);}
    static void deinit(peripheral * p) {delete (modem*)p;}
    destructor getDestructor() const override {return deinit;}
    library_t getMethods() const override {return methods;}
    modem(lua_State *L, const char * side);
    ~modem();
    int call(lua_State *L, const char * method) override;
    void reinitialize(lua_State *L) override;
};

#endif