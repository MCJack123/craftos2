/*
 * peripheral/printer.cpp
 * CraftOS-PC 2
 * 
 * This file implements the methods for the printer peripheral.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2024 JackMacWindows.
 */

#include <cstring>
#include "../platform.hpp"
#include "printer.hpp"
#include "../util.hpp"

#if PRINT_TYPE == PRINT_TYPE_PDF
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include "../runtime.hpp"
#include "../terminal/RawTerminal.hpp"
#include "../terminal/TRoRTerminal.hpp"
static std::unordered_map<HPDF_STATUS, const char *> pdf_errors = {
    {HPDF_ARRAY_COUNT_ERR, "Internal error. Data consistency was lost."},
    {HPDF_ARRAY_ITEM_NOT_FOUND, "Internal error. Data consistency was lost."},
    {HPDF_ARRAY_ITEM_UNEXPECTED_TYPE, "Internal error. Data consistency was lost."},
    {HPDF_BINARY_LENGTH_ERR, "Data length > HPDF_LIMIT_MAX_STRING_LEN."},
    {HPDF_CANNOT_GET_PALLET, "Cannot get pallet data from PNG image."},
    {HPDF_DICT_COUNT_ERR, "Dictionary elements > HPDF_LIMIT_MAX_DICT_ELEMENT"},
    {HPDF_DICT_ITEM_NOT_FOUND, "Internal error. Data consistency was lost."},
    {HPDF_DICT_ITEM_UNEXPECTED_TYPE, "Internal error. Data consistency was lost."},
    {HPDF_DICT_STREAM_LENGTH_NOT_FOUND, "Internal error. Data consistency was lost."},
    {HPDF_DOC_ENCRYPTDICT_NOT_FOUND, "HPDF_SetEncryptMode() or HPDF_SetPermission() called before password set."},
    {HPDF_DOC_INVALID_OBJECT, "Internal error. Data consistency was lost."},
    {HPDF_DUPLICATE_REGISTRATION, "Tried to re-register a registered font."},
    {HPDF_EXCEED_JWW_CODE_NUM_LIMIT, "Cannot register a character to the Japanese word wrap characters list."},
    {HPDF_ENCRYPT_INVALID_PASSWORD, "1. Tried to set the owner password to NULL.\t2. Owner and user password are the same."},
    {HPDF_ERR_UNKNOWN_CLASS, "Internal error. Data consistency was lost."},
    {HPDF_EXCEED_GSTATE_LIMIT, "Stack depth > HPDF_LIMIT_MAX_GSTATE."},
#ifdef HPDF_FAILED_TO_ALLOC_MEM
    {HPDF_FAILED_TO_ALLOC_MEM, "Memory allocation failed."},
#else
    {HPDF_FAILD_TO_ALLOC_MEM, "Memory allocation failed."},
#endif
    {HPDF_FILE_IO_ERROR, "File processing failed. (Detailed code is set.)"},
    {HPDF_FILE_OPEN_ERROR, "Cannot open a file. (Detailed code is set.)"},
    {HPDF_FONT_EXISTS, "Tried to load a font that has been registered."},
    {HPDF_FONT_INVALID_WIDTHS_TABLE, "1. Font-file format is invalid.\t2. Internal error. Data consistency was lost."},
    {HPDF_INVALID_AFM_HEADER, "Cannot recognize header of afm file."},
    {HPDF_INVALID_ANNOTATION, "Specified annotation handle is invalid."},
    {HPDF_INVALID_BIT_PER_COMPONENT, "Bit-per-component of a image which was set as mask-image is invalid."},
    {HPDF_INVALID_CHAR_MATRICS_DATA, "Cannot recognize char-matrics-data of afm file."},
    {HPDF_INVALID_COLOR_SPACE, "1. Invalid color_space parameter of HPDF_LoadRawImage.\t2. Color-space of a image which was set as mask-image is invalid.\t3. Invoked function invalid in present color-space."},
    {HPDF_INVALID_COMPRESSION_MODE, "Invalid value set when invoking HPDF_SetCommpressionMode()."},
    {HPDF_INVALID_DATE_TIME, "An invalid date-time value was set."},
    {HPDF_INVALID_DESTINATION, "An invalid destination handle was set."},
    {HPDF_INVALID_DOCUMENT, "An invalid document handle was set."},
    {HPDF_INVALID_DOCUMENT_STATE, "Function invalid in the present state was invoked."},
    {HPDF_INVALID_ENCODER, "An invalid encoder handle was set."},
    {HPDF_INVALID_ENCODER_TYPE, "Combination between font and encoder is wrong."},
    {HPDF_INVALID_ENCODING_NAME, "An Invalid encoding name is specified."},
    {HPDF_INVALID_ENCRYPT_KEY_LEN, "Encryption key length is invalid."},
    {HPDF_INVALID_FONTDEF_DATA, "1. An invalid font handle was set.\t2. Unsupported font format."},
    {HPDF_INVALID_FONTDEF_TYPE, "Internal error. Data consistency was lost."},
    {HPDF_INVALID_FONT_NAME, "Font with the specified name is not found."},
    {HPDF_INVALID_IMAGE, "Unsupported image format."},
    {HPDF_INVALID_JPEG_DATA, "Unsupported image format."},
    {HPDF_INVALID_N_DATA, "Cannot read a postscript-name from an afm file."},
    {HPDF_INVALID_OBJECT, "1. An invalid object is set.\t2. Internal error. Data consistency was lost."},
    {HPDF_INVALID_OBJ_ID, "Internal error. Data consistency was lost."},
    {HPDF_INVALID_OPERATION, "Invoked HPDF_Image_SetColorMask() against the image-object which was set a mask-image."},
    {HPDF_INVALID_OUTLINE, "An invalid outline-handle was specified."},
    {HPDF_INVALID_PAGE, "An invalid page-handle was specified."},
    {HPDF_INVALID_PAGES, "An invalid pages-handle was specified. (internal error)"},
    {HPDF_INVALID_PARAMETER, "An invalid value is set."},
    {HPDF_INVALID_PNG_IMAGE, "Invalid PNG image format."},
    {HPDF_INVALID_STREAM, "Internal error. Data consistency was lost."},
    {HPDF_MISSING_FILE_NAME_ENTRY, "Internal error. \"_FILE_NAME\" entry for delayed loading is missing."},
    {HPDF_INVALID_TTC_FILE, "Invalid .TTC file format."},
    {HPDF_INVALID_TTC_INDEX, "Index parameter > number of included fonts."},
    {HPDF_INVALID_WX_DATA, "Cannot read a width-data from an afm file."},
    {HPDF_ITEM_NOT_FOUND, "Internal error. Data consistency was lost."},
    {HPDF_LIBPNG_ERROR, "Error returned from PNGLIB while loading image."},
    {HPDF_NAME_INVALID_VALUE, "Internal error. Data consistency was lost."},
    {HPDF_NAME_OUT_OF_RANGE, "Internal error. Data consistency was lost."},
    {HPDF_PAGES_MISSING_KIDS_ENTRY, "Internal error. Data consistency was lost."},
    {HPDF_PAGE_CANNOT_FIND_OBJECT, "Internal error. Data consistency was lost."},
    {HPDF_PAGE_CANNOT_GET_ROOT_PAGES, "Internal error. Data consistency was lost."},
    {HPDF_PAGE_CANNOT_RESTORE_GSTATE, "There are no graphics-states to be restored."},
    {HPDF_PAGE_CANNOT_SET_PARENT, "Internal error. Data consistency was lost."},
    {HPDF_PAGE_FONT_NOT_FOUND, "The current font is not set."},
    {HPDF_PAGE_INVALID_FONT, "An invalid font-handle was specified."},
    {HPDF_PAGE_INVALID_FONT_SIZE, "An invalid font-size was set."},
    {HPDF_PAGE_INVALID_GMODE, "See Graphics mode."},
    {HPDF_PAGE_INVALID_INDEX, "Internal error. Data consistency was lost."},
    {HPDF_PAGE_INVALID_ROTATE_VALUE, "Specified value is not multiple of 90."},
    {HPDF_PAGE_INVALID_SIZE, "An invalid page-size was set."},
    {HPDF_PAGE_INVALID_XOBJECT, "An invalid image-handle was set."},
    {HPDF_PAGE_OUT_OF_RANGE, "The specified value is out of range."},
    {HPDF_REAL_OUT_OF_RANGE, "The specified value is out of range."},
    {HPDF_STREAM_EOF, "Unexpected EOF marker was detected."},
    {HPDF_STREAM_READLN_CONTINUE, "Internal error. Data consistency was lost."},
    {HPDF_STRING_OUT_OF_RANGE, "The length of the text is too long."},
    {HPDF_THIS_FUNC_WAS_SKIPPED, "Function not executed because of other errors."},
    {HPDF_TTF_CANNOT_EMBEDDING_FONT, "Font cannot be embedded. (license restriction)"},
    {HPDF_TTF_INVALID_CMAP, "Unsupported ttf format. (cannot find unicode cmap)"},
    {HPDF_TTF_INVALID_FOMAT, "Unsupported ttf format."},
    {HPDF_TTF_MISSING_TABLE, "Unsupported ttf format. (cannot find a necessary table)"},
    {HPDF_UNSUPPORTED_FONT_TYPE, "Internal error. Data consistency was lost."},
    {HPDF_UNSUPPORTED_FUNC, "1. Library not configured to use PNGLIB.\t2. Internal error. Data consistency was lost."},
    {HPDF_UNSUPPORTED_JPEG_FORMAT, "Unsupported JPEG format."},
    {HPDF_UNSUPPORTED_TYPE1_FONT, "Failed to parse .PFB file."},
    {HPDF_XREF_COUNT_ERR, "Internal error. Data consistency was lost."},
    {HPDF_ZLIB_ERROR, "Error while executing ZLIB function."},
    {HPDF_INVALID_PAGE_INDEX, "An invalid page index was passed."},
    {HPDF_INVALID_URI, "An invalid URI was set."},
    //{HPDF_PAGELAYOUT_OUT_OF_RANGE, "An invalid page-layout was set."},
    //{HPDF_PAGEMODE_OUT_OF_RANGE, "An invalid page-mode was set."},
    //{HPDF_PAGENUM_STYLE_OUT_OF_RANGE, "An invalid page-num-style was set."},
    {HPDF_ANNOT_INVALID_ICON, "An invalid icon was set."},
    {HPDF_ANNOT_INVALID_BORDER_STYLE, "An invalid border-style was set."},
    {HPDF_PAGE_INVALID_DIRECTION, "An invalid page-direction was set."},
    {HPDF_INVALID_FONT, "An invalid font-handle was specified."},
};

