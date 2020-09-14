/*
 * platform_win.cpp
 * CraftOS-PC 2
 * 
 * This file implements functions specific to Windows.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2020 JackMacWindows.
 */

#ifdef _WIN32
#include <windows.h>
#include "../platform.hpp"
#include "../mounter.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cstring>
#include <codecvt>
#include <unordered_map>
#include <processenv.h>
#include <shlwapi.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <wchar.h>
#include "../http.hpp"

const wchar_t * base_path = L"%appdata%\\CraftOS-PC";
std::wstring base_path_expanded;
std::wstring rom_path_expanded;
wchar_t expand_tmp[32767];

path_t wstr(std::string str) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.from_bytes(str);
}

std::string astr(path_t str) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    return converter.to_bytes(str);
}

FILE* platform_fopen(const wchar_t* path, const char * mode) { return _wfopen(path, wstr(mode).c_str()); }

void setBasePath(const char * path) {
    base_path_expanded = wstr(path);
}

void setROMPath(const char * path) {
    rom_path_expanded = wstr(path);
}

std::wstring getBasePath() {
    if (!base_path_expanded.empty()) return base_path_expanded;
    DWORD size = ExpandEnvironmentStringsW(base_path, expand_tmp, 32767);
    base_path_expanded = expand_tmp;
    return base_path_expanded;
}

std::wstring getROMPath() {
    if (!rom_path_expanded.empty()) return rom_path_expanded;
    GetModuleFileNameW(NULL, expand_tmp, 32767);
    rom_path_expanded = expand_tmp;
    rom_path_expanded = rom_path_expanded.substr(0, rom_path_expanded.find_last_of('\\'));
    return rom_path_expanded;
}

std::wstring getPlugInPath() { return getROMPath() + L"/plugins/"; }

std::wstring getMCSavePath() {
    DWORD size = ExpandEnvironmentStringsW(L"%appdata%\\.minecraft\\saves\\", expand_tmp, 32767);
    return std::wstring(expand_tmp);
}

void* kernel32handle = NULL;
HRESULT(*_SetThreadDescription)(HANDLE, PCWSTR) = NULL;

void setThreadName(std::thread &t, std::string name) {
    if (kernel32handle == NULL) {
        kernel32handle = SDL_LoadObject("kernel32");
        _SetThreadDescription = (HRESULT(*)(HANDLE, PCWSTR))SDL_LoadFunction(kernel32handle, "SetThreadDescription");
    }
    if (_SetThreadDescription != NULL) _SetThreadDescription((HANDLE)t.native_handle(), std::wstring(name.begin(), name.end()).c_str());
}

int createDirectory(std::wstring path) {
    struct_stat st;
    if (platform_stat(path.c_str(), &st) == 0) return !S_ISDIR(st.st_mode);
    if (CreateDirectoryExW(path.substr(0, path.find_last_of('\\', path.size() - 2)).c_str(), path.c_str(), NULL) == 0) {
        if ((GetLastError() == ERROR_PATH_NOT_FOUND || GetLastError() == ERROR_FILE_NOT_FOUND) && path != L"\\" && !path.empty()) {
            if (createDirectory(path.substr(0, path.find_last_of('\\', path.size() - 2)))) return 1;
            CreateDirectoryExW(path.substr(0, path.find_last_of('\\', path.size() - 2)).c_str(), path.c_str(), NULL);
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

unsigned long long getFreeSpace(std::wstring path) {
    ULARGE_INTEGER retval;
    if (GetDiskFreeSpaceExW(path.substr(0, path.find_last_of('\\', path.size() - 2)).c_str(), &retval, NULL, NULL) == 0) {
        if (path.substr(0, path.find_last_of(L"\\")-1).empty()) return 0;
        else return getFreeSpace(path.substr(0, path.find_last_of(L"\\")-1));
    }
    return retval.QuadPart;
}

unsigned long long getCapacity(std::wstring path) {
    ULARGE_INTEGER retval;
    if (GetDiskFreeSpaceExW(path.substr(0, path.find_last_of('\\', path.size() - 2)).c_str(), NULL, &retval, NULL) == 0) {
        if (path.substr(0, path.find_last_of(L"\\")-1).empty()) return 0;
        else return getCapacity(path.substr(0, path.find_last_of(L"\\")-1));
    }
    return retval.QuadPart;
}

int removeDirectory(std::wstring path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return GetLastError();
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        WIN32_FIND_DATAW find;
        std::wstring s = path;
        if (path[path.size() - 1] != '\\') s += L"\\";
        s += L"*";
        HANDLE h = FindFirstFileW(s.c_str(), &find);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(find.cFileName[0] == '.' && (wcslen(find.cFileName) == 1 || (find.cFileName[1] == '.' && wcslen(find.cFileName) == 2)))) {
                    std::wstring newpath = path;
                    if (path[path.size() - 1] != '\\') newpath += L"\\";
                    newpath += find.cFileName;
                    int res = removeDirectory(newpath);
                    if (res) {
                        FindClose(h);
                        return res;
                    }
                }
            } while (FindNextFileW(h, &find));
            FindClose(h);
        }
        return RemoveDirectoryW(path.c_str()) ? 0 : GetLastError();
    } else return DeleteFileW(path.c_str()) ? 0 : GetLastError();
}

