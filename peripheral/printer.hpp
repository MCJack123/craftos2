#include "peripheral.h"
#include <string>
#include <vector>
#ifndef PRINT_TYPE
#define PRINT_TYPE pdf
#endif
#if PRINT_TYPE == pdf
#include <hpdf.h>
#elif PRINT_TYPE == html || PRINT_TYPE == txt
#include <fstream>
#else
#error Unknown print type
#endif

class printer: public peripheral {
private:
    static library_t methods;
    static const int width = 25;
    static const int height = 21;
#if PRINT_TYPE == pdf
    HPDF_Doc out;
#else
    std::ofstream out;
#endif
    std::string title;
    std::vector<std::vector<char> > body;
    int cursorX = 0;
    int cursorY = 0;

    int write(lua_State *L);
    int setCursorPos(lua_State *L);
    int getCursorPos(lua_State *L);
    int getPageSize(lua_State *L);
    int newPage(lua_State *L);
    int endPage(lua_State *L);
    int getInkLevel(lua_State *L);
    int setPageTitle(lua_State *L);
    int getPaperLevel(lua_State *L);

public:
    printer(lua_State *L, const char * side);
    ~printer();
    int call(lua_State *L, const char * method);
    void update();
    library_t getMethods() {return methods;}
};