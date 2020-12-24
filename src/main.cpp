/*
 * main.cpp
 * CraftOS-PC 2
 * 
 * This file handles command-line flags, sets up the runtime, and starts the
 * first computer.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#include "main.hpp"
static int runRenderer();
static void showReleaseNotes();
static void* releaseNotesThread(void* data);
#include <functional>
#include <iomanip>
#include <thread>
#include <Computer.hpp>
#include <configuration.hpp>
#include "peripheral/drive.hpp"
#include "peripheral/speaker.hpp"
#include "platform.hpp"
#include "runtime.hpp"
#include "terminal/CLITerminal.hpp"
#include "terminal/RawTerminal.hpp"
#include "terminal/SDLTerminal.hpp"
#include "terminal/TRoRTerminal.hpp"
#include "terminal/HardwareSDLTerminal.hpp"
#include "termsupport.hpp"
#include <Poco/Checksum.h>
#include <Poco/JSON/Parser.h>
#ifndef __EMSCRIPTEN__
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#else
#include <emscripten/emscripten.h>
#endif
extern "C" {
#include <lualib.h>
}

extern void awaitTasks(const std::function<bool()>& predicate = []()->bool{return true;});
extern void http_server_stop();
extern library_t * libraries[8];
#ifdef WIN32
extern void* kernel32handle;
#endif
#ifdef STANDALONE_ROM
extern FileEntry standaloneROM;
extern FileEntry standaloneDebug;
extern std::string standaloneBIOS;
#endif

int selectedRenderer = -1; // 0 = SDL, 1 = headless, 2 = CLI, 3 = Raw
bool rawClient = false;
std::string overrideHardwareDriver;
std::map<uint8_t, Terminal*> rawClientTerminals;
std::unordered_map<unsigned, uint8_t> rawClientTerminalIDs;
std::string script_file;
std::string script_args;
std::string updateAtQuit;
int returnValue = 0;
std::unordered_map<path_t, std::string> globalPluginErrors;

static void* releaseNotesThread(void* data) {
    Computer * comp = (Computer*)data;
#ifdef __APPLE__
    pthread_setname_np(std::string("Computer " + std::to_string(comp->id) + " Thread").c_str());
#endif
    // seed the Lua RNG
    srand(std::chrono::high_resolution_clock::now().time_since_epoch().count() & UINT_MAX);
    // in case the allocator decides to reuse pointers
    if (freedComputers.find(comp) != freedComputers.end())
        freedComputers.erase(comp);
    try {
#ifdef STANDALONE_ROM
        runComputer(comp, wstr(standaloneDebug["bios.lua"].data));
#else
        runComputer(comp, WS("debug/bios.lua"));
#endif
    } catch (std::exception &e) {
        fprintf(stderr, "Uncaught exception while executing computer %d: %s\n", comp->id, e.what());
        queueTask([e](void*t)->void* {const std::string m = std::string("Uh oh, an uncaught exception has occurred! Please report this to https://www.craftos-pc.cc/bugreport. When writing the report, include the following exception message: \"Exception on computer thread: ") + e.what() + "\". The computer will now shut down.";  if (t != NULL) ((Terminal*)t)->showMessage(SDL_MESSAGEBOX_ERROR, "Uncaught Exception", m.c_str()); else if (selectedRenderer == 0 || selectedRenderer == 5) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Uncaught Exception", m.c_str(), NULL); return NULL; }, comp->term);
        if (comp->L != NULL) {
            comp->event_lock.notify_all();
            for (library_t * lib : libraries) if (lib->deinit != NULL) lib->deinit(comp);
            lua_close(comp->L);   /* Cya, Lua */
            comp->L = NULL;
        }
    }
    freedComputers.insert(comp);
    {
        LockGuard lock(computers);
        for (auto it = computers->begin(); it != computers->end(); ++it) {
            if (*it == comp) {
                it = computers->erase(it);
                queueTask([](void* arg)->void* {delete (Computer*)arg; return NULL;}, comp);
                if (it == computers->end()) break;
            }
        }
    }
    if (selectedRenderer != 0 && selectedRenderer != 2 && selectedRenderer != 5 && !exiting) {
        {LockGuard lock(taskQueue);}
        while (taskQueueReady && !exiting) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        taskQueueReady = true;
        taskQueueNotify.notify_all();
        while (taskQueueReady && !exiting) {std::this_thread::yield(); taskQueueNotify.notify_all();}
    }
    return NULL;
}