void updateNow(std::string tagname) {
    HTTPDownload("https://github.com/MCJack123/craftos2/releases/download/" + tagname + (PathFileExistsW((getROMPath() + L"\\rom\\apis\\command\\commands.lua").c_str()) ? "CraftOS-PC-CCT-Edition-Setup.exe" : "/CraftOS-PC-Setup.exe"), [](std::istream& in) {
        char str[261];
        GetTempPathA(261, str);
        std::string path = std::string(str) + "\\setup.exe";
        std::ofstream out(path, std::ios::binary);
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

std::vector<std::wstring> failedCopy;

int recursiveCopy(std::wstring path, std::wstring toPath) {
    DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return GetLastError();
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        if (CreateDirectoryExW(toPath.substr(0, toPath.find_last_of('\\', toPath.size() - 2)).c_str(), toPath.c_str(), NULL) == 0) return GetLastError();
        WIN32_FIND_DATAW find;
        std::wstring s = path;
        if (path[path.size() - 1] != '\\') s += L"\\";
        s += L"*";
        HANDLE h = FindFirstFileW(s.c_str(), &find);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(find.cFileName[0] == '.' && (wcslen(find.cFileName) == 1 || (find.cFileName[1] == '.' && wcslen(find.cFileName) == 2)))) {
                    std::wstring newpath = path;
                    if (path[path.size() - 1] != '\\') newpath += L"\\";
                    newpath += find.cFileName;
                    int res = recursiveCopy(newpath, toPath + L"\\" + std::wstring(find.cFileName));
                    if (res) failedCopy.push_back(toPath + L"\\" + std::wstring(find.cFileName));
                }
            } while (FindNextFileW(h, &find));
            FindClose(h);
        }
        return RemoveDirectoryW(path.c_str()) ? 0 : GetLastError();
    } else return MoveFileW(path.c_str(), toPath.c_str()) ? 0 : GetLastError();
}

void migrateData() {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    DWORD size = ExpandEnvironmentStringsW(L"%USERPROFILE%\\.craftos", expand_tmp, 32767);
    std::wstring oldpath = expand_tmp;
    struct_stat st;
    if (platform_stat(oldpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && platform_stat(getBasePath().c_str(), &st) != 0)
        recursiveCopy(oldpath, getBasePath());
    if (!failedCopy.empty())
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Migration Failure", "Some files were unable to be moved while migrating the user data directory. These files have been left in place, and they will not appear inside the computer. You can copy them over from the old directory manually.", NULL);
}

void copyImage(SDL_Surface* surf) {
    char * bmp = new char[surf->w*surf->h*surf->format->BytesPerPixel + 128];
    SDL_RWops * rw = SDL_RWFromMem(bmp, surf->w*surf->h*surf->format->BytesPerPixel + 128);
    SDL_SaveBMP_RW(surf, rw, false);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, rw->seek(rw, 0, RW_SEEK_CUR) - sizeof(BITMAPFILEHEADER));
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
    MessageBoxA(NULL, "Uh oh, CraftOS-PC has crashed! Please report this to https://www.craftos-pc.cc/bugreport. When writing the report, attach the latest CraftOS-PC.exe .dmp file located here (you can type this into the File Explorer): '%LOCALAPPDATA%\\CrashDumps'. CraftOS-PC will now close.", "Application Error", MB_OK | MB_ICONSTOP);
    return EXCEPTION_CONTINUE_SEARCH;
}

// Do nothing. We definitely don't want to crash when there's only an invalid parameter, and I assume functions affected will return some value that won't cause problems. (I know strftime, used in os.date, will be fine.)
void invalidParameterHandler(const wchar_t * expression, const wchar_t * function, const wchar_t * file, unsigned int line, uintptr_t pReserved) {}

// We're relying on WER to automatically generate a minidump here.
// Hopefully the user can figure out how to use the File Explorer to go to a folder...
void setupCrashHandler() {
    SetUnhandledExceptionFilter(exceptionHandler);
    _set_invalid_parameter_handler(invalidParameterHandler);
}

#endif