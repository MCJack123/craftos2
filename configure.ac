AC_INIT([CraftOS-PC], [2.1])
AC_CONFIG_SRCDIR([src/main.cpp])
m4_include([m4/find_cxx.m4])

AC_LANG([C++])
CXXFLAGS="$CXXFLAGS -std=c++11"
AC_PROG_CC
AC_PROG_CXX

AC_SEARCH_LIBS(pthread_create, pthread, [], [AC_MSG_ERROR([Could not find pthread library.])])
AC_SEARCH_LIBS(dlopen, dl, [], [AC_MSG_ERROR([Could not find dl library.])])

AC_CHECK_HEADERS(lua.h lualib.h lauxlib.h, [], [AC_MSG_ERROR([Could not find Lua 5.1 headers.])])
AC_SEARCH_LIBS(luaL_register, lua5.1, [], [AC_MSG_ERROR([Could not find Lua 5.1.])])

AC_CHECK_HEADERS(SDL2/SDL.h, [], [AC_MSG_ERROR([Could not find SDL2 header.])])
AC_SEARCH_LIBS(SDL_Init, SDL2, [], [AC_MSG_ERROR([Could not find SDL2 library.])])

AC_CHECK_HEADERS(SDL2/SDL_mixer.h, [], [AC_MSG_ERROR([Could not find SDL_mixer header.])])
AC_SEARCH_LIBS(Mix_Init, SDL2_mixer, [], [AC_MSG_ERROR([Could not find SDL2_mixer library.])])

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

AC_SEARCH_LIBS(mpg123_init, mpg123)
AC_SEARCH_LIBS(FLAC__stream_decoder_new, flac)

AC_CHECK_HEADERS([png++/png.hpp], [], [NO_PNG=1])
AC_SEARCH_LIBS(png_init_io, png, [], [NO_PNG=1])

AC_CHECK_HEADERS(hpdf.h, [], [PRINT_TYPE=1])
AC_SEARCH_LIBS(HPDF_New, hpdf, [], [PRINT_TYPE=1])

AC_CHECK_HEADERS(ncurses.h, [], [NO_CLI=1])
AC_SEARCH_LIBS(initscr, ncurses, [], [NO_CLI=1])

if test "x$NO_PNG" = "x1"; then CPPFLAGS="$CPPFLAGS -DNO_PNG"; fi
if test "x$NO_CLI" = "x1"; then CPPFLAGS="$CPPFLAGS -DNO_CLI"; fi
if test "x$PRINT_TYPE" != "x"; then CPPFLAGS="$CPPFLAGS -DPRINT_TYPE=$PRINT_TYPE"; fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
