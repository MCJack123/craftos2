/*
 * platform_win.cpp
 * CraftOS-PC 2
 * 
 * This file implements functions specific to Windows.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019 JackMacWindows.
 */

#ifdef _WIN32
#include <windows.h>
#include "platform.hpp"
#include "mounter.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <unordered_map>
#include <processenv.h>
#include <shlwapi.h>
#include <dirent.h>
#include <sys/stat.h>
#include "http.hpp"

const char * base_path = "%appdata%\\CraftOS-PC";
const char * rom_path = "%ProgramFiles%\\CraftOS-PC";
std::string base_path_expanded;
std::string rom_path_expanded;
char expand_tmp[32767];

std::string getBasePath() {
    if (!base_path_expanded.empty()) return base_path_expanded;
    DWORD size = ExpandEnvironmentStringsA(base_path, expand_tmp, 32767);
    base_path_expanded = expand_tmp;
    return base_path_expanded;
}

std::string getROMPath() {
    if (!rom_path_expanded.empty()) return rom_path_expanded;
    DWORD size = ExpandEnvironmentStringsA(rom_path, expand_tmp, 32767);
    rom_path_expanded = expand_tmp;
    return rom_path_expanded;
}

std::string getPlugInPath() { return getROMPath() + "/plugins/"; }

void setThreadName(std::thread &t, std::string name) {
#ifdef DEBUG
    SetThreadDescription((HANDLE)t.native_handle(), s2ws(name).c_str());
#endif
}

int createDirectory(std::string path) {
	if (CreateDirectoryExA(path.substr(0, path.find_last_of('\\', path.size() - 2)).c_str(), path.c_str(), NULL) == 0) {
		if ((GetLastError() == ERROR_PATH_NOT_FOUND || GetLastError() == ERROR_FILE_NOT_FOUND) && path != "\\") {
            if (createDirectory(path.substr(0, path.find_last_of('\\', path.size() - 2)))) return 1;
			CreateDirectoryExA(path.substr(0, path.find_last_of('\\', path.size() - 2)).c_str(), path.c_str(), NULL);
		}
        else if (GetLastError() != ERROR_ALREADY_EXISTS) return 1;
	}
	return 0;
}

char* basename(char* path) {
	char* filename = strrchr(path, '/');
	if (filename == NULL)
		filename = path;
	else
		filename++;
	//strcpy(path, filename);
	return filename;
}

char* dirname(char* path) {
	if (path[0] == '/') strcpy(path, &path[1]);
    char tch;
    if (strrchr(path, '/') != NULL) tch = '/';
    else if (strrchr(path, '\\') != NULL) tch = '\\';
    else return path;
    path[strrchr(path, tch) - path] = '\0';
	return path;
}

unsigned long long getFreeSpace(std::string path) {
	ULARGE_INTEGER retval;
	GetDiskFreeSpaceExA(path.substr(0, path.find_last_of('\\', path.size() - 2)).c_str(), &retval, NULL, NULL);
	return retval.QuadPart;
}

int removeDirectory(std::string path) {
	DWORD attr = GetFileAttributesA(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return GetLastError();
	if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        WIN32_FIND_DATA find;
        std::string s = path;
        if (path[path.size() - 1] != '\\') s += "\\";
        s += "*";
        HANDLE h = FindFirstFileA(s.c_str(), &find);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(find.cFileName[0] == '.' && (strlen(find.cFileName) == 1 || (find.cFileName[1] == '.' && strlen(find.cFileName) == 2)))) {
                    std::string newpath = path;
                    if (path[path.size() - 1] != '\\') newpath += "\\";
                    newpath += find.cFileName;
                    int res = removeDirectory(newpath);
                    if (res) {
                        FindClose(h);
                        return res;
                    }
                }
            } while (FindNextFileA(h, &find));
            FindClose(h);
        }
        return RemoveDirectoryA(path.c_str()) ? 0 : GetLastError();
	} else return DeleteFileA(path.c_str()) ? 0 : GetLastError();
}

std::unordered_map<double, const char *> windows_version_map = {
    {10.0, "Windows 10"},
    {6.3, "Windows 8.1"},
    {6.2, "Windows 8"},
    {6.1, "Windows 7"},
    {6.0, "Windows Vista"},
    {5.1, "Windows XP"},
    {5.0, "Windows 2000"},
    {4.0, "Windows NT 4"},
    {3.51, "Windows NT 3.51"},
    {3.5, "Windows NT 3.5"},
    {3.1, "Windows NT 3.1"}
};
#ifdef _MSC_VER