void pdf_error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void* userdata) {
    lua_State *L = ((printer*)userdata)->currentState;
    if (L) luaL_error(L, "Error printing to PDF: %s (%d, %d)\n", pdf_errors[error_no], error_no, detail_no);
    else {
        const std::string e = "Error printing to PDF: " + std::string(pdf_errors[error_no]) + " (" + std::to_string(error_no) + ", " + std::to_string(detail_no) + ")";
        switch (selectedRenderer) {
            case 0: case 5: SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Printer Error", e.c_str(), NULL); break;
            case 1: case 2: fprintf(stderr, "%s\n", e.c_str()); break;
            case 3: RawTerminal::showGlobalMessage(SDL_MESSAGEBOX_ERROR, "Printer Error", e.c_str()); break;
            case 4: TRoRTerminal::showGlobalMessage(SDL_MESSAGEBOX_ERROR, "Printer Error", e.c_str()); break;
            default: break;
        }
        throw std::runtime_error(e);
    }
}
#elif PRINT_TYPE == PRINT_TYPE_HTML
std::string page_ext = ".html";
#else
std::string page_ext = ".txt";
#endif

printer::printer(lua_State *L, const char * side) {
    outPath = luaL_checkstring(L, 3);
#if PRINT_TYPE == PRINT_TYPE_PDF
    currentState = NULL;
    out = HPDF_New(pdf_error_handler, this);
    try {
        if (HPDF_SaveToFile(out, outPath.c_str()) != HPDF_OK) throw std::runtime_error("Couldn't open output file");
    } catch (...) {
        HPDF_Free(out);
        throw;
    }
#else
    fs::create_directories(outPath); // throw
#endif
}

