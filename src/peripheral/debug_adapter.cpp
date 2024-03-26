/*
 * peripheral/debug_adapter.cpp
 * CraftOS-PC 2
 * 
 * This file implements the Debug Adapter Protocol connection.
 * 
 * This code is licensed under the MIT License.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

static void forwardInput();
#include "debug_adapter.hpp"
#include "../runtime.hpp"
#include "../terminal/SDLTerminal.hpp"
#include "../terminal/HardwareSDLTerminal.hpp"
#include "../termsupport.hpp"
#include <Poco/Net/TCPServerConnection.h>
#include <Poco/Net/TCPServerConnectionFactory.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static debug_adapter * stdio_debugger = NULL;
static std::mutex stdio_debugger_lock;
static std::thread * inputThread = NULL;

static std::string dap_input(lua_State *L, void* arg) {
    std::string * str = (std::string*)arg;
    lua_pushlstring(L, str->c_str(), str->size());
    delete str;
    return "dap_input";
}

static void forwardInput() {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    while (!exiting) {
        std::string data, line;
        do {
            std::getline(std::cin, line);
            data += line + "\n";
        } while (!exiting && line != "" && line != "\r");
        if (exiting) break;
        size_t sz = 0;
        bool state = false;
        for (int i = 0; i < data.size(); i++) {
            if (!state && isdigit(data[i])) {state = true; sz = data[i] - '0';}
            else if (isdigit(data[i])) sz = sz * 10 + (data[i] - '0');
            else if (state) break;
        }
        std::cerr << sz << "\n";
        for (int i = 0; i < sz; i++) data += std::cin.get();
        std::lock_guard<std::mutex> lock(stdio_debugger_lock);
        if (stdio_debugger != NULL) {
            std::string * str = new std::string(data);
            queueEvent(stdio_debugger->monitor, dap_input, str);
        }
    }
}

class DAPConnection: public Poco::Net::TCPServerConnection {
    debug_adapter * debug;
public:
    DAPConnection(const Poco::Net::StreamSocket& sock, debug_adapter * d): TCPServerConnection(sock), debug(d) {}
    void run() override {
        Poco::Net::StreamSocket& socket = this->socket();
        if (debug->socket != NULL) return;
        debug->socket = &socket;
        char buffer[4096];
        while (true) {
            int size = socket.receiveBytes(buffer, 4096);
            if (size <= 0) break;
            std::string * str = new std::string(buffer, size);
            queueEvent(debug->monitor, dap_input, str);
        }
        debug->socket = NULL;
    }

    class Factory: public Poco::Net::TCPServerConnectionFactory {
        debug_adapter * debug;
    public:
        Factory(debug_adapter * d): debug(d) {}
        Poco::Net::TCPServerConnection * createConnection(const Poco::Net::StreamSocket & socket) override {
            return new DAPConnection(socket, debug);
        }
    };
};

debug_adapter::debug_adapter(lua_State *L, const char * side): debugger(L, side), server(new DAPConnection::Factory(this), 12100 + get_comp(L)->id) {
    if (inputThread == NULL && (selectedRenderer == 0 || selectedRenderer == 5)) inputThread = new std::thread(forwardInput);
    if (stdio_debugger == NULL) {
        stdio_debugger = this;
#ifdef _WIN32
        // Disable CRLF -> LF conversion (DAP needs CRLF)
        _setmode(0, _O_BINARY);
        _setmode(1, _O_BINARY);
#endif
    }
    server.start();
    std::lock_guard<std::mutex> lock(renderTargetsLock);
    if (singleWindowMode) {
        const auto pos = currentWindowIDs.find(monitor->term->id);
        if (pos != currentWindowIDs.end()) currentWindowIDs.erase(pos);
    }
    for (auto it = renderTargets.begin(); it != renderTargets.end(); ++it) {
        if (*it == monitor->term)
            it = renderTargets.erase(it);
        if (it == renderTargets.end()) break;
    }
    HardwareSDLTerminal * hwterm = dynamic_cast<HardwareSDLTerminal*>(monitor->term);
    SDLTerminal * term = dynamic_cast<SDLTerminal*>(monitor->term);
    if (hwterm) {
        SDL_DestroyTexture(hwterm->pixtex);
        SDL_DestroyTexture(hwterm->font);
        SDL_DestroyRenderer(hwterm->ren);
        hwterm->pixtex = hwterm->font = NULL;
        hwterm->ren = NULL;
    }
    if (term) {
        SDL_DestroyWindow(term->win);
        term->win = NULL;
    }
}

debug_adapter::~debug_adapter() {
    if (stdio_debugger == this) stdio_debugger = NULL;
}

void debug_adapter::sendData(const std::string& data) {
    if (stdio_debugger == this) {
        std::cout.write(data.c_str(), data.size());
        std::cout << "\n";
        std::cout.flush();
    }
    if (socket != NULL) {
        socket->sendBytes(data.c_str(), data.size());
    }
}
