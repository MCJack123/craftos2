# Initialize
AC_INIT([CraftOS-PC], [2.2])
AC_CONFIG_SRCDIR([src/main.cpp])
m4_include([m4/find_cxx.m4])
m4_include([m4/find_cxx11.m4])

# Options
AC_ARG_WITH(png, [AS_HELP_STRING([--with-png], [support PNG screenshots])])
AC_ARG_WITH(png, [AS_HELP_STRING([--without-png], [do not support PNG screenshots])])
AC_ARG_WITH(ncurses, [AS_HELP_STRING([--with-ncurses], [support CLI mode])])
AC_ARG_WITH(ncurses, [AS_HELP_STRING([--without-ncurses], [do not support CLI mode])])
AC_ARG_WITH(sdl_mixer, [AS_HELP_STRING([--with-sdl_mixer], [support audio disks])])
AC_ARG_WITH(sdl_mixer, [AS_HELP_STRING([--without-sdl_mixer], [do not support audio disks])])
AC_ARG_WITH(hpdf, [AS_HELP_STRING([--with-hpdf], [make printer output PDF files])])
AC_ARG_WITH(html, [AS_HELP_STRING([--with-html], [make printer output HTML files])])
AC_ARG_WITH(txt, [AS_HELP_STRING([--with-txt], [make printer output text files])])

if test "x$with_hpdf" = "xyes"; then PRINT_TYPE=0; fi
if test "x$with_hpdf" = "xno"; then PRINT_TYPE=1; fi
if test "x$with_html" = "xyes"; then
    AS_IF([test "x$PRINT_TYPE" != "x"], [AC_MSG_ERROR([Only one print type can be specified.])], [])
    PRINT_TYPE=1
fi
if test "x$with_txt" = "xyes"; then
    AS_IF([test "x$PRINT_TYPE" != "x"], [AC_MSG_ERROR([Only one print type can be specified.])], [])
    PRINT_TYPE=2
fi

# Check language
AC_LANG([C++])
CXXFLAGS="$CXXFLAGS -std=c++11"
CPPFLAGS="$CPPFLAGS -Icraftos2-lua/include -I/usr/local/opt/openssl/include"
LDFLAGS="$LDFLAGS -L/usr/local/opt/openssl/lib"
AC_PROG_CC
AC_PROG_CXX
AX_CHECK_COMPILE_FLAG([-std=c++11], [], [AC_MSG_ERROR([C++ compiler does not support -std=c++11.])])

# Find required libraries

AC_SEARCH_LIBS(pthread_create, pthread, [], [AC_MSG_ERROR([Could not find pthread library.])])
AC_SEARCH_LIBS(dlopen, dl, [], [AC_MSG_ERROR([Could not find dl library.])])

AC_CHECK_HEADERS(SDL2/SDL.h, [], [AC_MSG_ERROR([Could not find SDL2 header.])])
AC_SEARCH_LIBS(SDL_Init, SDL2, [], [AC_MSG_ERROR([Could not find SDL2 library.])])

AC_CHECK_HEADERS(openssl/opensslv.h, [], [AC_MSG_ERROR([Could not find OpenSSL headers.])])
AC_SEARCH_LIBS(SSL_new, ssl, [], [AC_MSG_ERROR([Could not find OpenSSL library.])])
AC_SEARCH_LIBS(RSA_new, crypto, [], [AC_MSG_ERROR([Could not find OpenSSL crypto library.])])

