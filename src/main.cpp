/*
 * main.cpp
 * CraftOS-PC 2
 * 
 * This file controls the Lua VM, loads the CraftOS BIOS, and sends events back.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#define CRAFTOSPC_INTERNAL
#include "Computer.hpp"
#include "config.hpp"
#include "peripheral/drive.hpp"
#include "platform.hpp"
#include "terminal/CLITerminal.hpp"
#include "terminal/RawTerminal.hpp"
#include "terminal/SDLTerminal.hpp"
#include <functional>
#include <thread>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/SSLException.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/JSON/Parser.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

extern void config_init();
extern void config_save(bool deinit);
extern void mainLoop();
extern void awaitTasks();
extern void http_server_stop();
extern void* queueTask(std::function<void*(void*)> func, void* arg, bool async = false);
extern std::list<std::thread*> computerThreads;
extern bool exiting;
int selectedRenderer = 0; // 0 = SDL, 1 = headless, 2 = CLI, 3 = Raw
bool rawClient = false;
std::string script_file;
std::string script_args;
std::string updateAtQuit;

void update_thread() {
    try {
        Poco::Net::HTTPSClientSession session("api.github.com", 443, new Poco::Net::Context(Poco::Net::Context::CLIENT_USE, "", Poco::Net::Context::VERIFY_NONE, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"));
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/repos/MCJack123/craftos2/releases/latest", Poco::Net::HTTPMessage::HTTP_1_1);
        Poco::Net::HTTPResponse response;
        session.setTimeout(Poco::Timespan(5000000));
        request.add("User-Agent", "CraftOS-PC/2.0 Poco/1.9.3");
        session.sendRequest(request);
        Poco::JSON::Parser parser;
        parser.parse(session.receiveResponse(response));
        Poco::JSON::Object::Ptr root = parser.asVar().extract<Poco::JSON::Object::Ptr>();
        if (root->getValue<std::string>("tag_name") != CRAFTOSPC_VERSION) {
#if defined(__APPLE__) || defined(WIN32)
            SDL_MessageBoxData msg;
            SDL_MessageBoxButtonData buttons[] = {
                {0, 0, "Skip This Version"},
                {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 1, "Ask Me Later"},
                {0, 2, "Update On Quit"},
                {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 3, "Update Now"}
            };
            msg.flags = SDL_MESSAGEBOX_INFORMATION;
            msg.window = NULL;
            msg.title = "Update available!";
            std::string message = (std::string("A new update to CraftOS-PC is available (") + root->getValue<std::string>("tag_name") + " is the latest version, you have " CRAFTOSPC_VERSION "). Would you like to update to the latest version?");
            msg.message = message.c_str();
            msg.numbuttons = 4;
            msg.buttons = buttons;
            msg.colorScheme = NULL;
            int* choicep = (int*)queueTask([ ](void* arg)->void*{int* num = new int; SDL_ShowMessageBox((SDL_MessageBoxData*)arg, num); return num;}, &msg);
            int choice = *choicep;
            delete choicep;
            switch (choice) {
                case 0:
                    config.skipUpdate = CRAFTOSPC_VERSION;
                    return;
                case 1:
                    return;
                case 2:
                    updateAtQuit = root->getValue<std::string>("tag_name");
                    return;
                case 3:
                    queueTask([root](void*)->void*{updateNow(root->getValue<std::string>("tag_name")); return NULL;}, NULL);
                    return;
                default:
                    // this should never happen
                    exit(choice);
            }
#else
            queueTask([](void* arg)->void* {SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Update available!", (const char*)arg, NULL); return NULL; }, (void*)(std::string("A new update to CraftOS-PC is available (") + root->getValue<std::string>("tag_name") + " is the latest version, you have " CRAFTOSPC_VERSION "). Go to " + root->getValue<std::string>("html_url") + " to download the new version.").c_str());
#endif
        }
    } catch (std::exception &e) {
        printf("Could not check for updates: %s\n", e.what());
    }
}

int runRenderer() {
    if (selectedRenderer == 1) {
        std::cerr << "Error: Headless mode cannot be used in conjunction with raw client mode.";
        return 3;
    } else if (selectedRenderer == 2) CLITerminal::init();
    else if (selectedRenderer == 3) RawTerminal::init();
    else SDLTerminal::init();
    Terminal * term;
#ifndef NO_CLI
    if (selectedRenderer == 2) term = new CLITerminal("CraftOS Terminal");
    else
#endif
    if (selectedRenderer == 3) term = new RawTerminal(51, 19);
    else term = new SDLTerminal("CraftOS Terminal");
    while (true) {
        unsigned char c = std::cin.get();
        if (c == '!' && std::cin.get() == 'C' && std::cin.get() == 'P' && std::cin.get() == 'C') {
            char size[5];
            std::cin.read(size, 4);
            int sizen = strtol(size, NULL, 16);
            char * tmp = new char[sizen+1];
            tmp[sizen] = 0;
            std::cin.read(tmp, sizen);
            std::stringstream in(b64decode(tmp));
            delete[] tmp;
            uint16_t width, height;
            in.read((char*)&width, 2);
            in.read((char*)&height, 2);
            term->id = in.get();
            term->mode = in.get();
            term->blink = in.get();
            in.get();
            in.read((char*)&term->blinkX, 2);
            in.read((char*)&term->blinkY, 2);
            if (term->mode == 0) {
                unsigned char c = in.get();
                unsigned char n = in.get();
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        term->screen[y][x] = c;
                        n--;
                        if (n == 0) {
                            c = in.get();
                            n = in.get();
                        }
                    }
                }
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        term->colors[y][x] = c;
                        n--;
                        if (n == 0) {
                            c = in.get();
                            n = in.get();
                        }
                    }
                }
                in.putback(n);
                in.putback(c);
            } else {
                unsigned char c = in.get();
                unsigned char n = in.get();
                for (int y = 0; y < height * 9; y++) {
                    for (int x = 0; x < width * 6; x++) {
                        term->pixels[y][x] = c;
                        n--;
                        if (n == 0) {
                            c = in.get();
                            n = in.get();
                        }
                    }
                }
                in.putback(n);
                in.putback(c);
            }
            if (term->mode != 2) {
                for (int i = 0; i < 16; i++) {
                    term->palette[i].r = in.get();
                    term->palette[i].g = in.get();
                    term->palette[i].b = in.get();
                }
            } else {
                for (int i = 0; i < 256; i++) {
                    term->palette[i].r = in.get();
                    term->palette[i].g = in.get();
                    term->palette[i].b = in.get();
                }
            }
            term->changed = true;
            term->render();
            if (selectedRenderer == 0) {
                SDLTerminal * window = (SDLTerminal*)term;
                SDL_BlitSurface(window->surf, NULL, SDL_GetWindowSurface(window->win), NULL);
                SDL_UpdateWindowSurface(window->win);
                SDL_FreeSurface(window->surf);
                window->surf = NULL;
                SDL_PumpEvents();
            }
        }
    }
    return 0;
}

int main(int argc, char*argv[]) {
#ifdef __EMSCRIPTEN__
    EM_ASM(
        FS.mkdir('/user-data');
        FS.mount(IDBFS, {}, '/user-data');
        FS.syncfs(true, function(err) {if (err) console.log('Error while loading filesystem: ', err);});
    );
#endif
    int id = 0;
    bool manualID = false;
    for (int i = 1; i < argc; i++) {
		if (std::string(argv[i]) == "--headless") selectedRenderer = 1;
		else if (std::string(argv[i]) == "--cli" || std::string(argv[i]) == "-c") selectedRenderer = 2;
        else if (std::string(argv[i]) == "--raw") selectedRenderer = 3;
        else if (std::string(argv[i]) == "--raw-client") rawClient = true;
		else if (std::string(argv[i]) == "--script") script_file = argv[++i];
		else if (std::string(argv[i]).substr(0, 9) == "--script=") script_file = std::string(argv[i]).substr(9);
		else if (std::string(argv[i]) == "--args") script_args = argv[++i];
		else if (std::string(argv[i]) == "--directory" || std::string(argv[i]) == "-d") setBasePath(argv[++i]);
		else if (std::string(argv[i]) == "--rom") setROMPath(argv[++i]);
        else if (std::string(argv[i]) == "-i" || std::string(argv[i]) == "--id") {manualID = true; id = std::stoi(argv[++i]);}
        else if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h" || std::string(argv[i]) == "-?") {
            std::cerr << "Usage: " << argv[0] << " [-c|--cli] [--headless] [-d|--directory <path>] [--rom <path>] [-i|--id <id>] [--script <file>] [--args <args>]\n";
            return 0;
        }
    }
#ifdef NO_CLI
    if (selectedRenderer == 2) {
        std::cerr << "Warning: CraftOS-PC was not built with CLI support, but the --cli flag was specified anyway. Continuing in GUI mode.\n";
        selectedRenderer = 0;
    }
#endif
    migrateData();
    config_init();
    if (rawClient) return runRenderer();
#ifndef NO_CLI
    if (selectedRenderer == 2) CLITerminal::init();
    else 
#endif
    if (selectedRenderer == 3) RawTerminal::init();
    else if (selectedRenderer == 0) SDLTerminal::init();
    driveInit();
#ifndef __EMSCRIPTEN__
    if (!CRAFTOSPC_INDEV && selectedRenderer == 0 && config.checkUpdates && config.skipUpdate != CRAFTOSPC_VERSION) 
        std::thread(update_thread).detach();
#endif
    startComputer(manualID ? id : config.initialComputer);
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 60, 1);
    return 0;
#else
    mainLoop();
#endif
    for (std::thread *t : computerThreads) { if (t->joinable()) {t->join(); delete t;} }
    driveQuit();
    http_server_stop();
    config_save(true);
    if (!updateAtQuit.empty()) {
        updateNow(updateAtQuit);
        awaitTasks();
    }
#ifndef NO_CLI
    if (selectedRenderer == 2) CLITerminal::quit();
    else 
#endif
    if (selectedRenderer == 3) RawTerminal::quit();
    else if (selectedRenderer == 0) SDLTerminal::quit();
    return 0;
}
