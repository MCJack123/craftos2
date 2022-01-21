/*
 * peripheral/printer.hpp
 * CraftOS-PC 2
 * 
 * This file defines the class for the printer peripheral.
 * 
 * This code is licensed under the MIT license.
 * Copyright (c) 2019-2022 JackMacWindows.
 */

#ifndef PERIPHERAL_PRINTER_HPP
#define PERIPHERAL_PRINTER_HPP
#include <string>
#include <vector>
#include <peripheral.hpp>

#define PRINT_TYPE_PDF 0
#define PRINT_TYPE_HTML 1
#define PRINT_TYPE_TXT 2

#ifndef PRINT_TYPE
#define PRINT_TYPE PRINT_TYPE_PDF
#endif
#if PRINT_TYPE == PRINT_TYPE_PDF
#include <hpdf.h>
#ifdef HPDF_SHARED
// this cleans up some rough bits left in shared builds of libharu
#define HPDF_ARRAY_COUNT_ERR                      0x1001
#define HPDF_ARRAY_ITEM_NOT_FOUND                 0x1002
#define HPDF_ARRAY_ITEM_UNEXPECTED_TYPE           0x1003
#define HPDF_BINARY_LENGTH_ERR                    0x1004
#define HPDF_CANNOT_GET_PALLET                    0x1005
#define HPDF_DICT_COUNT_ERR                       0x1007
#define HPDF_DICT_ITEM_NOT_FOUND                  0x1008
#define HPDF_DICT_ITEM_UNEXPECTED_TYPE            0x1009
#define HPDF_DICT_STREAM_LENGTH_NOT_FOUND         0x100A
#define HPDF_DOC_ENCRYPTDICT_NOT_FOUND            0x100B
#define HPDF_DOC_INVALID_OBJECT                   0x100C
 /*                                                0x100D */
