# Initialize
AC_INIT([CraftOS-PC], [2.8])
AC_CONFIG_SRCDIR([src/main.cpp])
m4_include([m4/find_cxx.m4])
m4_include([m4/find_cxx11.m4])

# Options
AC_ARG_WITH(png, [AS_HELP_STRING([--with-png], [support PNG screenshots])])
AC_ARG_WITH(png, [AS_HELP_STRING([--without-png], [do not support PNG screenshots])])
AC_ARG_WITH(webp, [AS_HELP_STRING([--with-webp], [support WebP screenshots/recordings])])
AC_ARG_WITH(webp, [AS_HELP_STRING([--without-webp], [do not support WebP screenshots/recordings])])
AC_ARG_WITH(ncurses, [AS_HELP_STRING([--with-ncurses], [support CLI mode])])
AC_ARG_WITH(ncurses, [AS_HELP_STRING([--without-ncurses], [do not support CLI mode])])
AC_ARG_WITH(sdl_mixer, [AS_HELP_STRING([--with-sdl_mixer], [support audio disks])])
AC_ARG_WITH(sdl_mixer, [AS_HELP_STRING([--without-sdl_mixer], [do not support audio disks])])
AC_ARG_WITH(hpdf, [AS_HELP_STRING([--with-hpdf], [make printer output PDF files])])
AC_ARG_WITH(html, [AS_HELP_STRING([--with-html], [make printer output HTML files])])
AC_ARG_WITH(txt, [AS_HELP_STRING([--with-txt], [make printer output text files])])
AC_ARG_WITH(standalone_rom, [AS_HELP_STRING([--with-standalone_rom], [path to standalone ROM file, enables standalone build])])
AC_ARG_ENABLE(wasm, [AS_HELP_STRING([--enable-wasm], [compile for WebAssembly using Emscripten])])
AC_ARG_WITH(wasm_poco, [AS_HELP_STRING([--with-wasm-poco], [path to Poco build for WASM])])
AC_ARG_WITH(wasm_rom, [AS_HELP_STRING([--with-wasm-rom], [path to ROM for WASM builds])])

if test "x$CC" = "xemcc"; then enable_wasm=yes; fi
if test "x$CXX" = "xem++"; then enable_wasm=yes; fi

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

if test "x$enable_wasm" = "xyes"; then
    # Set up compiler for emcc/em++
    AS_IF([test "x$with_wasm_poco" = "x"], [AC_MSG_ERROR([--with-wasm-poco must be specified when building for WASM.])])
    AS_IF([test "x$with_wasm_rom" = "x"], [AC_MSG_ERROR([--with-wasm-rom must be specified when building for WASM.])])
    SQBROP=@<:@
    SQBRCL=@:>@
    CC=emcc
    CXX=em++
    CPPFLAGS="$CPPFLAGS -I$with_wasm_poco/include -Icraftos2-lua/include -Iapi -DNO_CLI -DNO_PNG -DNO_WEBP -DPRINT_TYPE=1"
    CXXFLAGS="$CXXFLAGS -std=c++17 -s USE_SDL=2 -s USE_PTHREADS=1 -s USE_SDL_MIXER=2 -Wno-implicit-const-int-float-conversion -Wno-pthreads-mem-growth"
    LDFLAGS="$LDFLAGS -L$with_wasm_poco/lib -lidbfs.js -s USE_SDL=2 -s USE_SDL_MIXER=2 -s ERROR_ON_UNDEFINED_SYMBOLS=0 -s TOTAL_MEMORY=33554432 -s DISABLE_EXCEPTION_CATCHING=0 -s FETCH=1 -s DEMANGLE_SUPPORT=1 -s EXTRA_EXPORTED_RUNTIME_METHODS=$SQBROP\"ccall\",\"cwrap\"$SQBRCL -s ASYNCIFY -s ALLOW_MEMORY_GROWTH=1 --no-heap-copy -Wno-pthreads-mem-growth"
fi

# Check language
AC_LANG([C++])
CXXFLAGS="$CXXFLAGS -std=c++17"
CPPFLAGS="$CPPFLAGS -Icraftos2-lua/include -I/usr/local/opt/openssl/include"
LDFLAGS="$LDFLAGS -L/usr/local/opt/openssl/lib"
AC_PROG_CC
AC_PROG_CXX
AX_CHECK_COMPILE_FLAG([-std=c++17], [], [AC_MSG_ERROR([C++ compiler does not support -std=c++17.])])