printer::~printer() {
    if (started) endPage(NULL);
#if PRINT_TYPE == PRINT_TYPE_PDF
    HPDF_Free(out);
#endif
}

int printer::write(lua_State *L) {
    lastCFunction = __func__;
    if (cursorY >= height) return 0;
    size_t str_sz;
    const char * str = luaL_checklstring(L, 1, &str_sz);
    unsigned i;
    for (i = 0; i < str_sz && i + cursorX < width; i++) 
        body[cursorY][i+cursorX] = str[i] == '\n' ? '?' : str[i];
    cursorX += (int)i;
    return 0;
}

int printer::setCursorPos(lua_State *L) {
    lastCFunction = __func__;
    cursorX = (int)luaL_checkinteger(L, 1)-1;
    cursorY = (int)luaL_checkinteger(L, 2)-1;
    return 0;
}

int printer::getCursorPos(lua_State *L) {
    lastCFunction = __func__;
    lua_pushinteger(L, cursorX+1);
    lua_pushinteger(L, cursorY+1);
    return 2;
}

int printer::getPageSize(lua_State *L) {
    lastCFunction = __func__;
    lua_pushinteger(L, width);
    lua_pushinteger(L, height);
    return 2;
}

int printer::newPage(lua_State *L) {
    lastCFunction = __func__;
    if (started) {endPage(L); lua_pop(L, 1);}
    body = std::vector<std::vector<char> >(height, std::vector<char>(width, ' '));
    title = "";
    pageNumber++;
    cursorX = 0;
    cursorY = 0;
#if PRINT_TYPE == PRINT_TYPE_PDF
    currentState = L;
    page = HPDF_AddPage(out);
#endif
    started = true;
    lua_pushboolean(L, true);
    return 1;
}