static void showReleaseNotes() {
    Computer * comp;
    try {comp = new Computer(-1, true);} catch (std::exception &e) {
        if ((selectedRenderer == 0 || selectedRenderer == 5) && !config.standardsMode) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to open computer", std::string("An error occurred while opening the computer session: " + std::string(e.what()) + ". See https://www.craftos-pc.cc/docs/error-messages for more info.").c_str(), NULL);
        else fprintf(stderr, "An error occurred while opening the computer session: %s", e.what());
        return;
    }
    {
        LockGuard lock(computers);
        computers->push_back(comp);
    }
    if (comp->term != NULL) comp->term->setLabel("Release Notes");
    std::thread * th = new std::thread(releaseNotesThread, comp);
    setThreadName(*th, "Release Note Viewer Thread");
    computerThreads.push_back(th);
}

#if !defined(__EMSCRIPTEN__) && !CRAFTOSPC_INDEV
static void update_thread() {
    try {
        Poco::Net::HTTPSClientSession session("api.github.com", 443, new Poco::Net::Context(Poco::Net::Context::CLIENT_USE, "", Poco::Net::Context::VERIFY_NONE, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"));
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, "/repos/MCJack123/craftos2/releases/latest", Poco::Net::HTTPMessage::HTTP_1_1);
        Poco::Net::HTTPResponse response;
        session.setTimeout(Poco::Timespan(5000000));
        request.add("User-Agent", "CraftOS-PC/" CRAFTOSPC_VERSION " ComputerCraft/" CRAFTOSPC_CC_VERSION);
        session.sendRequest(request);
        Poco::JSON::Parser parser;
        parser.parse(session.receiveResponse(response));
        Poco::JSON::Object::Ptr root = parser.asVar().extract<Poco::JSON::Object::Ptr>();
        if (root->getValue<std::string>("tag_name") != CRAFTOSPC_VERSION) {
#if !(defined(__APPLE__) || defined(WIN32)) && !defined(STANDALONE_ROM)
            SDL_MessageBoxData msg;
            SDL_MessageBoxButtonData buttons[] = {
                {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 3, "Update Now"},
                {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 1, "Ask Me Later"},
                {0, 0, "More Options..."}
            };
            msg.flags = SDL_MESSAGEBOX_INFORMATION;
            msg.window = NULL;
            msg.title = "Update available!";
            const std::string message = (std::string("A new update to CraftOS-PC is available (") + root->getValue<std::string>("tag_name") + " is the latest version, you have " CRAFTOSPC_VERSION "). Would you like to update to the latest version?");
            msg.message = message.c_str();
            msg.numbuttons = 3;
            msg.buttons = buttons;
            msg.colorScheme = NULL;
            int* choicep = (int*)queueTask([ ](void* arg)->void*{int* num = new int; SDL_ShowMessageBox((SDL_MessageBoxData*)arg, num); return num;}, &msg);
            int choice = *choicep;
            delete choicep;
            switch (choice) {
                case 0: {
                    SDL_MessageBoxButtonData buttons2[] = {
                        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 3, "Cancel"},
                        {0, 2, "View Release Notes"},
                        {0, 1, "Skip This Version"},
                        {0, 0, "Update at Quit"}
                    };
                    msg.flags = SDL_MESSAGEBOX_INFORMATION;
                    msg.window = NULL;
                    msg.title = "Update Options";
                    msg.message = "Select an option:";
                    msg.numbuttons = 4;
                    msg.buttons = buttons2;
                    msg.colorScheme = NULL;
                    choicep = (int*)queueTask([ ](void* arg)->void*{int* num = new int; SDL_ShowMessageBox((SDL_MessageBoxData*)arg, num); return num;}, &msg);
                    choice = *choicep;
                    delete choicep;
                    switch (choice) {
                    case 0:
                        updateAtQuit = root->getValue<std::string>("tag_name");
                        return;
                    case 1:
                        config.skipUpdate = CRAFTOSPC_VERSION;
                        config_save();
                        return;
                    case 2:
                        queueTask([](void*)->void*{showReleaseNotes(); return NULL;}, NULL);
                        return;
                    case 3:
                        return;
                    default:
                        exit(choice);
                    }
                } case 1:
                    return;
                case 3:
                    queueTask([root](void*)->void*{updateNow(root->getValue<std::string>("tag_name")); return NULL;}, NULL);
                    return;
                default:
                    // this should never happen
                    exit(choice);
            }
#else
            queueTask([](void* arg)->void*{
                SDL_MessageBoxData msg;
                SDL_MessageBoxButtonData buttons[] = {
                    {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "OK"},
                    {0, 1, "Release Notes"}
                };
                msg.flags = SDL_MESSAGEBOX_INFORMATION;
                msg.window = NULL;
                msg.title = "Update available!";
                msg.message = (const char*)arg;
                msg.numbuttons = 2;
                msg.buttons = buttons;
                msg.colorScheme = NULL;
                int choice = 0;
                SDL_ShowMessageBox(&msg, &choice);
                if (choice == 1) showReleaseNotes();
                return NULL;
            }, (void*)(std::string("A new update to CraftOS-PC is available (") + root->getValue<std::string>("tag_name") + " is the latest version, you have " CRAFTOSPC_VERSION "). Go to " + root->getValue<std::string>("html_url") + " to download the new version.").c_str());
#endif
        }
    } catch (std::exception &e) {
        fprintf(stderr, "Could not check for updates: %s\n", e.what());
    }
}
#endif

