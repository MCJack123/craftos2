#ifndef CONFIG_H
#define CONFIG_H
#ifdef __cplusplus
extern "C" {
#endif
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
    bool ignoreHotkeys;
};
struct computer_configuration {
    const char * label;
    bool isColor;
};
extern struct configuration config;
extern struct computer_configuration getComputerConfig(int id);
extern void setComputerConfig(int id, struct computer_configuration cfg);
#ifdef __cplusplus
}
#endif
#endif