# Find required libraries

if test "x$enable_wasm" != "xyes"; then
    AC_SEARCH_LIBS(pthread_create, pthread, [], [AC_MSG_ERROR([Could not find pthread library.])])
    AC_SEARCH_LIBS(dlopen, dl, [], [AC_MSG_ERROR([Could not find dl library.])])
    AC_SEARCH_LIBS(SDL_Init, SDL2, [], [AC_MSG_ERROR([Could not find SDL2 library.])])
    AC_SEARCH_LIBS(SSL_new, ssl, [], [AC_MSG_ERROR([Could not find OpenSSL library.])])
    AC_SEARCH_LIBS(RSA_new, crypto, [], [AC_MSG_ERROR([Could not find OpenSSL crypto library.])])
fi

AC_CHECK_HEADERS(SDL2/SDL.h, [], [AC_MSG_ERROR([Could not find SDL2 header.])])
AC_CHECK_HEADERS(openssl/opensslv.h, [], [AC_MSG_ERROR([Could not find OpenSSL headers.])])
AC_CHECK_HEADERS(Poco/Foundation.h, [], [AC_MSG_ERROR([Could not find Poco Foundation headers.])])
AC_CHECK_HEADERS(Poco/Util/Util.h, [], [AC_MSG_ERROR([Could not find Poco Util headers.])])
AC_CHECK_HEADERS(Poco/XML/XML.h, [], [AC_MSG_ERROR([Could not find Poco XML headers.])])
AC_CHECK_HEADERS(Poco/JSON/JSON.h, [], [AC_MSG_ERROR([Could not find Poco JSON headers.])])
AC_CHECK_HEADERS(Poco/Crypto/Crypto.h, [], [AC_MSG_ERROR([Could not find Poco Crypto headers.])])
AC_CHECK_HEADERS(Poco/Net/Net.h, [], [AC_MSG_ERROR([Could not find Poco Net headers.])])
AC_CHECK_HEADERS(Poco/Net/NetSSL.h, [], [AC_MSG_ERROR([Could not find Poco NetSSL headers.])])

if test "x$enable_wasm" = "xyes"; then
    LDFLAGS="$LDFLAGS -s USE_PTHREADS=1 --embed-file $with_wasm_rom@/craftos --shared-memory"
    LIBS="$LIBS -lPocoJSON -lPocoUtil -lPocoFoundation"
    AC_SUBST(OUT_TARGET, craftos.html)
    AC_SUBST(HTTP_TARGET, "apis_http_emscripten.o")
else
    AX_CXX_CHECK_LIB(PocoFoundation, [Poco::ASCIIEncoding], [], [AC_MSG_ERROR([Could not find Poco Foundation library.])])
    AX_CXX_CHECK_LIB(PocoUtil, [Poco::Util::Timer], [], [AC_MSG_ERROR([Could not find Poco Util library.])])
    AX_CXX_CHECK_LIB(PocoXML, [Poco::XML::Name], [], [AC_MSG_ERROR([Could not find Poco XML library.])])
    AX_CXX_CHECK_LIB(PocoJSON, [Poco::JSON::Template], [], [AC_MSG_ERROR([Could not find Poco JSON library.])])
    AX_CXX_CHECK_LIB(PocoCrypto, [Poco::Crypto::CipherFactory], [], [AC_MSG_ERROR([Could not find Poco Crypto library.])])
    AX_CXX_CHECK_LIB(PocoNet, [Poco::Net::HTTPRequest], [], [AC_MSG_ERROR([Could not find Poco Net library.])])
    AX_CXX_CHECK_LIB(PocoNetSSL, [Poco::Net::HTTPSClientSession], [], [AC_MSG_ERROR([Could not find Poco NetSSL library.])])
    AC_SUBST(OUT_TARGET, craftos)
    AC_SUBST(HTTP_TARGET, "apis_http_handle.o apis_http.o")
fi

# Find standalone ROM if supplied

if test "x$with_standalone_rom" != "x"; then
    if test ! -e "$with_standalone_rom"; then
        AC_MSG_ERROR([Could not find ROM file specified.])
    fi
    cp "$with_standalone_rom" "src/fs_standalone.cpp"
    CPPFLAGS="$CPPFLAGS -DSTANDALONE_ROM"
    AC_SUBST(OBJS, fs_standalone.o)
fi