inline Terminal * createTerminal(const std::string& title) {
#ifndef NO_CLI
    if (selectedRenderer == 2) return new CLITerminal(title);
    else
#endif
    if (selectedRenderer == 3) return new RawTerminal(title);
    else if (selectedRenderer == 5) return new HardwareSDLTerminal(title);
    else return new SDLTerminal(title);
}


static int runRenderer() {
    if (selectedRenderer == 0) SDLTerminal::init();
    else if (selectedRenderer == 5) HardwareSDLTerminal::init();
    else {
        std::cerr << "Error: Raw client mode requires using a GUI terminal.\n";
        return 3;
    }
    std::thread inputThread([](){
        while (!exiting) {
            unsigned char c1 = (unsigned char)std::cin.get();
            if (c1 == '!' && std::cin.get() == 'C' && std::cin.get() == 'P' && std::cin.get() == 'C') {
                char size[5];
                std::cin.read(size, 4);
                long sizen = strtol(size, NULL, 16);
                char * tmp = new char[(size_t)sizen+1];
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
                uint8_t type = (uint8_t)in.get();
                uint8_t id = (uint8_t)in.get();
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
                        in.seekg(in.tellg()+(std::streamoff)4); // reserved
                        if (term->mode == 0) {
                            unsigned char c = (unsigned char)in.get();
                            unsigned char n = (unsigned char)in.get();
                            for (int y = 0; y < height; y++) {
                                for (int x = 0; x < width; x++) {
                                    term->screen[y][x] = c;
                                    n--;
                                    if (n == 0) {
                                        c = (unsigned char)in.get();
                                        n = (unsigned char)in.get();
                                    }
                                }
                            }
                            for (int y = 0; y < height; y++) {
                                for (int x = 0; x < width; x++) {
                                    term->colors[y][x] = c;
                                    n--;
                                    if (n == 0) {
                                        c = (unsigned char)in.get();
                                        n = (unsigned char)in.get();
                                    }
                                }
                            }
                            in.putback(n);
                            in.putback(c);
                        } else {
                            unsigned char c = (unsigned char)in.get();
                            unsigned char n = (unsigned char)in.get();
                            for (int y = 0; y < height * 9; y++) {
                                for (int x = 0; x < width * 6; x++) {
                                    (*term->currentPixels())[y][x] = c;
                                    n--;
                                    if (n == 0) {
                                        c = (unsigned char)in.get();
                                        n = (unsigned char)in.get();
                                    }
                                }
                            }
                            in.putback(n);
                            in.putback(c);
                        }
                        if (term->mode != 2) {
                            for (int i = 0; i < 16; i++) {
                                term->palette[i].r = (uint8_t)in.get();
                                term->palette[i].g = (uint8_t)in.get();
                                term->palette[i].b = (uint8_t)in.get();
                            }
                        } else {
                            for (int i = 0; i < 256; i++) {
                                term->palette[i].r = (uint8_t)in.get();
                                term->palette[i].g = (uint8_t)in.get();
                                term->palette[i].b = (uint8_t)in.get();
                            }
                        }
                        term->changed = true;
                    }
                    break;
                } case 4: {
                    uint8_t quit = (uint8_t)in.get();
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
                    while ((c = (char)in.get())) title += c;
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
                    while ((c = (char)in.get())) title += c;
                    while ((c = (char)in.get())) message += c;
                    if (rawClientTerminals.find(id) != rawClientTerminals.end()) rawClientTerminals[id]->showMessage(flags, title.c_str(), message.c_str());
                    else if (id == 0) SDL_ShowSimpleMessageBox(flags, title.c_str(), message.c_str(), NULL);
                } default: {}}
            }
        }
    });
    mainLoop();
    inputThread.join();
    for (auto t : rawClientTerminals) delete t.second;
    if (selectedRenderer) HardwareSDLTerminal::quit();
    else SDLTerminal::quit();
    return 0;
}

