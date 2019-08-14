#include <windows.h>
#include "platform.h"
#include "mounter.h"
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <unordered_map>
#include <processenv.h>
#include <shlwapi.h>

const char * base_path = "%USERPROFILE%\\.craftos";
const char * rom_path = "%ProgramFiles(x86)%\\CraftOS-PC";
char * base_path_expanded;
char * rom_path_expanded;
char expand_tmp[32767];

std::wstring s2ws(const std::string& str) {
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

extern "C" {

void platformInit() {
    if (rom_path_expanded == NULL) {
        DWORD size = ExpandEnvironmentStringsA(rom_path, expand_tmp, 32767);
        rom_path_expanded = (char*)malloc(size + 1);
        memcpy(rom_path_expanded, expand_tmp, size);
        rom_path_expanded[size] = 0;
    }
    addMount((std::string(rom_path_expanded) + "\\rom").c_str(), "rom", true);
}

void platformFree() {
    if (base_path_expanded != NULL) {
        free(base_path_expanded);
        base_path_expanded = NULL;
    }
    if (rom_path_expanded != NULL) {
        free(rom_path_expanded);
        rom_path_expanded = NULL;
    }
}

const char * getBasePath() {
    if (base_path_expanded != NULL) return base_path_expanded;
    DWORD size = ExpandEnvironmentStringsA(base_path, expand_tmp, 32767);
    char* dest = (char*)malloc(size + 1);
    memcpy(dest, expand_tmp, size);
    dest[size] = 0;
    base_path_expanded = dest;
    return dest;
}

const char * getROMPath() {
    if (rom_path_expanded != NULL) return rom_path_expanded;
    DWORD size = ExpandEnvironmentStringsA(rom_path, expand_tmp, 32767);
    char* dest = (char*)malloc(size + 1);
    memcpy(dest, expand_tmp, size);
    dest[size] = 0;
    rom_path_expanded = dest;
    return dest;
}

char * getBIOSPath() {
    const char * rp = getROMPath();
    char * retval = (char*)malloc(strlen(rp) + 10);
    strcpy(retval, rp);
    strcat(retval, "\\bios.lua");
    return retval;
}

/*char * expandEnvironment(const char * src) {
    if (base_path_expanded != NULL && std::string(src) == std::string(base_path)) return base_path_expanded;
	if (rom_path_expanded != NULL && std::string(src) == std::string(rom_path)) return rom_path_expanded;
	DWORD size = ExpandEnvironmentStringsA(src, expand_tmp, 32767);
	char* dest = (char*)malloc(size + 1);
	memcpy(dest, expand_tmp, size);
	dest[size] = 0;
    if (std::string(src) == std::string(base_path)) base_path_expanded = dest;
    else if (std::string(src) == std::string(rom_path)) rom_path_expanded = dest;
	return dest;
}*/

struct thread_param {
    void*(*func)(void*);
    void* arg;
};

DWORD WINAPI WinThreadFunc(LPVOID lpParam) {
    struct thread_param* p = (struct thread_param*)lpParam;
    p->func(p->arg);
    free(p);
    return 0;
}

void * createThread(void*(*func)(void*), void* arg) {
    struct thread_param* p = new struct thread_param;
    p->func = func;
    p->arg = arg;
    return CreateThread(NULL, 0, WinThreadFunc, p, 0, NULL);
}

void joinThread(void * thread) {
    WaitForSingleObject((HANDLE)thread, 0);
}

int createDirectory(const char* path) {
    char* dir = (char*)malloc(strlen(path) + 1);
    strcpy(dir, path);
    dirname(dir);
	if (CreateDirectoryExA(dir, path, NULL) == 0) {
		if (GetLastError() == ERROR_PATH_NOT_FOUND && strcmp(path, "\\") != 0) {
            if (createDirectory(dir)) { free(dir); return 1; }
			CreateDirectoryExA(dir, path, NULL);
		}
        else if (GetLastError() != ERROR_ALREADY_EXISTS) { free(dir); return 1; }
	}
    free(dir);
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

void msleep(unsigned long time) {
    Sleep(time);
}

unsigned long long getFreeSpace(char* path) {
	PathRemoveFileSpecA(path);
	ULARGE_INTEGER retval;
	GetDiskFreeSpaceExA(path, &retval, NULL, NULL);
	return retval.QuadPart;
}

void platform_fs_find(lua_State* L, char* path) {
	// todo
}

int removeDirectory(char* path) {
	DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) return GetLastError();
	if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        WIN32_FIND_DATA find;
        char * s = (char*)malloc(strlen(path) + (path[strlen(path) - 1] != '\\') + 2);
        strcpy(s, path);
        if (path[strlen(path) - 1] != '\\') strcat(s, "\\");
        strcat(s, "*");
        HANDLE h = FindFirstFileA(s, &find);
        free(s);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (!(find.cFileName[0] == '.' && (strlen(find.cFileName) == 1 || (find.cFileName[1] == '.' && strlen(find.cFileName) == 2)))) {
                    char * newpath = (char*)malloc(strlen(path) + strlen(find.cFileName) + (path[strlen(path) - 1] != '\\') + 1);
                    strcpy(newpath, path);
                    if (path[strlen(path) - 1] != '\\') strcat(newpath, "\\");
                    strcat(newpath, find.cFileName);
                    int res = removeDirectory(newpath);
                    if (res) {
                        free(newpath);
                        FindClose(h);
                        return res;
                    }
                    free(newpath);
                }
            } while (FindNextFileA(h, &find));
            FindClose(h);
        }
        return RemoveDirectoryA(path) ? 0 : GetLastError();
	} else return DeleteFileA(path) ? 0 : GetLastError();
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
}