#if defined(_M_IX86__)
#define ARCHITECTURE "i386"
#elif defined(_M_AMD64)
#define ARCHITECTURE "amd64"
#elif defined(_M_X64)
#define ARCHITECTURE "x86_64"
#elif defined(_M_ARM)
#define ARCHITECTURE "armv7"
#elif defined(_M_ARM64)
#define ARCHITECTURE "arm64"
#else
#define ARCHITECTURE "unknown"
#endif

#else

#if defined(__i386__) || defined(__i386) || defined(i386)
#define ARCHITECTURE "i386"
#elif defined(__amd64__) || defined(__amd64)
#define ARCHITECTURE "amd64"
#elif defined(__x86_64__) || defined(__x86_64)
#define ARCHITECTURE "x86_64"
#elif defined(__arm__) || defined(__arm)
#define ARCHITECTURE "armv7"
#elif defined(__arm64__) || defined(__arm64)
#define ARCHITECTURE "arm64"
#elif defined(__aarch64__) || defined(__aarch64)
#define ARCHITECTURE "aarch64"
#else
#define ARCHITECTURE "unknown"
#endif

#endif

void pushHostString(lua_State *L) {
    OSVERSIONINFOA info;
    ZeroMemory(&info, sizeof(OSVERSIONINFOA));
    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    GetVersionEx(&info);
    double version = info.dwMajorVersion
        + (info.dwMinorVersion / 10.0);
    lua_pushfstring(L, "%s %s %d.%d", windows_version_map[version], ARCHITECTURE, info.dwMajorVersion, info.dwMinorVersion);
}

void updateNow(std::string tagname) {
    HTTPDownload("https://github.com/MCJack123/craftos2/releases/download/" + tagname + "/CraftOS-PC-Setup.exe", [](std::istream& in) {
        char str[261];
        GetTempPathA(261, str);
        std::string path = std::string(str) + "\\setup.exe";
        std::ofstream out(path, std::ios::binary);
        //char c = in.get();
        //while (in.good()) {out.put(c); c = in.get();}
        out << in.rdbuf();
        out.close();
        STARTUPINFOA info;
        memset(&info, 0, sizeof(info));
        info.cb = sizeof(info);
        PROCESS_INFORMATION process;
        CreateProcessA(path.c_str(), (char*)(path + " /SILENT").c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &info, &process);
        CloseHandle(process.hProcess);
        CloseHandle(process.hThread);
        exit(0);
    });
}

std::vector<std::string> failedCopy;

int recursiveCopy(std::string path, std::string toPath) {
	DWORD attr = GetFileAttributesA(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return GetLastError();
	if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        if (CreateDirectoryExA(toPath.substr(0, toPath.find_last_of('\\', toPath.size() - 2)).c_str(), toPath.c_str(), NULL) == 0) return GetLastError();
        WIN32_FIND_DATA find;
        std::string s = path;
        if (path[path.size() - 1] != '\\') s += "\\";
        s += "*";
        HANDLE h = FindFirstFileA(s.c_str(), &find);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(find.cFileName[0] == '.' && (strlen(find.cFileName) == 1 || (find.cFileName[1] == '.' && strlen(find.cFileName) == 2)))) {
                    std::string newpath = path;
                    if (path[path.size() - 1] != '\\') newpath += "\\";
                    newpath += find.cFileName;
                    int res = recursiveCopy(newpath, toPath + "\\" + std::string(find.cFileName));
                    if (res) {
                        //FindClose(h);
                        //return res;
                        failedCopy.push_back(toPath + "\\" + std::string(find.cFileName));
                    }
                }
            } while (FindNextFileA(h, &find));
            FindClose(h);
        }
        return RemoveDirectoryA(path.c_str()) ? 0 : GetLastError();
	} else return MoveFileA(path.c_str(), toPath.c_str()) ? 0 : GetLastError();
}

void migrateData() {
    DWORD size = ExpandEnvironmentStringsA("%USERPROFILE%\\.craftos", expand_tmp, 32767);
    std::string oldpath = expand_tmp;
    struct stat st;
    if (stat(oldpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && stat(getBasePath().c_str(), &st) != 0)
        recursiveCopy(oldpath, getBasePath());
    if (!failedCopy.empty())
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Migration Failure", "Some files were unable to be moved while migrating the user data directory. These files have been left in place, and they will not appear inside the computer. You can copy them over from the old directory manually.", NULL);
}

std::unordered_map<std::string, HINSTANCE> dylibs;

void * loadSymbol(std::string path, std::string symbol) {
    HINSTANCE handle;
    if (dylibs.find(path) == dylibs.end()) dylibs[path] = LoadLibrary(path.c_str());
    handle = dylibs[path];
    return GetProcAddress(handle, symbol.c_str());
}

void unloadLibraries() {
    for (auto lib : dylibs) FreeLibrary(lib.second);
}

#endif