#ifdef WINDOWS_SUBSYSTEM
#define checkTTY() {SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Unsupported command-line argument", "This build of CraftOS-PC does not support console input/output, which is required for one or more arguments passed to CraftOS-PC. Please use CraftOS-PC_console.exe instead, as this supports console I/O. If it is not present in the install directory, please reinstall CraftOS-PC with the console build option enabled.", NULL); return 5;}
#else
#define checkTTY() 
#endif

int main(int argc, char*argv[]) {
    lualib_debug_ccpc_functions(setcompmask_, db_debug, db_breakpoint, db_unsetbreakpoint);
    lualib_io_ccpc_functions(mounter_fopen_, mounter_fclose_);
#ifdef __EMSCRIPTEN__
    while (EM_ASM_INT(return window.waitingForFilesystemSynchronization ? 1 : 0;)) emscripten_sleep(100);
#endif
    int id = 0;
    bool manualID = false;
    std::string base_path_storage;
    std::string rom_path_storage;
    path_t customDataDir;
    for (int i = 1; i < argc; i++) {
        std::string arg(argv[i]);
        if (arg == "--headless") { selectedRenderer = 1; checkTTY(); } else if (arg == "--gui" || arg == "--sdl" || arg == "--software-sdl") selectedRenderer = 0;
        else if (arg == "--cli" || arg == "-c") { selectedRenderer = 2; checkTTY(); } else if (arg == "--raw") { selectedRenderer = 3; checkTTY(); } else if (arg == "--raw-client") { rawClient = true; checkTTY(); } else if (arg == "--tror") { selectedRenderer = 4; checkTTY(); } else if (arg == "--hardware-sdl" || arg == "--hardware") selectedRenderer = 5;
        else if (arg == "--script") script_file = argv[++i];
        else if (arg.substr(0, 9) == "--script=") script_file = arg.substr(9);
        else if (arg == "--exec") script_file = "\x1b" + std::string(argv[++i]);
        else if (arg == "--args") script_args = argv[++i];
        else if (arg == "--plugin") customPlugins.push_back(wstr(argv[++i]));
        else if (arg == "--directory" || arg == "-d" || arg == "--data-dir") setBasePath(argv[++i]);
        else if (arg.substr(0, 3) == "-d=") setBasePath((base_path_storage = arg.substr(3)).c_str());
        else if (arg == "--computers-dir" || arg == "-C") computerDir = wstr(argv[++i]);
        else if (arg.substr(0, 3) == "-C=") computerDir = wstr(arg.substr(3));
        else if (arg == "--start-dir") customDataDir = wstr(argv[++i]);
        else if (arg.substr(0, 3) == "-c=") customDataDir = wstr(arg.substr(3));
        else if (arg == "--rom") setROMPath(argv[++i]);
#ifdef _WIN32
        else if (arg == "--assets-dir" || arg == "-a") setROMPath((rom_path_storage = std::string(argv[++i]) + "\\assets\\computercraft\\lua").c_str());
        else if (arg.substr(0, 3) == "-a=") setROMPath((rom_path_storage = arg.substr(3) + "\\assets\\computercraft\\lua").c_str());
        else if (arg == "--mc-save") computerDir = getMCSavePath() + wstr(argv[++i]) + WS("\\computer");
#else
        else if (arg == "--assets-dir" || arg == "-a") setROMPath((rom_path_storage = std::string(argv[++i]) + "/assets/computercraft/lua").c_str());
        else if (arg.substr(0, 3) == "-a=") setROMPath((rom_path_storage = arg.substr(3) + "/assets/computercraft/lua").c_str());
        else if (arg == "--mc-save") computerDir = getMCSavePath() + argv[++i] + "/computer";
#endif
        else if (arg == "-i" || arg == "--id") { manualID = true; id = std::stoi(argv[++i]); }
        else if (arg == "--mount" || arg == "--mount-ro" || arg == "--mount-rw") {
            std::string mount_path = argv[++i];
            if (mount_path.find('=') == std::string::npos) {
                std::cerr << "Could not parse mount path string\n";
                return 1;
            }
            customMounts.push_back(std::make_tuple(mount_path.substr(0, mount_path.find('=')), mount_path.substr(mount_path.find('=') + 1), arg == "--mount" ? -1 : (arg == "--mount-rw")));
        } else if (arg == "--renderer" || arg == "-r") {
            if (++i == argc) {
                checkTTY();
                std::cout << "Available renderering methods:\n sdl\n headless\n "
#ifndef NO_CLI
                << "ncurses\n "
#endif
                << "raw\n tror\n hardware-sdl\n";
                for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
                    SDL_RendererInfo rendererInfo;
                    SDL_GetRenderDriverInfo(i, &rendererInfo);
                    printf(" %s\n", rendererInfo.name);
                }
                return 0;
            } else {
                arg = std::string(argv[i]);
                std::transform(arg.begin(), arg.end(), arg.begin(), [](unsigned char c) {return std::tolower(c); });
                if (arg == "sdl" || arg == "awt") selectedRenderer = 0;
                else if (arg == "headless") selectedRenderer = 1;
#ifndef NO_CLI
                else if (arg == "ncurses" || arg == "cli") selectedRenderer = 2;
#endif
                else if (arg == "raw") selectedRenderer = 3;
                else if (arg == "tror") selectedRenderer = 4;
                else if (arg == "hardware-sdl" || arg == "jfx") selectedRenderer = 5;
                else if (arg == "direct3d" || arg == "direct3d11" || arg == "directfb" || arg == "metal" || arg == "opengl" || arg == "opengles" || arg == "opengles2" || arg == "software") {
                    selectedRenderer = 5;
                    overrideHardwareDriver = arg;
                } else {
                    std::cerr << "Unknown renderer type " << arg << "\n";
                    return 1;
                }
            }
        } else if (arg == "-V" || arg == "--version") {
            checkTTY();
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
#if !defined(PRINT_TYPE) || PRINT_TYPE == 0
            std::cout << " print_pdf";
#elif PRINT_TYPE == 1
            std::cout << " print_html";
#else
            std::cout << " print_txt";
#endif
            std::cout << "\nCopyright (c) 2019-2020 JackMacWindows. Licensed under the MIT License.\n";
            return 0;
        } else if (arg == "--help" || arg == "-h" || arg == "-?") {
            checkTTY();
            std::cout << "Usage: " << argv[0] << " [options...]\n\n"
                      << "General options:\n"
                      << "  -d|--directory <dir>             Sets the directory that stores user data\n"
                      << "  --mc-save <name>                 Uses the selected Minecraft save name for computer data\n"
                      << "  --rom <dir>                      Sets the directory that holds the ROM & BIOS\n"
                      << "  -i|--id <id>                     Sets the ID of the computer that will launch\n"
                      << "  --script <file>                  Sets a script to be run before starting the shell\n"
                      << "  --exec <code>                    Sets Lua code to be run before starting the shell\n"
                      << "  --args \"<args>\"                  Sets arguments to be passed to the file in --script\n"
                      << "  --mount[-ro|-rw] <path>=<dir>    Automatically mounts a directory at startup\n"
                      << "    Variants:\n"
                      << "      --mount      Uses default mount_mode in config\n"
                      << "      --mount-ro   Forces mount to be read-only\n"
                      << "      --mount-rw   Forces mount to be read-write\n"
                      << "  -h|-?|--help                     Shows this help message\n"
                      << "  -V|--version                     Shows the current version\n\n"
                      << "Renderer options:\n"
                      << "  --gui                            Default: Outputs to a GUI terminal\n"
#ifndef NO_CLI
                      << "  -c|--cli                         Outputs using an ncurses-based interface\n"
#endif
                      << "  --headless                       Outputs only text straight to stdout\n"
                      << "  --raw                            Outputs terminal contents using a binary format\n"
                      << "  --raw-client                     Renders raw output from another terminal (GUI only)\n"
                      << "  --tror                           Outputs TRoR (terminal redirect over Rednet) packets\n"
                      << "  --hardware                       Outputs to a GUI terminal with hardware acceleration\n\n"
                      << "CCEmuX compatibility options:\n"
                      << "  -a|--assets-dir <dir>            Sets the CC:T directory that holds the ROM & BIOS\n"
                      << "  -C|--computers-dir <dir>         Sets the directory that stores data for each computer\n"
                      << "  -c=|--start-dir <dir>            Sets the directory that holds the startup computer's files\n"
                      << "  -d|--data-dir <dir>              Sets the directory that stores user data\n"
                      << "  --plugin <file>                  Adds an additional plugin to the load list\n"
                      << "  -r|--renderer [renderer]         Lists all available renderers, or selects the renderer\n";
            return 0;
        }
    }
