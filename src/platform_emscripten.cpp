#include "platform.hpp"
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dirent.h>

void setThreadName(std::thread &t, std::string name) {}

unsigned long long getFreeSpace(std::string path) {
    return 1000000;
}

int createDirectory(std::string path) {
    if (mkdir(path.c_str(), 0777) != 0) {
        if (errno == ENOENT && path != "/") {
            if (createDirectory(path.substr(0, path.find_last_of('/')).c_str())) return 1;
            mkdir(path.c_str(), 0777);
        } else if (errno != EEXIST) return 1;
    }
    return 0;
}

int removeDirectory(std::string path) {
    struct stat statbuf;
    if (!stat(path.c_str(), &statbuf)) {
        if (S_ISDIR(statbuf.st_mode)) {
            DIR *d = opendir(path.c_str());
            int r = -1;
            if (d) {
                struct dirent *p;
                r = 0;
                while (!r && (p=readdir(d))) {
                    /* Skip the names "." and ".." as we don't want to recurse on them. */
                    if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) continue;
                    r = removeDirectory(path + "/" + std::string(p->d_name));
                }
                closedir(d);
            }
            if (!r) r = rmdir(path.c_str());
            return r;
        } else return unlink(path.c_str());
    } else return -1;
}

void setBasePath(const char * path) {}

std::string getBasePath() {
    return "/user-data";
}

std::string getROMPath() {
    return "/craftos";
}

std::string getPlugInPath() {
    return "/user-data/plugins";
}

void updateNow(std::string tag_name) {}

void migrateData() {}

void * loadSymbol(std::string path, std::string symbol) {
    return NULL;
}

void unloadLibraries() {}

void copyImage(SDL_Surface* surf) {

}
