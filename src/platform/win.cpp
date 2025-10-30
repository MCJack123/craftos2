/*
 * platform/win.cpp
 * CraftOS-PC 2
 * 
 * This file implements functions specific to Windows.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#ifdef _WIN32
#include <Windows.h>
#include <vector>
#include <string>
#include <fstream>
#include <cstring>
#include <codecvt>
#include <Poco/SHA2Engine.h>
#include <Poco/URI.h>
#include <Poco/Version.h>
#include <Poco/Crypto/X509Certificate.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/SSLException.h>
#include <processenv.h>
#include <Shlwapi.h>
#include <wincrypt.h>
#include <aclapi.h>
#include <sddl.h>
#include <commctrl.h>
#include <dirent.h>
#include <SDL2/SDL_syswm.h>
#include <sys/stat.h>
#include <zlib.h>
#include "../platform.hpp"
#include "../util.hpp"

const wchar_t * base_path = L"%appdata%\\CraftOS-PC";
path_t base_path_expanded;
path_t rom_path_expanded;
wchar_t expand_tmp[32768];

void setBasePath(path_t path) {
    base_path_expanded = path;
}

void setROMPath(path_t path) {
    rom_path_expanded = path;
}

path_t getBasePath() {
    if (!base_path_expanded.empty()) return base_path_expanded;
    ExpandEnvironmentStringsW(base_path, expand_tmp, 32768);
    base_path_expanded = path_t(expand_tmp, expand_tmp + wcsnlen(expand_tmp, 32768));
    return base_path_expanded;
}

path_t getROMPath() {
    if (!rom_path_expanded.empty()) return rom_path_expanded;
    GetModuleFileNameW(NULL, expand_tmp, 32768);
    rom_path_expanded = path_t(expand_tmp, expand_tmp + wcsnlen(expand_tmp, 32768)).parent_path();
    return rom_path_expanded;
}

path_t getPlugInPath() { return getROMPath() / "plugins"; }

path_t getMCSavePath() {
    ExpandEnvironmentStringsW(L"%appdata%\\.minecraft\\saves\\", expand_tmp, 32768);
    return path_t(expand_tmp);
}

void* kernel32handle = NULL;
HRESULT(*_SetThreadDescription)(HANDLE, PCWSTR) = NULL;

void setThreadName(std::thread &t, const std::string& name) {
    if (kernel32handle == NULL) {
        kernel32handle = SDL_LoadObject("kernel32");
        _SetThreadDescription = (HRESULT(*)(HANDLE, PCWSTR))SDL_LoadFunction(kernel32handle, "SetThreadDescription");
    }
    if (_SetThreadDescription != NULL) _SetThreadDescription((HANDLE)t.native_handle(), std::wstring(name.begin(), name.end()).c_str());
}

static std::string makeSize(double n) {
    if (n >= 100) return std::to_string((long)floor(n));
    else return std::to_string(n).substr(0, 4);
}

void updateNow(const std::string& tagname, const Poco::JSON::Object::Ptr root) {
    // If a delta update in the form "CraftOS-PC-Setup_Delta-v2.x.y.exe" is available, use that instead of the full installer
    // "v2.x.y" indicates the oldest version that can update from this installer
    std::string assetName = "CraftOS-PC-Setup.exe";
    Poco::JSON::Array::Ptr assets = root->getArray("assets");
    for (auto it = assets->begin(); it != assets->end(); it++) {
        Poco::JSON::Object::Ptr obj = it->extract<Poco::JSON::Object::Ptr>();
        std::string name = obj->getValue<std::string>("name");
        if (name.substr(0, 24) == "CraftOS-PC-Setup_Delta-v") {
            std::string tag = name.substr(23, name.size() - 27);
            if (strcmp(tag.c_str(), CRAFTOSPC_VERSION) <= 0) assetName = name;
            break;
        }
    }
    HTTPDownload("https://github.com/MCJack123/craftos2/releases/download/" + tagname + "/sha256-hashes.txt", [tagname, &assetName](std::istream * shain, Poco::Exception * e, Poco::Net::HTTPResponse * res){
        if (e != NULL) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Update Error", std::string("An error occurred while downloading the update: " + e->displayText()).c_str(), NULL);
            return;
        }
        std::string line;
        bool found = false;
        while (!shain->eof()) {
            std::getline(*shain, line);
            if (line.find(assetName) != std::string::npos) {found = true; break;}
        }
        if (!found) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Update Error", "A file required for verification could not be downloaded sucessfully. Please download the installer manually.", NULL);
            return;
        }
        std::string hash = line.substr(0, 64);
        HTTPDownload("https://github.com/MCJack123/craftos2/releases/download/" + tagname + "/" + assetName, [&hash](std::istream * in, Poco::Exception * e, Poco::Net::HTTPResponse * res) {
            if (e != NULL) {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Update Error", std::string("An error occurred while downloading the update: " + e->displayText()).c_str(), NULL);
                return;
            }

            size_t totalSize = res->getContentLength64();
            SDL_Window * win = SDL_CreateWindow("Downloading...", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 300, 50, SDL_WINDOW_UTILITY);
            SDL_FillRect(SDL_GetWindowSurface(win), NULL, 0xeeeeee);
            SDL_UpdateWindowSurface(win);
            SDL_SysWMinfo info;
            SDL_VERSION(&info.version);
            SDL_GetWindowWMInfo(win, &info);
            InitCommonControls();
            HWND hwndPB = CreateWindowEx(0, PROGRESS_CLASS, (LPTSTR) NULL, 
                                    WS_CHILD | WS_VISIBLE,
                                    5, 25, 290, 20,
                                    info.info.win.window, (HMENU) 0, info.info.win.hinstance, NULL);
            SendMessage(hwndPB, PBM_SETRANGE, 0, MAKELPARAM(0, 10000));
            SendMessage(hwndPB, PBM_SETSTEP, (WPARAM) 1, 0); 
            HWND hwndLabel = CreateWindow("static", "ST_U",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                              5, 3, 290, 20,
                              info.info.win.window, (HMENU)(501),
                              info.info.win.hinstance, NULL);
            std::string label = "0.0 / " + makeSize(totalSize / 1048576.0) + " MB, 0 B/s";
            SetWindowText(hwndLabel, label.c_str());
            HFONT hFont = CreateFont(
		            18, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, ANSI_CHARSET, 
		            OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
		            DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            SendMessage(hwndLabel, WM_SETFONT, (WPARAM)hFont, TRUE);

            std::string data;
            data.reserve(totalSize);
            char buf[2048];
            size_t total = 0;
            size_t bps = 0;
            size_t lastSecondSize = 0;
            std::chrono::system_clock::time_point lastSecond = std::chrono::system_clock::now();
            while (in->good() && !in->eof()) {
                in->read(buf, 2048);
                size_t sz = in->gcount();
                data += std::string(buf, sz);
                total += sz;
                if (std::chrono::system_clock::now() - lastSecond >= std::chrono::milliseconds(50) || in->eof()) {
                    bps = (total - lastSecondSize) * 20;
                    lastSecondSize = total;
                    lastSecond = std::chrono::system_clock::now();
                    label = makeSize(total / 1048576.0) + " / " + makeSize(totalSize / 1048576.0) + " MB, ";
                    if (bps >= 1048576) label += makeSize(bps / 1048576.0) + " MB/s";
                    else if (bps >= 1024) label += makeSize(bps / 1024.0) + " kB/s";
                    else label += std::to_string(bps) + " B/s";
                    SetWindowText(hwndLabel, label.c_str());
                    SendMessage(hwndPB, PBM_SETPOS, (WPARAM)((double)total / (double)totalSize * 10000) + 1, 0);
                    SendMessage(hwndPB, PBM_SETPOS, (WPARAM)((double)total / (double)totalSize * 10000), 0);
                    SDL_PumpEvents();
                }
            }
            SendMessage(hwndPB, PBM_SETPOS, (WPARAM)((double)total / (double)totalSize * 10000) - 1, 0);
            SendMessage(hwndPB, PBM_SETPOS, (WPARAM)((double)total / (double)totalSize * 10000), 0);
            SDL_PumpEvents();

            Poco::SHA2Engine engine;
            engine.update(data);
            std::string myhash = Poco::SHA2Engine::digestToHex(engine.digest());
            if (hash != myhash) {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Update Error", "The installer file could not be verified. Please try again later. If this issue persists, please download the installer manually.", NULL);
                return;
            }
            char str[261];
            GetTempPathA(261, str);
            const std::string path = std::string(str) + "\\setup.exe";
            std::ofstream out(path, std::ios::binary);
            out << data;
            out.close();
            DestroyWindow(hwndPB);
            DestroyWindow(hwndLabel);
            SDL_DestroyWindow(win);

            STARTUPINFOA sinfo;
            memset(&sinfo, 0, sizeof(sinfo));
            sinfo.cb = sizeof(sinfo);
            PROCESS_INFORMATION process;
            CreateProcessA(path.c_str(), (char*)(path + " /SILENT").c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &sinfo, &process);
            CloseHandle(process.hProcess);
            CloseHandle(process.hThread);
            exit(0);
        });
    });
}

std::vector<std::wstring> failedCopy;

static int recursiveMove(const std::wstring& path, const std::wstring& toPath) {
    const DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return GetLastError();
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        if (CreateDirectoryExW(toPath.substr(0, toPath.find_last_of('\\', toPath.size() - 2)).c_str(), toPath.c_str(), NULL) == 0) return GetLastError();
        WIN32_FIND_DATAW find;
        std::wstring s = path;
        if (path[path.size() - 1] != '\\') s += L"\\";
        s += L"*";
        const HANDLE h = FindFirstFileW(s.c_str(), &find);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(find.cFileName[0] == '.' && (wcslen(find.cFileName) == 1 || (find.cFileName[1] == '.' && wcslen(find.cFileName) == 2)))) {
                    std::wstring newpath = path;
                    if (path[path.size() - 1] != '\\') newpath += L"\\";
                    newpath += find.cFileName;
                    const int res = recursiveMove(newpath, toPath + L"\\" + std::wstring(find.cFileName));
                    if (res) failedCopy.push_back(toPath + L"\\" + std::wstring(find.cFileName));
                }
            } while (FindNextFileW(h, &find));
            FindClose(h);
        }
        return RemoveDirectoryW(path.c_str()) ? 0 : (int)GetLastError();
    } else return MoveFileW(path.c_str(), toPath.c_str()) ? 0 : (int)GetLastError();
}

void migrateOldData() {
    ExpandEnvironmentStringsW(L"%USERPROFILE%\\.craftos", expand_tmp, 32767);
    const std::wstring oldpath = expand_tmp;
    struct _stat st;
    if (_wstat(oldpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && _wstat(getBasePath().c_str(), &st) != 0)
        recursiveMove(oldpath, getBasePath());
    if (!failedCopy.empty())
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Migration Failure", "Some files were unable to be moved while migrating the user data directory. These files have been left in place, and they will not appear inside the computer. You can copy them over from the old directory manually.", NULL);
}

void copyImage(SDL_Surface* surf, SDL_Window* win) {
    char * bmp = new char[surf->w*surf->h*surf->format->BytesPerPixel + 128];
    SDL_RWops * rw = SDL_RWFromMem(bmp, surf->w*surf->h*surf->format->BytesPerPixel + 128);
    SDL_SaveBMP_RW(surf, rw, false);
    const HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, rw->seek(rw, 0, RW_SEEK_CUR) - sizeof(BITMAPFILEHEADER));
    if (hMem == NULL) { delete[] bmp; return; }
    memcpy(GlobalLock(hMem), bmp + sizeof(BITMAPFILEHEADER), rw->seek(rw, 0, RW_SEEK_CUR) - sizeof(BITMAPFILEHEADER));
    GlobalUnlock(hMem);
    OpenClipboard(0);
    EmptyClipboard();
    SetClipboardData(CF_DIB, hMem);
    CloseClipboard();
    delete[] bmp;
}

LONG WINAPI exceptionHandler(PEXCEPTION_POINTERS pExceptionInfo) {
    if (!loadingPlugin.empty()) MessageBoxA(NULL, std::string("Uh oh, CraftOS-PC has crashed! It appears the plugin \"" + loadingPlugin + "\" may have been responsible for this. Please remove it and try again. CraftOS-PC will now close.").c_str(), "Application Error", MB_OK | MB_ICONSTOP);
#ifdef CRASHREPORT_API_KEY
    else if (config.snooperEnabled) MessageBoxA(NULL, "Uh oh, CraftOS-PC has crashed! A crash log has been saved and will be uploaded on next launch. CraftOS-PC will now close.", "Application Error", MB_OK | MB_ICONSTOP);
#endif
    else MessageBoxA(NULL, std::string("Uh oh, CraftOS-PC has crashed! Please report this to https://www.craftos-pc.cc/bugreport. When writing the report, attach the latest CraftOS-PC.exe .dmp file located here (you can type this into the File Explorer): '%LOCALAPPDATA%\\CrashDumps'. Add this text to the report as well: \"Last C function: " + std::string(lastCFunction) + "\". CraftOS-PC will now close.").c_str(), "Application Error", MB_OK | MB_ICONSTOP);
    return EXCEPTION_CONTINUE_SEARCH;
}

// Do nothing. We definitely don't want to crash when there's only an invalid parameter, and I assume functions affected will return some value that won't cause problems. (I know strftime, used in os.date, will be fine.)
void invalidParameterHandler(const wchar_t * expression, const wchar_t * function, const wchar_t * file, unsigned int line, uintptr_t pReserved) {}

#ifdef CRASHREPORT_API_KEY
#include "../apikey.cpp" // if you get an error here, please go into Project Properties => C/C++ => Preprocessor => Preprocessor Defines and remove "CRASHREPORT_API_KEY" from the list

const std::string amazon_root_certificate = "-----BEGIN CERTIFICATE-----\n\
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n\
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n\
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n\
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n\
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n\
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n\
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n\
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n\
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n\
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n\
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n\
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n\
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n\
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n\
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n\
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n\
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n\
rqXRfboQnoZsG4q5WTP468SQvvG5\n\
-----END CERTIFICATE-----";

const std::string amazon_certificate = "-----BEGIN CERTIFICATE-----\n\
MIIESTCCAzGgAwIBAgITBntQXCplJ7wevi2i0ZmY7bibLDANBgkqhkiG9w0BAQsF\n\
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n\
b24gUm9vdCBDQSAxMB4XDTE1MTAyMTIyMjQzNFoXDTQwMTAyMTIyMjQzNFowRjEL\n\
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEVMBMGA1UECxMMU2VydmVyIENB\n\
IDFCMQ8wDQYDVQQDEwZBbWF6b24wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEK\n\
AoIBAQDCThZn3c68asg3Wuw6MLAd5tES6BIoSMzoKcG5blPVo+sDORrMd4f2AbnZ\n\
cMzPa43j4wNxhplty6aUKk4T1qe9BOwKFjwK6zmxxLVYo7bHViXsPlJ6qOMpFge5\n\
blDP+18x+B26A0piiQOuPkfyDyeR4xQghfj66Yo19V+emU3nazfvpFA+ROz6WoVm\n\
B5x+F2pV8xeKNR7u6azDdU5YVX1TawprmxRC1+WsAYmz6qP+z8ArDITC2FMVy2fw\n\
0IjKOtEXc/VfmtTFch5+AfGYMGMqqvJ6LcXiAhqG5TI+Dr0RtM88k+8XUBCeQ8IG\n\
KuANaL7TiItKZYxK1MMuTJtV9IblAgMBAAGjggE7MIIBNzASBgNVHRMBAf8ECDAG\n\
AQH/AgEAMA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUWaRmBlKge5WSPKOUByeW\n\
dFv5PdAwHwYDVR0jBBgwFoAUhBjMhTTsvAyUlC4IWZzHshBOCggwewYIKwYBBQUH\n\
AQEEbzBtMC8GCCsGAQUFBzABhiNodHRwOi8vb2NzcC5yb290Y2ExLmFtYXpvbnRy\n\
dXN0LmNvbTA6BggrBgEFBQcwAoYuaHR0cDovL2NybC5yb290Y2ExLmFtYXpvbnRy\n\
dXN0LmNvbS9yb290Y2ExLmNlcjA/BgNVHR8EODA2MDSgMqAwhi5odHRwOi8vY3Js\n\
LnJvb3RjYTEuYW1hem9udHJ1c3QuY29tL3Jvb3RjYTEuY3JsMBMGA1UdIAQMMAow\n\
CAYGZ4EMAQIBMA0GCSqGSIb3DQEBCwUAA4IBAQAfsaEKwn17DjAbi/Die0etn+PE\n\
gfY/I6s8NLWkxGAOUfW2o+vVowNARRVjaIGdrhAfeWHkZI6q2pI0x/IJYmymmcWa\n\
ZaW/2R7DvQDtxCkFkVaxUeHvENm6IyqVhf6Q5oN12kDSrJozzx7I7tHjhBK7V5Xo\n\
TyS4NU4EhSyzGgj2x6axDd1hHRjblEpJ80LoiXlmUDzputBXyO5mkcrplcVvlIJi\n\
WmKjrDn2zzKxDX5nwvkskpIjYlJcrQu4iCX1/YwZ1yNqF9LryjlilphHCACiHbhI\n\
RnGfN8j8KLDVmWyTYMk8V+6j0LI4+4zFh2upqGMQHL3VFVFWBek6vCDWhB/b\n\
-----END CERTIFICATE-----";

static bool pushCrashDump(const char * data, const size_t size, const path_t& path, const std::string& url = "https://kkppoknwel.execute-api.us-east-2.amazonaws.com/dev/uploadCrashDump", const std::string& method = "POST") {
    Poco::URI uri(url);
    Poco::Net::Context::Ptr ctx = new Poco::Net::Context(Poco::Net::Context::TLS_CLIENT_USE, "", Poco::Net::Context::VERIFY_STRICT, 9, false, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
#if POCO_VERSION >= 0x010A0000
    ctx->disableProtocols(Poco::Net::Context::PROTO_TLSV1_3);
#endif
    std::stringstream rootcertstream(amazon_root_certificate);
    Poco::Crypto::X509Certificate rootcert(rootcertstream);
    ctx->addCertificateAuthority(rootcert);
    std::stringstream certstream(amazon_certificate);
    Poco::Crypto::X509Certificate cert(certstream);
    ctx->addCertificateAuthority(cert);
    ctx->enableExtendedCertificateVerification();
    Poco::Net::HTTPSClientSession session(uri.getHost(), uri.getPort(), ctx);
    if (!config.http_proxy_server.empty()) session.setProxy(config.http_proxy_server, config.http_proxy_port);
    Poco::Net::HTTPRequest request(method, uri.getPathAndQuery(), Poco::Net::HTTPMessage::HTTP_1_1);
    Poco::Net::HTTPResponse response;
    session.setTimeout(Poco::Timespan(5000000));
    request.add("Host", uri.getHost());
    request.add("User-Agent", "CraftOS-PC/" CRAFTOSPC_VERSION " ComputerCraft/" CRAFTOSPC_CC_VERSION);
    request.add("X-API-Key", getAPIKey());
    request.add("x-amz-server-side-encryption", "AES256");
    request.setContentType("application/gzip");
    request.setContentLength(size);
    try {
        session.sendRequest(request).write(data, size);
        std::istream& stream = session.receiveResponse(response);
        if (response.getStatus() / 100 == 3 && response.has("Location")) 
            return pushCrashDump(data, size, path, response.get("Location"), method);
        else if (response.getStatus() == 200 && method == "POST") {
            Value root;
            Poco::JSON::Object::Ptr p = root.parse(stream);
            if (root.isMember("uploadURL")) {
                return pushCrashDump(data, size, path, root["uploadURL"].asString(), "PUT");
            } else if (root.isMember("error")) {
                fprintf(stderr, "Warning: Couldn't upload crash dump at %s: %s\n", path.string().c_str(), root["error"].asString().c_str());
                return false;
            } else if (root.isMember("message")) {
                fprintf(stderr, "Warning: Couldn't upload crash dump at %s: %s\n", path.string().c_str(), root["message"].asString().c_str());
                return false;
            }
        }
    } catch (Poco::Net::SSLException &e) {
        fprintf(stderr, "Warning: Couldn't upload crash dump at %s: %s\n", path.string().c_str(), e.message().c_str());
        return false;
    } catch (Poco::Exception &e) {
        fprintf(stderr, "Warning: Couldn't upload crash dump at %s: %s\n", path.string().c_str(), e.displayText().c_str());
        return false;
    }
    return true;
}
#endif

// We're relying on WER to automatically generate a minidump here.
// If this is an official build with an API key, we'll automatically upload the dump on next start
void setupCrashHandler() {
    SetUnhandledExceptionFilter(exceptionHandler);
    _set_invalid_parameter_handler(invalidParameterHandler);
}

void uploadCrashDumps() {
#ifdef CRASHREPORT_API_KEY
    if (config.snooperEnabled) {
        WIN32_FIND_DATAW find;
        wchar_t path[32767];
        ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\CrashDumps\\", path, 32767);
        std::wstring searchpath = std::wstring(path) + L"CraftOS-PC.exe.*.dmp";
        const HANDLE h = FindFirstFileW(searchpath.c_str(), &find);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                std::wstring newpath = std::wstring(path) + find.cFileName;
                std::stringstream ss;
                FILE * source = _wfopen(newpath.c_str(), L"rb");

                int ret, flush;
                unsigned have;
                z_stream strm;
                unsigned char in[16384];
                unsigned char out[16384];

                /* allocate deflate state */
                strm.zalloc = Z_NULL;
                strm.zfree = Z_NULL;
                strm.opaque = Z_NULL;
                ret = deflateInit2(&strm, 7, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
                if (ret != Z_OK) {
                    fclose(source);
                    continue;
                }

                /* compress until end of file */
                do {
                    strm.avail_in = fread(in, 1, 16384, source);
                    if (ferror(source)) {
                        (void)deflateEnd(&strm);
                        fclose(source);
                        continue;
                    }
                    flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
                    strm.next_in = in;

                    /* run deflate() on input until output buffer not full, finish
                    compression if all of source has been read in */
                    do {
                        strm.avail_out = 16384;
                        strm.next_out = out;
                        ret = deflate(&strm, flush);    /* no bad return value */
                        assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
                        have = 16384 - strm.avail_out;
                        ss.write((const char*)out, have);
                    } while (strm.avail_out == 0);

                    /* done when last data in file processed */
                } while (flush != Z_FINISH);

                /* clean up and return */
                (void)deflateEnd(&strm);
                fclose(source);
                std::string data = ss.str();
                if (pushCrashDump(data.c_str(), data.size(), newpath)) DeleteFileW(newpath.c_str());
            } while (FindNextFileW(h, &find));
            FindClose(h);
        }
    }
