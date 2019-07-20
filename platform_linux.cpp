#include <lua.h>
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#include <stdio.h>
#include <unistd.h>
}
#include <string>
#include <vector>
#include <sstream>
extern "C" {

const char * rom_path = "/usr/share/craftos";
#ifdef FS_ROOT
const char * base_path = "";
#else
const char * base_path = "$HOME/.craftos/computer/0";
#endif

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

char * fixpath(const char * path) {
    std::vector<std::string> elems = split(path, '/');
    std::vector<std::string> pathc;
    for (std::string s : elems) {
        if (s == "..") {if (pathc.size() < 1) return NULL; else pathc.pop_back();}
        else if (s != "." && s != "") pathc.push_back(s);
    }
    char * bp = expandEnvironment((pathc.size() > 0 && pathc[0] == "rom") ? rom_path : base_path);
    std::stringstream ss;
    ss << bp;
    for (std::string s : pathc) ss << "/" << s;
    free(bp);
    std::string retstr = ss.str();
    char * retval = (char*)malloc(retstr.size() + 1);
    strcpy(retval, retstr.c_str());
    //printf("%s\n", retval);
    return retval;
}

char * expandEnvironment(const char * src) {
    wordexp_t p;
    wordexp(src, &p, 0);
    int size = 0;
    for (int i = 0; i < p.we_wordc; i++) size += strlen(p.we_wordv[i]);
    char * retval = (char*)malloc(size + 1);
    strcpy(retval, p.we_wordv[0]);
    for (int i = 1; i < p.we_wordc; i++) strcat(retval, p.we_wordv[i]);
    return retval;
}