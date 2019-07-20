extern "C" {
#include "platform.h"
}
#ifdef __WIN32
#include "platform_win.cpp"
#else
#ifdef __APPLE__
#include "platform_darwin.cpp"
#else
#ifdef __linux__
#include "platform_linux.cpp"
#else
#error Unknown platform
#endif
#endif
#endif