#ifdef NO_CLI
    if (selectedRenderer == 2) {
        std::cerr << "Warning: CraftOS-PC was not built with CLI support, but the --cli flag was specified anyway. Continuing in GUI mode.\n";
        selectedRenderer = 0;
    }
#endif
#ifdef _WIN32
    if (computerDir.empty()) computerDir = getBasePath() + WS("\\computer");
#else
    if (computerDir.empty()) computerDir = getBasePath() + WS("/computer");
#endif
    if (!customDataDir.empty()) customDataDirs[id] = customDataDir;
    setupCrashHandler();
    migrateData();
    config_init();
    if (selectedRenderer == -1) selectedRenderer = config.useHardwareRenderer ? 5 : 0;
    if (rawClient) return runRenderer();
#ifndef NO_CLI
    if (selectedRenderer == 2) CLITerminal::init();
    else 
#endif
    if (selectedRenderer == 3) RawTerminal::init();
    else if (selectedRenderer == 0) SDLTerminal::init();
    else if (selectedRenderer == 4) TRoRTerminal::init();
    else if (selectedRenderer == 5) HardwareSDLTerminal::init();
    else SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO);
    driveInit();
#ifndef NO_MIXER
    speakerInit();
#endif
    globalPluginErrors = initializePlugins();
#if !defined(__EMSCRIPTEN__) && !CRAFTOSPC_INDEV
    if ((selectedRenderer == 0 || selectedRenderer == 5) && config.checkUpdates && config.skipUpdate != CRAFTOSPC_VERSION) 
        std::thread(update_thread).detach();
