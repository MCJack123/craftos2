/*
 * main.cpp
 * CraftOS-PC 2
 * 
 * This file handles command-line flags, sets up the runtime, and starts the
 * first computer.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#include "main.hpp"
static int runRenderer(const std::function<std::string()>& read, const std::function<void(const std::string&)>& write);
static void showReleaseNotes();
static void* releaseNotesThread(void* data);
#include <functional>
#include <fstream>
#include <iomanip>
#include <thread>
#include <Computer.hpp>
#include <configuration.hpp>
#include <sys/stat.h>
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
#include <Poco/Version.h>
#include <Poco/URI.h>
#include <Poco/Checksum.h>
#include <Poco/JSON/Parser.h>
#include <Poco/JSON/JSONException.h>
#ifndef __EMSCRIPTEN__
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/WebSocket.h>
#else
#include <emscripten/emscripten.h>
#endif
extern "C" {
#include <lualib.h>
}

using namespace Poco::Net;

#ifdef __ANDROID__
extern "C" {extern int Android_JNI_SetupThread(void);}
#endif

#ifdef _WIN32
extern void uploadCrashDumps();
#endif

extern void awaitTasks(const std::function<bool()>& predicate = []()->bool{return true;});
extern void http_server_stop();
extern void clearPeripherals();
extern library_t * libraries[];
extern int onboardingMode;
extern std::function<void(const std::string&)> rawWriter;
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
static std::string rawWebSocketURL;

#if !defined(__EMSCRIPTEN__) && !CRAFTOSPC_INDEV
Poco::JSON::Object updateAtQuitRoot;
static void* releaseNotesThread(void* data) {
    Computer * comp = (Computer*)data;
#ifdef __APPLE__
    pthread_setname_np(std::string("Computer " + std::to_string(comp->id) + " Thread").c_str());
#endif
#ifdef __ANDROID__
    Android_JNI_SetupThread();
#endif
    // seed the Lua RNG
    srand(std::chrono::high_resolution_clock::now().time_since_epoch().count() & UINT_MAX);
    // in case the allocator decides to reuse pointers
    if (freedComputers.find(comp) != freedComputers.end())
        freedComputers.erase(comp);
    try {
#ifdef STANDALONE_ROM
        runComputer(comp, "debug/bios.lua", standaloneDebug["bios.lua"].data);
#else
        runComputer(comp, "debug/bios.lua");
#endif
    } catch (std::exception &e) {
        fprintf(stderr, "Uncaught exception while executing computer %d (last C function: %s): %s\n", comp->id, lastCFunction, e.what());
        queueTask([e](void*t)->void* {const std::string m = std::string("Uh oh, an uncaught exception has occurred! Please report this to https://www.craftos-pc.cc/bugreport. When writing the report, include the following exception message: \"Exception on computer thread: ") + e.what() + "\". The computer will now shut down.";  if (t != NULL) ((Terminal*)t)->showMessage(SDL_MESSAGEBOX_ERROR, "Uncaught Exception", m.c_str()); else if (selectedRenderer == 0 || selectedRenderer == 5) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Uncaught Exception", m.c_str(), NULL); return NULL; }, comp->term);
        if (comp->L != NULL) {
            comp->event_lock.notify_all();
            for (library_t ** lib = libraries; *lib != NULL; lib++) if ((*lib)->deinit != NULL) (*lib)->deinit(comp);
            if (comp->eventTimeout != 0) SDL_RemoveTimer(comp->eventTimeout);
            comp->eventTimeout = 0;
            lua_close(comp->L);   /* Cya, Lua */
            comp->L = NULL;
            if (comp->rawFileStack) {
                std::lock_guard<std::mutex> lock(comp->rawFileStackMutex);
                lua_close(comp->rawFileStack);
                comp->rawFileStack = NULL;
            }
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

