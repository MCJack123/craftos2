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
#include <iomanip>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/SSLException.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Checksum.h>
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
extern std::mutex taskQueueMutex;
extern std::atomic_bool taskQueueReady;
extern std::condition_variable taskQueueNotify;
int selectedRenderer = 0; // 0 = SDL, 1 = headless, 2 = CLI, 3 = Raw
bool rawClient = false;
std::map<uint8_t, Terminal*> rawClientTerminals;
std::unordered_map<unsigned, uint8_t> rawClientTerminalIDs;
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

inline Terminal * createTerminal(std::string title) {
#ifndef NO_CLI
    if (selectedRenderer == 2) return new CLITerminal(title);
    else
#endif
    if (selectedRenderer == 3) return new RawTerminal(title);
    else return new SDLTerminal(title);
}

extern std::thread::id mainThreadID;

int runRenderer() {
    if (selectedRenderer != 0) {
        std::cerr << "Error: Raw client mode requires using a GUI terminal.\n";
        return 3;
    } else SDLTerminal::init();
    std::thread inputThread([](){
        while (!exiting) {
            unsigned char c = std::cin.get();
            if (c == '!' && std::cin.get() == 'C' && std::cin.get() == 'P' && std::cin.get() == 'C') {
                char size[5];
                std::cin.read(size, 4);
                long sizen = strtol(size, NULL, 16);
                char * tmp = new char[sizen+1];
                tmp[sizen] = 0;
                std::cin.read(tmp, sizen);
                Poco::Checksum chk;
                chk.update(tmp, sizen);
                char hexstr[9];
                std::cin.read(hexstr, 8);
                hexstr[8] = 0;
                if (chk.checksum() != strtoul(hexstr, NULL, 16)) {
                    fprintf(stderr, "Invalid checksum: expected %08X, got %08lX\n", chk.checksum(), strtoul(hexstr, NULL, 16));
                    continue;
                }
                std::stringstream in(b64decode(tmp));
                delete[] tmp;
                uint8_t type = in.get();
                uint8_t id = in.get();
                switch (type) {
                case 0: {
                    if (rawClientTerminals.find(id) != rawClientTerminals.end()) {
                        Terminal * term = rawClientTerminals[id];
                        term->mode = in.get();
                        term->blink = in.get();
                        uint16_t width, height;
                        in.read((char*)&width, 2);
                        in.read((char*)&height, 2);
                        in.read((char*)&term->blinkX, 2);
                        in.read((char*)&term->blinkY, 2);
                        in.seekg(in.tellg()+std::streamoff(4)); // reserved
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
                    }
                    break;
                } case 4: {
                    uint8_t quit = in.get();
                    if (quit == 1) {
                        queueTask([id](void*)->void*{
                            rawClientTerminalIDs.erase(rawClientTerminals[id]->id);
                            delete rawClientTerminals[id];
                            rawClientTerminals.erase(id);
                            return NULL;
                        }, NULL);
                        break;
                    } else if (quit == 2) {
                        exiting = true;
                        if (selectedRenderer == 0) {
                            SDL_Event e;
                            memset(&e, 0, sizeof(SDL_Event));
                            e.type = SDL_QUIT;
                            SDL_PushEvent(&e);
                        }
                        return;
                    }
                    in.get(); // reserved
                    uint16_t width = 0, height = 0;
                    in.read((char*)&width, 2);
                    in.read((char*)&height, 2);
                    std::string title;
                    char c;
                    while ((c = in.get())) title += c;
                    if (rawClientTerminals.find(id) == rawClientTerminals.end()) {
                        rawClientTerminals[id] = (Terminal*)queueTask([](void*t)->void*{return createTerminal(*(std::string*)t);}, &title);
                        rawClientTerminalIDs[rawClientTerminals[id]->id] = id;
                    } else rawClientTerminals[id]->setLabel(title);
                    rawClientTerminals[id]->resize(width, height);
                    break;
                } case 5: {
                    uint32_t flags = 0;
                    std::string title, message;
                    char c;
                    in.read((char*)&flags, 4);
                    while ((c = in.get())) title += c;
                    while ((c = in.get())) message += c;
                    if (rawClientTerminals.find(id) != rawClientTerminals.end()) rawClientTerminals[id]->showMessage(flags, title.c_str(), message.c_str());
                }}
            }
        }
    });
    mainLoop();
    inputThread.join();
    for (auto t : rawClientTerminals) delete t.second;
    SDLTerminal::quit();
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
        else if (std::string(argv[i]) == "--gui" || std::string(argv[i]) == "--sdl") selectedRenderer = 0;
		else if (std::string(argv[i]) == "--cli" || std::string(argv[i]) == "-c") selectedRenderer = 2;
        else if (std::string(argv[i]) == "--raw") selectedRenderer = 3;
        else if (std::string(argv[i]) == "--raw-client") rawClient = true;
		else if (std::string(argv[i]) == "--script") script_file = argv[++i];
		else if (std::string(argv[i]).substr(0, 9) == "--script=") script_file = std::string(argv[i]).substr(9);
		else if (std::string(argv[i]) == "--args") script_args = argv[++i];
		else if (std::string(argv[i]) == "--directory" || std::string(argv[i]) == "-d") setBasePath(argv[++i]);
		else if (std::string(argv[i]) == "--rom") setROMPath(argv[++i]);
        else if (std::string(argv[i]) == "-i" || std::string(argv[i]) == "--id") {manualID = true; id = std::stoi(argv[++i]);}
        else if (std::string(argv[i]) == "-V" || std::string(argv[i]) == "--version") {
            std::cout << "CraftOS-PC " << CRAFTOSPC_VERSION;
#if CRAFTOSPC_INDEV == true && defined(CRAFTOSPC_COMMIT)
            std::cout << " (commit " << CRAFTOSPC_COMMIT << ")";
#endif
            std::cout << "\nBuilt with:";
#ifndef NO_CLI
            std::cout << " cli";
#endif
#ifndef NO_PNG
            std::cout << " png";
#endif
#ifndef NO_MIXER
            std::cout << " mixer";
#endif
#ifdef __EMSCRIPTEN__
            std::cout << " wasm";
#endif
#if PRINT_TYPE == 0
            std::cout << " print_pdf";
#elif PRINT_TYPE == 1
            std::cout << " print_html";
#else
            std::cout << " print_txt";
#endif
            std::cout << "\nCopyright (c) 2019-2020 JackMacWindows. Licensed under the MIT License.\n";
            return 0;
        } else if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h" || std::string(argv[i]) == "-?") {
            std::cout << "Usage: " << argv[0] << " [options...]\n\n"
                      << "General options:\n"
                      << "  --directory <dir>       Sets the directory that stores user data\n"
                      << "  --rom <dir>             Sets the directory that holds the ROM & BIOS\n"
                      << "  -i|--id <id>            Sets the ID of the computer that will launch\n"
                      << "  --script <file>         Sets a script to be run before starting the shell\n"
                      << "  --args \"<args>\"         Sets arguments to be passed to the file in --script\n"
                      << "  -h|-?|--help            Shows this help message\n"
                      << "  -V|--version            Shows the current version\n\n"
                      << "Renderer options:\n"
                      << "  --gui                   Default: Outputs to a GUI terminal\n"
#ifndef NO_CLI
                      << "  -c|--cli                Outputs using an ncurses-based interface\n"
#endif
                      << "  --headless              Outputs only text straight to stdout\n"
                      << "  --raw                   Outputs terminal contents using a binary format\n"
                      << "  --raw-client            Renders raw output from another terminal\n";
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
    else SDL_Init(SDL_INIT_TIMER);
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
    else SDL_Quit();
    return 0;
}
