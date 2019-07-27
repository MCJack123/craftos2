#include "platform.h"
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
#include <windows.h>
#include <processenv.h>

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

extern "C" {

char * expandEnvironment(const char * src) {
    if (base_path_expanded != NULL && std::string(src) == std::string(base_path)) return base_path_expanded;

}

char * fixpath(const char * path) {
    std::vector<std::string> elems = split(path, '/');
    std::vector<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") {if (pathc.size() < 1) return NULL; else pathc.pop_back();}
        else if (s != "." && s != "") pathc.push_back(s);
    }
    const char * bp = (pathc.size() > 0 && pathc[0] == "rom") ? rom_path : expandEnvironment(base_path);
    std::stringstream ss;
    ss << bp;
    for (std::string s : pathc) ss << "/" << s;
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

void msleep(unsigned long time) {
    Sleep(time);
}
}