static void update_thread() {
    try {
        Context::Ptr ctx = new Context(Context::CLIENT_USE, "", Context::VERIFY_RELAXED, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
        addSystemCertificates(ctx);
        HTTPSClientSession session("api.github.com", 443, ctx);
        if (!config.http_proxy_server.empty()) session.setProxy(config.http_proxy_server, config.http_proxy_port);
        HTTPRequest request(HTTPRequest::HTTP_GET, "/repos/MCJack123/craftos2/releases/latest", HTTPMessage::HTTP_1_1);
        HTTPResponse response;
        session.setTimeout(Poco::Timespan(5000000));
        request.add("User-Agent", "CraftOS-PC/" CRAFTOSPC_VERSION " ComputerCraft/" CRAFTOSPC_CC_VERSION);
        session.sendRequest(request);
        Poco::JSON::Parser parser;
        parser.parse(session.receiveResponse(response));
        Poco::JSON::Object::Ptr root = parser.asVar().extract<Poco::JSON::Object::Ptr>();
        if (root->getValue<std::string>("tag_name") != CRAFTOSPC_VERSION) {
#if (defined(__APPLE__) || defined(WIN32)) && !defined(STANDALONE_ROM)
            SDL_MessageBoxData msg;
            SDL_MessageBoxButtonData buttons[] = {
                {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 3, "Update at Quit"},
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
            int* choicep = (int*)queueTask([](void* arg)->void* {int* num = new int; SDL_ShowMessageBox((SDL_MessageBoxData*)arg, num); return num; }, &msg);
            int choice = *choicep;
            delete choicep;
            switch (choice) {
            case 0: {
                SDL_MessageBoxButtonData buttons2[] = {
                    {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 3, "Cancel"},
                    {0, 2, "View Release Notes"},
                    {0, 1, "Skip This Version"},
                    {0, 0, "Update Now"}
                };
                msg.flags = SDL_MESSAGEBOX_INFORMATION;
                msg.window = NULL;
                msg.title = "Update Options";
                msg.message = "Select an option:";
                msg.numbuttons = 4;
                msg.buttons = buttons2;
                msg.colorScheme = NULL;
                choicep = (int*)queueTask([](void* arg)->void* {int* num = new int; SDL_ShowMessageBox((SDL_MessageBoxData*)arg, num); return num; }, &msg);
                choice = *choicep;
                delete choicep;
                switch (choice) {
                case 0:
                    queueTask([root](void*)->void* {updateNow(root->getValue<std::string>("tag_name"), root); return NULL; }, NULL);
                    return;
                case 1:
                    config.skipUpdate = CRAFTOSPC_VERSION;
                    config_save();
                    return;
                case 2:
                    queueTask([](void*)->void* {showReleaseNotes(); return NULL; }, NULL);
                    return;
                case 3:
                    return;
                default:
                    exit(choice);
                }
            } case 1:
                return;
            case 3:
                updateAtQuit = root->getValue<std::string>("tag_name");
                updateAtQuitRoot = *root;
                return;
            default:
                // this should never happen
                exit(choice);
            }
#else
            queueTask([](void* arg)->void* {
                SDL_MessageBoxData msg;
                SDL_MessageBoxButtonData buttons[] = {
                    {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "OK"},
                    {0, 1, "Release Notes"},
                    {0, 2, "Don't Ask Again"}
                };
                msg.flags = SDL_MESSAGEBOX_INFORMATION;
                msg.window = NULL;
                msg.title = "Update available!";
                msg.message = (const char*)arg;
                msg.numbuttons = 3;
                msg.buttons = buttons;
                msg.colorScheme = NULL;
                int choice = 0;
                SDL_ShowMessageBox(&msg, &choice);
                if (choice == 1) showReleaseNotes();
                else if (choice == 2) {
                    config.skipUpdate = CRAFTOSPC_VERSION;
                    config_save();
                }
                return NULL;
            }, (void*)(std::string("A new update to CraftOS-PC is available (") + root->getValue<std::string>("tag_name") + " is the latest version, you have " CRAFTOSPC_VERSION "). Go to " + root->getValue<std::string>("html_url") + " to download the new version.").c_str());
#endif
        }
    } catch (Poco::Exception &e) {
        fprintf(stderr, "Could not check for updates: %s\n", e.message().c_str());
    } catch (std::exception &e) {
        fprintf(stderr, "Could not check for updates: %s\n", e.what());
    }
}
#endif

static int runRenderer(const std::function<std::string()>& read, const std::function<void(const std::string&)>& write) {
    if (selectedRenderer == 0) SDLTerminal::init();
    else if (selectedRenderer == 5) HardwareSDLTerminal::init();
    else {
        std::cerr << "Error: Raw client mode requires using a GUI terminal.\n";
        return 3;
    }
    rawWriter = write;
    std::thread inputThread([read](){
        while (!exiting) {
            std::string data = read();
            if (data.empty()) {
                exiting = true;
                break;
            }
            long sizen;
            size_t off = 8;
            if (data[3] == 'C') sizen = std::stol(data.substr(4, 4), nullptr, 16);
            else if (data[3] == 'D') {sizen = std::stol(data.substr(4, 12), nullptr, 16); off = 16;}
            else continue;
            std::string ddata = b64decode(data.substr(off, sizen));
            Poco::Checksum chk;
            if (RawTerminal::supportedFeatures & CCPC_RAW_FEATURE_FLAG_BINARY_CHECKSUM) chk.update(ddata);
            else chk.update(data.substr(off, sizen));
            if (chk.checksum() != std::stoul(data.substr(sizen + off, 8), NULL, 16)) {
                fprintf(stderr, "Invalid checksum: expected %08X, got %08lX\n", chk.checksum(), std::stoul(data.substr(sizen + off, 8), NULL, 16));
                continue;
            }
            std::stringstream in(ddata);
            uint8_t type = (uint8_t)in.get();
            uint8_t id = (uint8_t)in.get();
            switch (type) {
            case CCPC_RAW_TERMINAL_DATA: {
                if (rawClientTerminals.find(id) != rawClientTerminals.end()) {
                    Terminal * term = rawClientTerminals[id];
                    term->mode = in.get();
                    term->canBlink = in.get();
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
                                term->pixels[y][x] = c;
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
            } case CCPC_RAW_TERMINAL_CHANGE: {
                uint8_t quit = (uint8_t)in.get();
                if (quit == 1) {
                    queueTask([id](void*)->void*{
                        rawClientTerminalIDs.erase(rawClientTerminals[id]->id);
                        rawClientTerminals[id]->factory->deleteTerminal(rawClientTerminals[id]);
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
            } case CCPC_RAW_MESSAGE_DATA: {
                uint32_t flags = 0;
                std::string title, message;
                char c;
                in.read((char*)&flags, 4);
                while ((c = (char)in.get())) title += c;
                while ((c = (char)in.get())) message += c;
                if (rawClientTerminals.find(id) != rawClientTerminals.end()) rawClientTerminals[id]->showMessage(flags, title.c_str(), message.c_str());
                else if (id == 0) SDL_ShowSimpleMessageBox(flags, title.c_str(), message.c_str(), NULL);
            } case CCPC_RAW_FEATURE_FLAGS: {
                uint16_t f = 0;
                uint32_t ef = 0;
                in.read((char*)&f, 2);
                if (f & CCPC_RAW_FEATURE_FLAG_HAS_EXTENDED_FEATURES) in.read((char*)&ef, 4);
                RawTerminal::supportedFeatures = f & (CCPC_RAW_FEATURE_FLAG_BINARY_CHECKSUM | CCPC_RAW_FEATURE_FLAG_FILESYSTEM_SUPPORT | CCPC_RAW_FEATURE_FLAG_SEND_ALL_WINDOWS);
                RawTerminal::supportedExtendedFeatures = ef & (0x00000000);
            }}
            std::this_thread::yield();
        }
    });
    setThreadName(inputThread, "Input Thread");
    RawTerminal::initClient(CCPC_RAW_FEATURE_FLAG_BINARY_CHECKSUM | CCPC_RAW_FEATURE_FLAG_SEND_ALL_WINDOWS);
    mainLoop();
    inputThread.join();
    for (auto t : rawClientTerminals) t.second->factory->deleteTerminal(t.second);
    if (selectedRenderer) HardwareSDLTerminal::quit();
    else SDLTerminal::quit();
    return 0;
}

#define migrateSetting(oldname, newname, type) if (oldroot.isMember(oldname)) config.newname = oldroot[oldname].as##type()

static void migrateData(bool forced) {
    migrateOldData();
    if ((forced || !fs::exists(getBasePath())) && fs::exists(getBasePath().parent_path() / "ccemux")) {
        if (!forced) {
            SDL_MessageBoxData data;
            data.title = "Migrate Data";
            data.message = "An existing installation of CCEmuX has been detected. Would you like to migrate your data from that installation? The old data will not be deleted. (You can do this at any time with the '--migrate' flag.)";
            data.colorScheme = NULL;
            data.window = NULL;
            data.flags = SDL_MESSAGEBOX_INFORMATION;
            SDL_MessageBoxButtonData buttons[2] = {
                {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No"},
                {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes"}
            };
            data.numbuttons = 2;
            data.buttons = buttons;
            int retval = 0;
            SDL_ShowMessageBox(&data, &retval);
            if (!retval) return;
        }
        // Copy computer data
        std::error_code copy_error;
        std::string msg;
        fs::copy(getBasePath().parent_path() / "ccemux" / "computer", getBasePath() / "computer", fs::copy_options::recursive | fs::copy_options::update_existing, copy_error);
        if (copy_error) msg += "Could not copy files: " + copy_error.message() + "\n";
        // Copy config file
        std::ifstream in(getBasePath().parent_path() / "ccemux" / "ccemux.json");
        if (in.is_open()) {
            Value oldroot;
            Poco::JSON::Object::Ptr p;
            try {
                config_init();
                p = oldroot.parse(in);
                migrateSetting("maximumFilesOpen", maximumFilesOpen, Int);
                migrateSetting("maxComputerCapacity", computerSpaceLimit, Int);
                migrateSetting("httpEnable", http_enable, Bool);
                migrateSetting("disableLua51Features", disable_lua51_features, Bool);
                migrateSetting("defaultComputerSettings", default_computer_settings, String);
                migrateSetting("termWidth", defaultWidth, Int);
                migrateSetting("termHeight", defaultHeight, Int);
                if (oldroot.isMember("httpWhitelist")) {
                    config.http_whitelist = std::vector<std::string>();
                    for (auto it = oldroot["httpWhitelist"].arrayBegin(); it != oldroot["httpWhitelist"].arrayEnd(); ++it) config.http_whitelist.push_back(it->convert<std::string>());
                }
                if (oldroot.isMember("httpBlacklist")) {
                    config.http_blacklist = std::vector<std::string>();
                    for (auto it = oldroot["httpBlacklist"].arrayBegin(); it != oldroot["httpBlacklist"].arrayEnd(); ++it) config.http_blacklist.push_back(it->convert<std::string>());
                }
                if (oldroot.isMember("plugins") && oldroot["plugins"].isMember("net.clgd.ccemux.plugins.builtin.HDFontPlugin") && oldroot["plugins"]["net.clgd.ccemux.plugins.builtin.HDFontPlugin"].isMember("enabled") && !oldroot["plugins"]["net.clgd.ccemux.plugins.builtin.HDFontPlugin"]["enabled"].asBool())
                    config.customFontPath = "";
                else config.customFontPath = "hdfont";
                if (oldroot.isMember("http")) {
                    oldroot = oldroot["http"];
                    migrateSetting("websocketEnabled", http_websocket_enabled, Bool);
                    migrateSetting("max_requests", http_max_requests, Int);
                    migrateSetting("max_websockets", http_max_websockets, Int);
                }
                config_save();
            } catch (Poco::JSON::JSONException &e) {
                fprintf(stderr, "Could not read CCEmuX config file: %s\n", e.displayText().c_str());
                msg += "Could not parse " + (getBasePath().parent_path() / "ccemux" / "ccemux.json").string() + "\n";
            }
        } else msg += "Could not find " + (getBasePath().parent_path() / "ccemux" / "ccemux.json").string() + "\n";
        if (!msg.empty()) {
            fprintf(stderr, "Some errors occurred while copying CCEmuX data:\n%s", msg.c_str());
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Migration Failure", "Some files failed to be copied while migrating from CCEmuX. Check the console to see what failed.\n", NULL);
        } else if (forced) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Migration Success", "The migration of CCEmuX data to CraftOS-PC has completed successfully.", NULL);
    }
}

#ifdef WINDOWS_SUBSYSTEM
#define checkTTY() {SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Unsupported command-line argument", "This build of CraftOS-PC does not support console input/output, which is required for one or more arguments passed to CraftOS-PC. Please use CraftOS-PC_console.exe instead, as this supports console I/O. If it is not present in the install directory, please reinstall CraftOS-PC with the console build option enabled.", NULL); return 5;}
#else
#define checkTTY() 
#endif

static int id = 0;
static bool manualID = false;
static bool forceMigrate = false;
static path_t customDataDir;
static std::vector<std::pair<std::string, std::string>> configOptions;

#define setConfigSettingB(n) else if (strcmp(name, #n) == 0) config.n = strcasecmp(value, "true") == 0
#define setConfigSettingI(n) else if (strcmp(name, #n) == 0) config.n = atoi(value)

static void setConfigOption(const char * name, const char * value) {
    bool isUserConfig = false;
    if (strcmp(name, "http_enable") == 0)
        config.http_enable = strcasecmp(value, "true") == 0;
    else if (strcmp(name, "debug_enable") == 0) ; // do nothing
    else if (strcmp(name, "mount_mode") == 0) {
        if (strcmp(value, "none") == 0) config.mount_mode = MOUNT_MODE_NONE;
        else if (strcmp(value, "ro strict") == 0 || strcmp(value, "ro_strict") == 0) config.mount_mode = MOUNT_MODE_RO_STRICT;
        else if (strcmp(value, "ro") == 0) config.mount_mode = MOUNT_MODE_RO;
        else if (strcmp(value, "rw") == 0) config.mount_mode = MOUNT_MODE_RW;
        else config.mount_mode = atoi(value);
    } setConfigSettingB(disable_lua51_features);
    else if (strcmp(name, "default_computer_settings") == 0)
        config.default_computer_settings = value;
    setConfigSettingB(logErrors);
    setConfigSettingI(computerSpaceLimit);
    setConfigSettingI(maximumFilesOpen);
    setConfigSettingI(maxNotesPerTick);
    setConfigSettingI(clockSpeed);
    setConfigSettingB(showFPS);
    setConfigSettingI(abortTimeout);
    setConfigSettingB(ignoreHotkeys);
    setConfigSettingB(checkUpdates);
    setConfigSettingB(vanilla);
    setConfigSettingI(initialComputer);
    setConfigSettingI(maxRecordingTime);
    setConfigSettingI(recordingFPS);
    setConfigSettingI(maxOpenPorts);
    setConfigSettingI(mouse_move_throttle);
    setConfigSettingB(monitorsUseMouseEvents);
    setConfigSettingI(defaultWidth);
    setConfigSettingI(defaultHeight);
    setConfigSettingB(standardsMode);
    setConfigSettingB(useHardwareRenderer);
    else if (strcmp(name, "preferredHardwareDriver") == 0)
        config.preferredHardwareDriver = value;
    setConfigSettingB(useVsync);
    setConfigSettingB(http_websocket_enabled);
    setConfigSettingI(http_max_websockets);
    setConfigSettingI(http_max_websocket_message);
    setConfigSettingI(http_max_requests);
    setConfigSettingI(http_max_upload);
    setConfigSettingI(http_max_download);
    setConfigSettingI(http_timeout);
    setConfigSettingB(extendMargins);
    setConfigSettingB(snapToSize);
    setConfigSettingB(snooperEnabled);
    setConfigSettingB(keepOpenOnShutdown);
    setConfigSettingB(useWebP);
    setConfigSettingB(dropFilePath);
    setConfigSettingB(useDFPWM);
    else if (strcmp(name, "useHDFont") == 0)
        config.customFontPath = strcasecmp(value, "true") == 0 ? "hdfont" : "";
    else if (strcmp(name, "http_whitelist") == 0) {
        // ?
    } else if (strcmp(name, "http_blacklist") == 0) {
        // ?
    } else if (userConfig.find(name) != userConfig.end()) {
        isUserConfig = true;
        switch (std::get<0>(userConfig[name])) {
            case 0: config.pluginData[name] = strcasecmp(value, "true") == 0 ? "true" : "false"; break;
            case 1: config.pluginData[name] = std::to_string(atoi(value)); break;
            case 2: config.pluginData[name] = value; break;
            case 3: fprintf(stderr, "Invalid type for option '%s'\n", name); break; // maybe fix this later?
        }
    } else fprintf(stderr, "Unknown configuration option '%s'\n", name);
}

int parseArguments(const std::vector<std::string>& argv) {
    for (int i = 0; i < argv.size(); i++) {
        std::string arg = argv[i];
        if (arg == "--headless") { selectedRenderer = 1; checkTTY(); }
        else if (arg == "--gui" || arg == "--sdl" || arg == "--software-sdl") selectedRenderer = 0;
        else if (arg == "--cli" || arg == "-c") { selectedRenderer = 2; checkTTY(); }
        else if (arg == "--raw") { selectedRenderer = 3; checkTTY(); }
        else if (arg == "--raw-client") { rawClient = true; checkTTY(); }
        else if (arg == "--raw-websocket") { rawClient = true; rawWebSocketURL = argv[++i]; }
        else if (arg.substr(0, 16) == "--raw-websocket=") { rawClient = true; rawWebSocketURL = arg.substr(16); }
        else if (arg == "--tror") { selectedRenderer = 4; checkTTY(); }
        else if (arg == "--hardware-sdl" || arg == "--hardware") selectedRenderer = 5;
        else if (arg == "--single") singleWindowMode = true;
        else if (arg == "--script") script_file = argv[++i];
        else if (arg.substr(0, 9) == "--script=") script_file = arg.substr(9);
        else if (arg == "--exec") script_file = "\x1b" + argv[++i];
        else if (arg == "--args") script_args = argv[++i];
        else if (arg == "--plugin") customPlugins.push_back(argv[++i]);
        else if (arg == "--directory" || arg == "-d" || arg == "--data-dir") setBasePath(argv[++i]);
        else if (arg.substr(0, 3) == "-d=") setBasePath(arg.substr(3));
        else if (arg == "--computers-dir" || arg == "-C") computerDir = argv[++i];
        else if (arg.substr(0, 3) == "-C=") computerDir = arg.substr(3);
        else if (arg == "--start-dir") customDataDir = argv[++i];
        else if (arg.substr(0, 3) == "-c=") customDataDir = arg.substr(3);
        else if (arg == "--rom") setROMPath(argv[++i].c_str());
        else if (arg == "--assets-dir" || arg == "-a") setROMPath(path_t(argv[++i])/"assets"/"computercraft"/"lua");
        else if (arg.substr(0, 3) == "-a=") setROMPath(path_t(arg.substr(3))/"assets"/"computercraft"/"lua");
        else if (arg == "--mc-save") computerDir = getMCSavePath() / argv[++i] / "computer";
        else if (arg == "-i" || arg == "--id") {
            manualID = true;
            try {id = std::stoi(argv[++i]);}
            catch (std::out_of_range &e) {
                std::cerr << "Error: Computer ID is out of range\n";
                return 1;
            }
        } else if (arg == "-o" || arg == "--option") {
            std::string val = argv[++i];
            size_t pos = val.find('=');
            if (pos != std::string::npos) {
                configOptions.push_back(std::make_pair(val.substr(0, pos), val.substr(pos + 1)));
            } else {
                std::cerr << "Invalid parameter passed to --option\n";
                return 1;
            }
        } else if (arg == "--migrate") forceMigrate = true;
        else if (arg == "--mount" || arg == "--mount-ro" || arg == "--mount-rw") {
            std::string mount_path = argv[++i];
            if (mount_path.find('=') == std::string::npos) {
                std::cerr << "Could not parse mount path string\n";
                return 1;
            }
            customMounts.push_back(std::make_tuple(mount_path.substr(0, mount_path.find('=')), mount_path.substr(mount_path.find('=') + 1), arg == "--mount" ? -1 : (arg == "--mount-rw")));
        } else if (arg == "--renderer" || arg == "-r") {
            if (++i == argv.size()) {
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
                arg = argv[i];
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
                } else if (std::stoi(arg)) selectedRenderer = std::stoi(arg);
                else {
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
#ifndef NO_WEBP
            std::cout << " webp";
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
            std::cout << "\nCopyright (c) 2019-2024 JackMacWindows. Licensed under the MIT License.\n";
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
                      << "  -o|--option <name>=<value>       Sets a global config option before starting (can be used multiple times)\n"
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
                      << "  --raw-websocket <url>            Like --raw-client, but connects to a WebSocket server\n"
                      << "  --tror                           Outputs TRoR (terminal redirect over Rednet) packets\n"
                      << "  --hardware                       Outputs to a GUI terminal with hardware acceleration\n"
                      << "  --single                         Forces all screen output to a single window\n\n"
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
    return -1;
}

int main(int argc, char*argv[]) {
    lualib_debug_ccpc_functions(setcompmask_, db_debug, db_breakpoint, db_unsetbreakpoint);
#ifdef __EMSCRIPTEN__
    while (EM_ASM_INT(return window.waitingForFilesystemSynchronization ? 1 : 0;)) emscripten_sleep(100);
#endif
    std::vector<std::string> args;
    for (int i = 1; i < argc; i++) args.push_back(std::string(argv[i]));
    int res = parseArguments(args);
    if (res >= 0) return res;
#ifdef NO_CLI
    if (selectedRenderer == 2) {
        std::cerr << "Warning: CraftOS-PC was not built with CLI support, but the --cli flag was specified anyway. Continuing in GUI mode.\n";
        selectedRenderer = 0;
    }
#endif
    if (computerDir.empty()) computerDir = getBasePath() / "computer";
    if (!customDataDir.empty()) customDataDirs[id] = customDataDir;
    mainThreadID = std::this_thread::get_id();
    setupCrashHandler();
    migrateData(forceMigrate);
    config_init();
    if (!configOptions.empty()) {
        for (const auto& opt : configOptions) setConfigOption(opt.first.c_str(), opt.second.c_str());
        config_save();
    }
    if (selectedRenderer == -1) selectedRenderer = config.useHardwareRenderer ? 5 : 0;
    if (rawClient) {
        if (!rawWebSocketURL.empty()) {
            Poco::URI uri;
            try {
                uri = Poco::URI(rawWebSocketURL);
            } catch (Poco::SyntaxException &e) {
                std::cerr << "Could not connect to WebSocket: URL malformed\n";
                return 6;
            }
            if (uri.getHost() == "localhost") uri.setHost("127.0.0.1");
            HTTPClientSession * cs;
            if (uri.getScheme() == "ws") cs = new HTTPClientSession(uri.getHost(), uri.getPort());
            else if (uri.getScheme() == "wss") {
                Context::Ptr ctx = new Context(Context::CLIENT_USE, "", Context::VERIFY_RELAXED, 9, true, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
                addSystemCertificates(ctx);
                cs = new HTTPSClientSession(uri.getHost(), uri.getPort(), ctx);
            } else {
                std::cerr << "Could not connect to WebSocket: Invalid scheme '" + uri.getScheme() + "'\n";
                return 6;
            }
            if (uri.getPathAndQuery().empty()) uri.setPath("/");
            if (!config.http_proxy_server.empty()) cs->setProxy(config.http_proxy_server, config.http_proxy_port);
            HTTPRequest request(HTTPRequest::HTTP_GET, uri.getPathAndQuery(), HTTPMessage::HTTP_1_1);
            request.add("User-Agent", "computercraft/" CRAFTOSPC_CC_VERSION " CraftOS-PC/" CRAFTOSPC_VERSION);
            request.add("Accept-Charset", "UTF-8");
            HTTPResponse response;
            WebSocket* ws;
            try {
                ws = new WebSocket(*cs, request, response);
            } catch (Poco::Exception &e) {
                std::cerr << "Could not connect to WebSocket: " << e.displayText() << "\n";
                return 6;
            } catch (std::exception &e) {
                std::cerr << "Could not connect to WebSocket: " << e.what() << "\n";
                return 6;
            }
            ws->setReceiveTimeout(Poco::Timespan(1, 0));
#if POCO_VERSION >= 0x01090100
            ws->setMaxPayloadSize(65536);
#endif
            bool open = true;
            int retval = runRenderer([ws, &open]()->std::string {
                char buf[65536];
                while (open) {
                    int flags = 0;
                    int res;
                    try {
                        res = ws->receiveFrame(buf, 65536, flags);
                        if (res == 0) {
                            open = false;
                            break;
                        }
                    } catch (Poco::TimeoutException &e) {
                        continue;
                    } catch (NetException &e) {
                        open = false;
                        break;
                    }
                    if (flags & WebSocket::FRAME_OP_CLOSE) {
                        open = false;
                        break;
                    } else {
                        return std::string(buf, res);
                    }
                }
                return "";
            }, [ws](const std::string& data) {
                ws->sendFrame(data.c_str(), data.size());
            });
            open = false;
            try {ws->shutdown();} catch (...) {}
            delete ws;
            delete cs;
            return retval;
        } else return runRenderer([]()->std::string {
            while (true) {
                unsigned char c1 = (unsigned char)std::cin.get();
                if (c1 == '!' && std::cin.get() == 'C' && std::cin.get() == 'P' && std::cin.get() == 'C') {
                    char size[5];
                    std::cin.read(size, 4);
                    const long sizen = strtol(size, NULL, 16);
                    char * tmp = new char[(size_t)sizen+10];
                    std::cin.read(tmp, sizen + 9);
                    std::string retval = "!CPC" + std::string(size, 4) + std::string(tmp, sizen + 9);
                    if (tmp[sizen + 8] == '\r') retval += '\n';
                    delete[] tmp;
                    return retval;
                }
            }
        }, [](const std::string& str) {
            std::cout << str;
            std::cout.flush();
        });
    }
    preloadPlugins();
    TerminalFactory * factory = selectedRenderer >= terminalFactories.size() ? NULL : terminalFactories[selectedRenderer];
    try {
        if (factory) factory->init();
        else SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO);
    } catch (std::exception &e) {
        if (selectedRenderer == 0 || selectedRenderer == 5) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to initialize renderer", ("An error occurred while initializing the renderer: " + std::string(e.what()) + ". See https://www.craftos-pc.cc/docs/error-messages for more info. CraftOS-PC will now close").c_str(), NULL);
        else fprintf(stderr, "An error occurred while initializing the renderer: %s. See https://www.craftos-pc.cc/docs/error-messages for more info. CraftOS-PC will now close.\n", e.what());
        SDL_Quit();
        return 2;
    }
    driveInit();
#ifndef NO_MIXER
    speakerInit();
#endif
    globalPluginErrors = initializePlugins();
#if !defined(__EMSCRIPTEN__) && !defined(__IPHONEOS__) && !CRAFTOSPC_INDEV
    if ((selectedRenderer == 0 || selectedRenderer == 5) && config.checkUpdates && config.skipUpdate != CRAFTOSPC_VERSION) 
        std::thread(update_thread).detach();
#endif
#if defined(_WIN32) && defined(CRASHREPORT_API_KEY)
    if (onboardingMode == 1 && !config.snooperEnabled && (selectedRenderer == 0 || selectedRenderer == 5)) {
        SDL_MessageBoxData data;
        data.title = "Allow analytics?";
        data.message = "CraftOS-PC can automatically upload crash logs to help bugs get fixed. These files are sent anonymously and don't contain direct personal data, but they do include general system information (see https://www.craftos-pc.cc/docs/privacy for more info). Would you like to allow crash logs to be uploaded?";
        data.colorScheme = NULL;
        data.window = NULL;
        data.flags = SDL_MESSAGEBOX_INFORMATION;
        SDL_MessageBoxButtonData buttons[2] = {
            {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Deny"},
            {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Allow"}
        };
        data.numbuttons = 2;
        data.buttons = buttons;
        int res = 0;
        SDL_ShowMessageBox(&data, &res);
        config.snooperEnabled = res;
    }
    uploadCrashDumps();
#endif
    startComputer(manualID ? id : config.initialComputer);
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainLoop, 0, false);
    return 0;
#else
    try {
        mainLoop();
    } catch (Poco::Exception &e) {
        fprintf(stderr, "Uncaught exception on main thread: %s\n", e.displayText().c_str());
        if (selectedRenderer == 0 || selectedRenderer == 5) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Uncaught Exception", ("Uh oh, CraftOS-PC has crashed! Please report this to https://www.craftos-pc.cc/bugreport. When writing the report, include the following exception message: \"Poco exception on main thread: " + e.displayText() + "\". CraftOS-PC will now close.").c_str(), NULL);
        for (Computer * c : *computers) {
            c->running = 0;
            c->event_lock.notify_all();
        }
        exiting = true;
        awaitTasks([]()->bool {return computers.locked() || !computers->empty() || !taskQueue->empty();});
    } catch (std::exception &e) {
        fprintf(stderr, "Uncaught exception on main thread: %s\n", e.what());
        if (selectedRenderer == 0 || selectedRenderer == 5) SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Uncaught Exception", (std::string("Uh oh, CraftOS-PC has crashed! Please report this to https://www.craftos-pc.cc/bugreport. When writing the report, include the following exception message: \"Exception on main thread: ") + e.what() + "\". CraftOS-PC will now close.").c_str(), NULL);
        for (Computer * c : *computers) {
            c->running = 0;
            c->event_lock.notify_all();
        }
        exiting = true;
        awaitTasks([]()->bool {return computers.locked() || !computers->empty() || !taskQueue->empty();});
    }
#endif
    unblockInput();
    awaitTasks([]()->bool {return computers.locked() || !computers->empty() || !taskQueue->empty();});
    for (std::thread *t : computerThreads) { if (t->joinable()) {t->join(); delete t;} }
    computerThreads.clear();
    deinitializePlugins();
#ifndef NO_MIXER
    speakerQuit();
#endif
    driveQuit();
    http_server_stop();
    config_save();
#if !defined(__EMSCRIPTEN__) && !CRAFTOSPC_INDEV
    if (!updateAtQuit.empty()) {
        updateNow(updateAtQuit, &updateAtQuitRoot);
        awaitTasks();
    }
#endif
    if (factory) factory->quit();
    else SDL_Quit();
    // Clear out a few lists that plugins may insert functions into
    // We can't let these stay past the lifetime of plugins since C++ will try
    // to access methods that were unloaded to destroy the objects
    SDLTerminal::eventHandlers.clear();
    clearPeripherals();
    unloadPlugins();
    platformExit();
    return returnValue;
}