#define HPDF_DUPLICATE_REGISTRATION               0x100E
#define HPDF_EXCEED_JWW_CODE_NUM_LIMIT            0x100F
/*                                                0x1010 */
#define HPDF_ENCRYPT_INVALID_PASSWORD             0x1011
/*                                                0x1012 */
#define HPDF_ERR_UNKNOWN_CLASS                    0x1013
#define HPDF_EXCEED_GSTATE_LIMIT                  0x1014
#define HPDF_FAILD_TO_ALLOC_MEM                   0x1015
#define HPDF_FILE_IO_ERROR                        0x1016
#define HPDF_FILE_OPEN_ERROR                      0x1017
/*                                                0x1018 */
#define HPDF_FONT_EXISTS                          0x1019
#define HPDF_FONT_INVALID_WIDTHS_TABLE            0x101A
#define HPDF_INVALID_AFM_HEADER                   0x101B
#define HPDF_INVALID_ANNOTATION                   0x101C
/*                                                0x101D */
#define HPDF_INVALID_BIT_PER_COMPONENT            0x101E
#define HPDF_INVALID_CHAR_MATRICS_DATA            0x101F
#define HPDF_INVALID_COLOR_SPACE                  0x1020
#define HPDF_INVALID_COMPRESSION_MODE             0x1021
#define HPDF_INVALID_DATE_TIME                    0x1022
#define HPDF_INVALID_DESTINATION                  0x1023
/*                                                0x1024 */
#define HPDF_INVALID_DOCUMENT                     0x1025
#define HPDF_INVALID_DOCUMENT_STATE               0x1026
#define HPDF_INVALID_ENCODER                      0x1027
#define HPDF_INVALID_ENCODER_TYPE                 0x1028
/*                                                0x1029 */
/*                                                0x102A */
#define HPDF_INVALID_ENCODING_NAME                0x102B
#define HPDF_INVALID_ENCRYPT_KEY_LEN              0x102C
#define HPDF_INVALID_FONTDEF_DATA                 0x102D
#define HPDF_INVALID_FONTDEF_TYPE                 0x102E
#define HPDF_INVALID_FONT_NAME                    0x102F
#define HPDF_INVALID_IMAGE                        0x1030
#define HPDF_INVALID_JPEG_DATA                    0x1031
#define HPDF_INVALID_N_DATA                       0x1032
#define HPDF_INVALID_OBJECT                       0x1033
#define HPDF_INVALID_OBJ_ID                       0x1034
#define HPDF_INVALID_OPERATION                    0x1035
#define HPDF_INVALID_OUTLINE                      0x1036
#define HPDF_INVALID_PAGE                         0x1037
#define HPDF_INVALID_PAGES                        0x1038
#define HPDF_INVALID_PARAMETER                    0x1039
/*                                                0x103A */
#define HPDF_INVALID_PNG_IMAGE                    0x103B
#define HPDF_INVALID_STREAM                       0x103C
#define HPDF_MISSING_FILE_NAME_ENTRY              0x103D
/*                                                0x103E */
#define HPDF_INVALID_TTC_FILE                     0x103F
#define HPDF_INVALID_TTC_INDEX                    0x1040
#define HPDF_INVALID_WX_DATA                      0x1041
#define HPDF_ITEM_NOT_FOUND                       0x1042
#define HPDF_LIBPNG_ERROR                         0x1043
#define HPDF_NAME_INVALID_VALUE                   0x1044
#define HPDF_NAME_OUT_OF_RANGE                    0x1045
/*                                                0x1046 */
/*                                                0x1047 */
#define HPDF_PAGE_INVALID_PARAM_COUNT             0x1048
#define HPDF_PAGES_MISSING_KIDS_ENTRY             0x1049
#define HPDF_PAGE_CANNOT_FIND_OBJECT              0x104A
#define HPDF_PAGE_CANNOT_GET_ROOT_PAGES           0x104B
#define HPDF_PAGE_CANNOT_RESTORE_GSTATE           0x104C
#define HPDF_PAGE_CANNOT_SET_PARENT               0x104D
#define HPDF_PAGE_FONT_NOT_FOUND                  0x104E
#define HPDF_PAGE_INVALID_FONT                    0x104F
#define HPDF_PAGE_INVALID_FONT_SIZE               0x1050
#define HPDF_PAGE_INVALID_GMODE                   0x1051
#define HPDF_PAGE_INVALID_INDEX                   0x1052
#define HPDF_PAGE_INVALID_ROTATE_VALUE            0x1053
#define HPDF_PAGE_INVALID_SIZE                    0x1054
#define HPDF_PAGE_INVALID_XOBJECT                 0x1055
#define HPDF_PAGE_OUT_OF_RANGE                    0x1056
#define HPDF_REAL_OUT_OF_RANGE                    0x1057
#define HPDF_STREAM_EOF                           0x1058
#define HPDF_STREAM_READLN_CONTINUE               0x1059
/*                                                0x105A */
#define HPDF_STRING_OUT_OF_RANGE                  0x105B
#define HPDF_THIS_FUNC_WAS_SKIPPED                0x105C
#define HPDF_TTF_CANNOT_EMBEDDING_FONT            0x105D
#define HPDF_TTF_INVALID_CMAP                     0x105E
#define HPDF_TTF_INVALID_FOMAT                    0x105F
#define HPDF_TTF_MISSING_TABLE                    0x1060
#define HPDF_UNSUPPORTED_FONT_TYPE                0x1061
#define HPDF_UNSUPPORTED_FUNC                     0x1062
#define HPDF_UNSUPPORTED_JPEG_FORMAT              0x1063
#define HPDF_UNSUPPORTED_TYPE1_FONT               0x1064
#define HPDF_XREF_COUNT_ERR                       0x1065
#define HPDF_ZLIB_ERROR                           0x1066
#define HPDF_INVALID_PAGE_INDEX                   0x1067
#define HPDF_INVALID_URI                          0x1068
#define HPDF_PAGE_LAYOUT_OUT_OF_RANGE             0x1069
#define HPDF_PAGE_MODE_OUT_OF_RANGE               0x1070
#define HPDF_PAGE_NUM_STYLE_OUT_OF_RANGE          0x1071
#define HPDF_ANNOT_INVALID_ICON                   0x1072
#define HPDF_ANNOT_INVALID_BORDER_STYLE           0x1073
#define HPDF_PAGE_INVALID_DIRECTION               0x1074
#define HPDF_INVALID_FONT                         0x1075
#define HPDF_PAGE_INSUFFICIENT_SPACE              0x1076
#define HPDF_PAGE_INVALID_DISPLAY_TIME            0x1077
#define HPDF_PAGE_INVALID_TRANSITION_TIME         0x1078
#define HPDF_INVALID_PAGE_SLIDESHOW_TYPE          0x1079
#define HPDF_EXT_GSTATE_OUT_OF_RANGE              0x1080
#define HPDF_INVALID_EXT_GSTATE                   0x1081
#define HPDF_EXT_GSTATE_READ_ONLY                 0x1082
#define HPDF_INVALID_U3D_DATA                     0x1083
#define HPDF_NAME_CANNOT_GET_NAMES                0x1084
#define HPDF_INVALID_ICC_COMPONENT_NUM            0x1085
/*                                                0x1086 */
/*                                                0x1087 */
#define HPDF_INVALID_SHADING_TYPE                 0x1088
#endif /* HPDF_SHARED */
#elif PRINT_TYPE == PRINT_TYPE_HTML || PRINT_TYPE == PRINT_TYPE_TXT
#include <fstream>
#else
#error Unknown print type
#endif /* PRINT_TYPE */

class printer: public peripheral {
private:
    static library_t methods;
    static const int width = 25;
    static const int height = 21;
#if PRINT_TYPE == PRINT_TYPE_PDF
    friend void pdf_error_handler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void* userdata);
    HPDF_Doc out;
    HPDF_Page page;
    lua_State *currentState;
#endif
    std::string outPath;
    std::string title;
    std::vector<std::vector<char> > body;
    int pageNumber = 0;
    int cursorX = 0;
    int cursorY = 0;
    bool started = false;
    int inkColor = 15;

    int write(lua_State *L);
    int setCursorPos(lua_State *L);
    int getCursorPos(lua_State *L);
    int getPageSize(lua_State *L);
    int newPage(lua_State *L);
    int endPage(lua_State *L);
    int getInkLevel(lua_State *L);
    int setPageTitle(lua_State *L);
    int getPaperLevel(lua_State *L);
    int getInkColor(lua_State *L);
    int setInkColor(lua_State *L);

public:
    static peripheral * init(lua_State *L, const char * side) {return new printer(L, side);}
    static void deinit(peripheral * p) {delete (printer*)p;}
    destructor getDestructor() const override {return deinit;}
    printer(lua_State *L, const char * side);
    ~printer();
    int call(lua_State *L, const char * method) override;
    library_t getMethods() const override {return methods;}
};

#endif