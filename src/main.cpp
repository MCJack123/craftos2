/*
 * main.cpp
 * CraftOS-PC 2
 * 
 * This file controls the Lua VM, loads the CraftOS BIOS, and sends events back.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#include "Computer.hpp"
#include "config.hpp"
#include "peripheral/drive.hpp"
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/SSLException.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/JSON/Parser.h>

extern void termInit();
extern void termClose();
#ifndef NO_CLI
extern void cliInit();
extern void cliClose();
#endif
extern void config_init();
extern void config_save(bool deinit);
extern void mainLoop();
extern void http_server_stop();
extern void* queueTask(std::function<void*(void*)> func, void* arg);
extern std::list<std::thread*> computerThreads;
extern bool exiting;
bool headless = false;
bool cli = false;
std::string script_file = "";

void update_thread() {
    // Update checker (2.1 will add auto-install)
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
        if (root->getValue<std::string>("tag_name") != CRAFTOSPC_VERSION) 
            queueTask([ ](void* arg)->void*{SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "Update available!", (const char*)arg, NULL); return NULL;}, 
                      (void*)(std::string("A new update to CraftOS-PC is available (") + root->getValue<std::string>("tag_name") + ", you have " CRAFTOSPC_VERSION "). Go to " + root->getValue<std::string>("html_url") + " to download the new version.").c_str());
    } catch (std::exception &e) {
        printf("Could not check for updates: %s\n", e.what());
    }
}

int main(int argc, char*argv[]) {
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--headless") headless = true;
#ifndef NO_CLI
        else if (std::string(argv[i]) == "--cli") cli = true;
#endif
        else if (std::string(argv[i]) == "--script") script_file = argv[++i];
        else if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h" || std::string(argv[i]) == "-?") {
            std::cerr << "Usage: " << argv[0] << " [--cli] [--headless] [--script <file>]\n";
            return 0;
        }
    }
    if (headless && cli) {
        std::cerr << "Error: Cannot combine headless & CLI options\n";
        return 1;
    }
#ifndef NO_CLI
    if (cli) cliInit();
    else 
#endif
        termInit();
    config_init();
    driveInit();
    if (!headless && !cli && config.checkUpdates) {
        std::thread update_bg(update_thread);
        update_bg.detach();
    }
    startComputer(0);
    mainLoop();
    for (std::thread *t : computerThreads) { t->join(); delete t; }
    driveQuit();
#ifndef NO_CLI
    if (cli) cliClose();
    else 
#endif
        termClose();
    http_server_stop();
    config_save(true);
    return 0;
}
