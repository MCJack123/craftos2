#include <windows.h>
#include "platform.h"
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <processenv.h>
#include <shlwapi.h>

const char * base_path = "%USERPROFILE%\\.craftos\\computer\\0";
const char * rom_path = "%USERPROFILE%\\AppData\\Local\\craftos";
const char * bios_path = "%USERPROFILE%\\AppData\\Local\\craftos\\bios.lua";
char * base_path_expanded;
char * rom_path_expanded;
char expand_tmp[32767];

std::vector<std::string> split(std::string strToSplit, char delimeter) 
{
    std::stringstream ss(strToSplit);
    std::string item;
    std::vector<std::string> splittedStrings;
    while (std::getline(ss, item, delimeter))
    {
       splittedStrings.push_back(item);
    }
    return splittedStrings;
}

std::wstring s2ws(const std::string& str) {
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

extern "C" {

char * expandEnvironment(const char * src) {
    if (base_path_expanded != NULL && std::string(src) == std::string(base_path)) return base_path_expanded;
	if (rom_path_expanded != NULL && std::string(src) == std::string(rom_path)) return rom_path_expanded;
	DWORD size = ExpandEnvironmentStringsA(src, expand_tmp, 32767);
	char* dest = (char*)malloc(size + 1);
	memcpy(dest, expand_tmp, size);
	dest[size] = 0;
	return dest;
}

char * fixpath(const char * path) {
    std::vector<std::string> elems = split(path, '/');
    std::vector<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") {if (pathc.size() < 1) return NULL; else pathc.pop_back();}
        else if (s != "." && s != "") pathc.push_back(s);
    }
    const char * bp = expandEnvironment((pathc.size() > 0 && pathc[0] == "rom") ? rom_path : base_path);
    std::stringstream ss;
    ss << bp;
    for (std::string s : pathc) ss << "\\" << s;
    //if (bp != base_path_expanded) free(bp);
    std::string retstr = ss.str();
    char * retval = (char*)malloc(retstr.size() + 1);
    strcpy(retval, retstr.c_str());
    //printf("%s\n", retval);
    return retval;
}

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

int getUptime() {
    return GetTickCount64() / 1000;
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
	PathRemoveFileSpecA(path);
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
}