int printer::endPage(lua_State *L) {
    lastCFunction = __func__;
    if (!started) {
        if (L) lua_pushboolean(L, false);
        return 1;
    }
#if PRINT_TYPE == PRINT_TYPE_PDF
    currentState = L;
    try {
        HPDF_Page_BeginText(page);
        HPDF_Page_SetFontAndSize(page, HPDF_GetFont(out, "Courier", "StandardEncoding"), 24.0);
        HPDF_Page_SetRGBFill(page, defaultPalette[inkColor].r / 255.0, defaultPalette[inkColor].g / 255.0, defaultPalette[inkColor].b / 255.0);
        for (unsigned i = 0; i < body.size(); i++) {
            char * str = new char[width + 1];
            memcpy(str, &body[i][0], width);
            str[width] = 0;
            HPDF_Page_TextOut(page, 72, HPDF_Page_GetHeight(page) - (HPDF_REAL)((i + 1) * 30 + 72), (const char *)str);
            delete[] str;
        }
        HPDF_Page_EndText(page);
        HPDF_SaveToFile(out, outPath.c_str());
    } catch (...) {
        if (L) lua_pushboolean(L, false);
        return 1;
    }
    page = NULL;
#else
    std::ofstream out(outPath + "/" + (title == "" ? "Page" + std::to_string(pageNumber) : title) + page_ext);
#if PRINT_TYPE == PRINT_TYPE_HTML
    out << "<html>\n\t";
    if (title != "") out << "<head>\n\t\t<title>" << title << "</title>\n\t</head>\n\t";
    out << "<body>\n\t\t<pre style=\"color: rgb(" << defaultPalette[inkColor].r << ", " << defaultPalette[inkColor].g << ", " << defaultPalette[inkColor].b << ")\">";
#endif
    for (std::vector<char> r : body) {
        for (char c : r) out.put(c);
        out.put('\n');
    }
#if PRINT_TYPE == PRINT_TYPE_HTML
    out << "\n\t\t</pre>\n\t</body>\n</html>";
#endif
    out.close();
#endif
    started = false;
    if (L) lua_pushboolean(L, true);
    return 1;
}

int printer::getInkLevel(lua_State *L) {
    lastCFunction = __func__;
    lua_pushinteger(L, INT32_MAX);
    return 1;
}

int printer::setPageTitle(lua_State *L) {
    lastCFunction = __func__;
    title = checkstring(L, 1);
    return 0;
}

int printer::getPaperLevel(lua_State *L) {
    lastCFunction = __func__;
    lua_pushinteger(L, INT32_MAX);
    return 1;
}

int printer::getInkColor(lua_State *L) {
    lastCFunction = __func__;
    lua_pushinteger(L, 2^inkColor);
    return 1;
}

int printer::setInkColor(lua_State *L) {
    lastCFunction = __func__;
    int color = log2i((int)luaL_checkinteger(L, 1));
    if (color > 15) luaL_error(L, "bad argument #1 (color out of range)");
    inkColor = color;
    return 0;
}

int printer::call(lua_State *L, const char * method) {
    const std::string m(method);
    if (m == "write") return write(L);
    else if (m == "setCursorPos") return setCursorPos(L);
    else if (m == "getCursorPos") return getCursorPos(L);
    else if (m == "getPageSize") return getPageSize(L);
    else if (m == "newPage") return newPage(L);
    else if (m == "endPage") return endPage(L);
    else if (m == "getInkLevel") return getInkLevel(L);
    else if (m == "setPageTitle") return setPageTitle(L);
    else if (m == "getPaperLevel") return getPaperLevel(L);
    else if (m == "getInkColor" || m == "getInkColour") return getInkColor(L);
    else if (m == "setInkColor" || m == "setInkColour") return setInkColor(L);
    else return luaL_error(L, "No such method");
}

static luaL_Reg printer_reg[] = {
    {"write", NULL},
    {"setCursorPos", NULL},
    {"getCursorPos", NULL},
    {"getPageSize", NULL},
    {"newPage", NULL},
    {"endPage", NULL},
    {"getInkLevel", NULL},
    {"setPageTitle", NULL},
    {"getPaperLevel", NULL},
    {"getInkColor", NULL},
    {"getInkColour", NULL},
    {"setInkColor", NULL},
    {"setInkColour", NULL},
    {NULL, NULL}
};

library_t printer::methods = {"printer", printer_reg, nullptr, nullptr};