# Check optional features

if test "x$enable_wasm" != "xyes"; then

    AS_IF([test "x$with_png" != "xno"], [
        AC_CHECK_HEADERS([png++/png.hpp], [], [AS_IF([test "x$with_png" = "xyes"], [AC_MSG_ERROR([Could not find png++ header.])], [NO_PNG=1])])
        AC_SEARCH_LIBS(png_init_io, png, [], [AS_IF([test "x$with_png" = "xyes"], [AC_MSG_ERROR([Could not find libpng library.])], [NO_PNG=1])])
    ], [NO_PNG=1])

    AS_IF([test "x$with_webp" != "xno"], [
        AC_CHECK_HEADERS([webp/mux.h], [], [AS_IF([test "x$with_webp" = "xyes"], [AC_MSG_ERROR([Could not find webp header.])], [NO_WEBP=1])])
        AC_SEARCH_LIBS(WebPEncodeRGB, webp, [], [AS_IF([test "x$with_webp" = "xyes"], [AC_MSG_ERROR([Could not find libwebp library.])], [NO_WEBP=1])])
        AC_SEARCH_LIBS(WebPAnimEncoderNewInternal, webpmux, [], [AS_IF([test "x$with_webp" = "xyes"], [AC_MSG_ERROR([Could not find libwebp library.])], [NO_WEBP=1])])
    ], [NO_WEBP=1])

    AS_IF([test "x$with_ncurses" != "xno"], [
        AC_CHECK_HEADERS(ncurses.h, [], [AS_IF([test "x$with_ncurses" = "xyes"], [AC_MSG_ERROR([Could not find ncurses header.])], [NO_CLI=1])])
        AC_SEARCH_LIBS(initscr, ncursesw ncurses, [], [AS_IF([test "x$with_ncurses" = "xyes"], [AC_MSG_ERROR([Could not find ncurses library.])], [NO_CLI=1])])
    ], [NO_CLI=1])

    AS_IF([test "x$with_sdl_mixer" != "xno"], [
        AC_CHECK_HEADERS(SDL2/SDL_mixer.h, [], [AS_IF([test "x$with_sdl_mixer" = "xyes"], [AC_MSG_ERROR([Could not find SDL2_mixer header.])], [NO_MIXER=1])])
        AC_SEARCH_LIBS(Mix_Init, SDL2_mixer, [], [AS_IF([test "x$with_sdl_mixer" = "xyes"], [AC_MSG_ERROR([Could not find SDL2_mixer library.])], [NO_MIXER=1])])
        AC_SEARCH_LIBS(mpg123_init, mpg123)
        AC_SEARCH_LIBS(FLAC__stream_decoder_new, flac)
        AC_SEARCH_LIBS(new_fluid_synth, fluidsynth, [], [AS_IF([test "x$with_sdl_mixer" = "xyes"], [AC_MSG_WARN([Could not find FluidSynth library. Soundfont support will be limited or non-existent.])], [])])
    ], [NO_MIXER=1])

    AS_IF([test "x$PRINT_TYPE" = "x" || test "x$PRINT_TYPE" = "x0"], [
        AC_CHECK_HEADERS(hpdf.h, [], [AS_IF([test "x$PRINT_TYPE" = "x0"], [AC_MSG_ERROR([Could not find hpdf header.])], [PRINT_TYPE=1])])
        AC_SEARCH_LIBS(HPDF_New, hpdf, [], [AS_IF([test "x$PRINT_TYPE" = "x0"], [AC_MSG_ERROR([Could not find hpdf library.])], [PRINT_TYPE=1])])
    ], [])

    AC_SEARCH_LIBS(backtrace, execinfo, [], []) # for musl libc on Linux

    if test "x$NO_PNG" = "x1"; then CPPFLAGS="$CPPFLAGS -DNO_PNG"; fi
    if test "x$NO_WEBP" = "x1"; then CPPFLAGS="$CPPFLAGS -DNO_WEBP"; fi
    if test "x$NO_CLI" = "x1"; then CPPFLAGS="$CPPFLAGS -DNO_CLI"; fi
    if test "x$NO_MIXER" = "x1"; then CPPFLAGS="$CPPFLAGS -DNO_MIXER"; fi
    if test "x$PRINT_TYPE" != "x"; then CPPFLAGS="$CPPFLAGS -DPRINT_TYPE=$PRINT_TYPE"; fi

fi

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
