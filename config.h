#include "lib.h"
#include <stdbool.h>
extern library_t config_lib;
struct configuration {
    bool http_enable;
    //String[] http_whitelist;
    //String[] http_blacklist;
    bool disable_lua51_features;
    const char * default_computer_settings;
    bool logPeripheralErrors;
    bool showFPS;
    bool readFail;
    int computerSpaceLimit;
    int maximumFilesOpen;
    int abortTimeout;
    int maxNotesPerTick;
    int clockSpeed;
};
extern struct configuration config;