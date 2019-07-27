#include "peripheral.h"
#include <string>
#include <vector>

#define PRINT_TYPE_PDF 0
#define PRINT_TYPE_HTML 1
#define PRINT_TYPE_TXT 2

#ifndef PRINT_TYPE
#define PRINT_TYPE PRINT_TYPE_PDF
#endif
#if PRINT_TYPE == PRINT_TYPE_PDF
#include <hpdf.h>
#elif PRINT_TYPE == PRINT_TYPE_HTML || PRINT_TYPE == PRINT_TYPE_TXT
#include <fstream>
#else
#error Unknown print type
#endif

class printer: public peripheral {
private:
    static library_t methods;
    static const int width = 25;
    static const int height = 21;
#if PRINT_TYPE == PRINT_TYPE_PDF
    HPDF_Doc out;
    HPDF_Page page;
#endif
    std::string outPath;
    std::string title;
    std::vector<std::vector<char> > body;
    int pageNumber = 0;
    int cursorX = 0;
    int cursorY = 0;
    bool started = false;

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
    void update() {}
    library_t getMethods() {return methods;}
};