#endif
    startComputer(manualID ? id : config.initialComputer);
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 60, 1);
    return 0;
#else
    try {
        mainLoop();
    } catch (std::exception &e) {
        fprintf(stderr, "Uncaught exception on main thread: %s\n", e.what());
        if (selectedRenderer == 0 || selectedRenderer == 5) queueTask([e](void*t)->void* {SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Uncaught Exception", (std::string("Uh oh, CraftOS-PC has crashed! Please report this to https://www.craftos-pc.cc/bugreport. When writing the report, include the following exception message: \"Exception on main thread: ") + e.what() + "\". CraftOS-PC will now close.").c_str(), NULL); return NULL; }, NULL);
        for (Computer * c : *computers) {
            c->running = 0;
            c->event_lock.notify_all();
        }
        exiting = true;
        awaitTasks([]()->bool {return computers.locked() || !computers->empty() || !taskQueue->empty();});
    }
#endif
    for (std::thread *t : computerThreads) { if (t->joinable()) {t->join(); delete t;} }
    // C++ doesn't like it if we try to empty the SDL event list once the plugins are gone
    SDLTerminal::eventHandlers.clear();
    deinitializePlugins();
#ifndef NO_MIXER
    speakerQuit();
#endif
    driveQuit();
    http_server_stop();
    config_save();
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
    else if (selectedRenderer == 4) TRoRTerminal::quit();
    else if (selectedRenderer == 5) HardwareSDLTerminal::quit();
    else SDL_Quit();
#ifdef WIN32
    if (kernel32handle != NULL) SDL_UnloadObject(kernel32handle);
#endif
    return returnValue;
}
