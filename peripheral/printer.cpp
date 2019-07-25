#include "printer.hpp"

printer::printer(lua_State *L, const char * side) {
    if (!lua_isstring(L, 3)) bad_argument(L, "string", 3);
    
}

int printer::write(lua_State *L) {
    if (!lua_isstring(L, 1)) bad_argument(L, "string", 1);
    
    return 0;
}

int printer::setCursorPos(lua_State *L) {

}

int printer::getCursorPos(lua_State *L) {

}

int printer::getPageSize(lua_State *L) {

}

int printer::newPage(lua_State *L) {

}

int printer::endPage(lua_State *L) {

}

int printer::getInkLevel(lua_State *L) {

}

int printer::setPageTitle(lua_State *L) {

}

int printer::getPaperLevel(lua_State *L) {

}

const char * printer_keys[9] = {
    "write",
    "setCursorPos",
    "getCursorPos",
    "getPageSize",
    "newPage",
    "endPage",
    "getInkLevel",
    "setPageTitle",
    "getPaperLevel"
};

library_t printer::methods = {"printer", 9, printer_keys, NULL};