#endif
}

void setFloating(SDL_Window* win, bool state) {
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(win, &info);
    if (info.subsystem != SDL_SYSWM_WINDOWS) return; // should always be true
    SetWindowPos(info.info.win.window, state ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void platformExit() {
    if (kernel32handle != NULL) SDL_UnloadObject(kernel32handle);
}

void addSystemCertificates(Poco::Net::Context::Ptr context) {
    HCERTSTORE store = CertOpenSystemStore(NULL, "ROOT");
    if (store == NULL) return;
    for (PCCERT_CONTEXT c = CertEnumCertificatesInStore(store, NULL); c != NULL; c = CertEnumCertificatesInStore(store, c)) {
        X509 * cert = d2i_X509(NULL, (const unsigned char**)&c->pbCertEncoded, c->cbCertEncoded);
        context->addCertificateAuthority(Poco::Crypto::X509Certificate(cert));
    }
    CertCloseStore(store, 0);
}

void unblockInput() {
    DWORD tmp;
    INPUT_RECORD ir[2];
    ir[0].EventType = KEY_EVENT;
    ir[0].Event.KeyEvent.bKeyDown = TRUE;
    ir[0].Event.KeyEvent.dwControlKeyState = 0;
    ir[0].Event.KeyEvent.uChar.UnicodeChar = VK_RETURN;
    ir[0].Event.KeyEvent.wRepeatCount = 1;
    ir[0].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
    ir[0].Event.KeyEvent.wVirtualScanCode = MapVirtualKey(VK_RETURN, MAPVK_VK_TO_VSC);
    ir[1] = ir[0];
    ir[1].Event.KeyEvent.bKeyDown = FALSE;
    WriteConsoleInput(GetStdHandle(STD_INPUT_HANDLE), ir, 2, &tmp);
}

// This function was partially generated by ChatGPT. It has been edited and
// verified for correctness and security.
bool winFolderIsReadOnly(path_t path) {
    DWORD desiredAccess = FILE_GENERIC_WRITE;
    DWORD grantedAccess = 0;
    BOOL accessStatus = FALSE;
    HANDLE tokenHandle = nullptr;

    // Get current process token
    if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tokenHandle)) {
        if (GetLastError() == ERROR_NO_TOKEN) {
            if (!ImpersonateSelf(SecurityImpersonation))
                return true;
            if (!OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &tokenHandle))
                return true;
        } else return true;
    }

    PSECURITY_DESCRIPTOR securityDesc = nullptr;
    PACL dacl = nullptr;
    PSID owner = nullptr, group = nullptr;

    DWORD result = GetNamedSecurityInfoW(path.wstring().c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, &owner, &group, &dacl, nullptr, &securityDesc);
    if (result != ERROR_SUCCESS) {
        CloseHandle(tokenHandle);
        return true;
    }

    PRIVILEGE_SET privileges = {};
    DWORD privilegeSetLength = sizeof(privileges);
    GENERIC_MAPPING mapping = { FILE_GENERIC_READ, FILE_GENERIC_WRITE, FILE_GENERIC_EXECUTE, FILE_ALL_ACCESS };

    MapGenericMask(&desiredAccess, &mapping);

    if (!AccessCheck(securityDesc, tokenHandle, desiredAccess, &mapping, &privileges, &privilegeSetLength, &grantedAccess, &accessStatus)) {
        accessStatus = FALSE;
    }

    if (securityDesc) LocalFree(securityDesc);
    CloseHandle(tokenHandle);

    return accessStatus == FALSE;
}

#endif