AC_CHECK_HEADERS(Poco/Foundation.h, [], [AC_MSG_ERROR([Could not find Poco Foundation headers.])])
AX_CXX_CHECK_LIB(PocoFoundation, [Poco::ASCIIEncoding], [], [AC_MSG_ERROR([Could not find Poco Foundation library.])])
AC_CHECK_HEADERS(Poco/Util/Util.h, [], [AC_MSG_ERROR([Could not find Poco Util headers.])])
AX_CXX_CHECK_LIB(PocoUtil, [Poco::Util::Timer], [], [AC_MSG_ERROR([Could not find Poco Util library.])])
AC_CHECK_HEADERS(Poco/XML/XML.h, [], [AC_MSG_ERROR([Could not find Poco XML headers.])])
AX_CXX_CHECK_LIB(PocoXML, [Poco::XML::Name], [], [AC_MSG_ERROR([Could not find Poco XML library.])])
AC_CHECK_HEADERS(Poco/JSON/JSON.h, [], [AC_MSG_ERROR([Could not find Poco JSON headers.])])
AX_CXX_CHECK_LIB(PocoJSON, [Poco::JSON::Template], [], [AC_MSG_ERROR([Could not find Poco JSON library.])])
AC_CHECK_HEADERS(Poco/Crypto/Crypto.h, [], [AC_MSG_ERROR([Could not find Poco Crypto headers.])])
AX_CXX_CHECK_LIB(PocoCrypto, [Poco::Crypto::CipherFactory], [], [AC_MSG_ERROR([Could not find Poco Crypto library.])])
AC_CHECK_HEADERS(Poco/Net/Net.h, [], [AC_MSG_ERROR([Could not find Poco Net headers.])])
AX_CXX_CHECK_LIB(PocoNet, [Poco::Net::HTTPRequest], [], [AC_MSG_ERROR([Could not find Poco Net library.])])
AC_CHECK_HEADERS(Poco/Net/NetSSL.h, [], [AC_MSG_ERROR([Could not find Poco NetSSL headers.])])
AX_CXX_CHECK_LIB(PocoNetSSL, [Poco::Net::HTTPSClientSession], [], [AC_MSG_ERROR([Could not find Poco NetSSL library.])])

# Check optional features

AC_SEARCH_LIBS(mpg123_init, mpg123)
AC_SEARCH_LIBS(FLAC__stream_decoder_new, flac)

AS_IF([test "x$with_png" != "xno"], [
    AC_CHECK_HEADERS([png++/png.hpp], [], [AS_IF([test "x$with_png" = "xyes"], [AC_MSG_ERROR([Could not find png++ header.])], [NO_PNG=1])])
    AC_SEARCH_LIBS(png_init_io, png, [], [AS_IF([test "x$with_png" = "xyes"], [AC_MSG_ERROR([Could not find libpng library.])], [NO_PNG=1])])
], [NO_PNG=1])

AS_IF([test "x$with_ncurses" != "xno"], [
    AC_CHECK_HEADERS(ncurses.h, [], [AS_IF([test "x$with_ncurses" = "xyes"], [AC_MSG_ERROR([Could not find ncurses header.])], [NO_CLI=1])])
    AC_SEARCH_LIBS(initscr, ncurses, [], [AS_IF([test "x$with_ncurses" = "xyes"], [AC_MSG_ERROR([Could not find ncurses library.])], [NO_CLI=1])])
], [NO_CLI=1])

AS_IF([test "x$with_sdl_mixer" != "xno"], [
	AC_CHECK_HEADERS(SDL2/SDL_mixer.h, [], [AS_IF([test "x$with_sdl_mixer" = "xyes"], [AC_MSG_ERROR([Could not find SDL2_mixer header.])], [NO_MIXER=1])])
	AC_SEARCH_LIBS(Mix_Init, SDL2_mixer, [], [AS_IF([test "x$with_sdl_mixer" = "xyes"], [AC_MSG_ERROR([Could not find SDL2_mixer library.])], [NO_MIXER=1])])
], [NO_MIXER=1])

AS_IF([test "x$PRINT_TYPE" = "x" || test "x$PRINT_TYPE" = "x0"], [
    AC_CHECK_HEADERS(hpdf.h, [], [AS_IF([test "x$PRINT_TYPE" = "x0"], [AC_MSG_ERROR([Could not find hpdf header.])], [PRINT_TYPE=1])])
    AC_SEARCH_LIBS(HPDF_New, hpdf, [], [AS_IF([test "x$PRINT_TYPE" = "x0"], [AC_MSG_ERROR([Could not find hpdf library.])], [PRINT_TYPE=1])])
], [])

if test "x$NO_PNG" = "x1"; then CPPFLAGS="$CPPFLAGS -DNO_PNG"; fi
if test "x$NO_CLI" = "x1"; then CPPFLAGS="$CPPFLAGS -DNO_CLI"; fi
if test "x$NO_MIXER" = "x1"; then CPPFLAGS="$CPPFLAGS -DNO_MIXER"; fi
if test "x$PRINT_TYPE" != "x"; then CPPFLAGS="$CPPFLAGS -DPRINT_TYPE=$PRINT_TYPE"; fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
