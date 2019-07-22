CC=gcc
CXX=g++
CFLAGS=-c -g
CXXFLAGS= -std=c++11
LIBS=-L/usr/local/include -llua -lm -ldl -lSDL2 -lpthread -lcurl

craftos: obj obj/config.o obj/fs_handle.o obj/fs.o obj/http.o obj/http_handle.o obj/lib.o obj/main.o obj/os.o obj/platform.o obj/term.o obj/TerminalWindow.o
	$(CXX) -o craftos obj/config.o obj/fs_handle.o obj/fs.o obj/http.o obj/http_handle.o obj/lib.o obj/main.o obj/os.o obj/platform.o obj/term.o obj/TerminalWindow.o $(LIBS)

obj:
	mkdir obj

obj/config.o: config.c config.h lib.h
	$(CC) -o obj/config.o $(CFLAGS) config.c

obj/fs_handle.o: fs_handle.c fs_handle.h
	$(CC) -o obj/fs_handle.o $(CFLAGS) fs_handle.c

obj/fs.o: fs.c fs.h fs_handle.h lib.h platform.h
	$(CC) -o obj/fs.o $(CFLAGS) fs.c

obj/http.o: http.c http.h http_handle.h lib.h platform.h term.h
	$(CC) -o obj/http.o $(CFLAGS) http.c

obj/http_handle.o: http_handle.c http_handle.h
	$(CC) -o obj/http_handle.o $(CFLAGS) http_handle.c

obj/lib.o: lib.c lib.h
	$(CC) -o obj/lib.o $(CFLAGS) lib.c

obj/main.o: main.c lib.h fs.h os.h bit.h redstone.h http.h
	$(CC) -o obj/main.o $(CFLAGS) main.c

obj/os.o: os.cpp os.h lib.h
	$(CXX) -o obj/os.o $(CXXFLAGS) $(CFLAGS) os.cpp

obj/platform.o: platform.cpp platform.h platform_linux.cpp platform_darwin.cpp
	$(CXX) -o obj/platform.o $(CXXFLAGS) $(CFLAGS) platform.cpp

obj/term.o: term.cpp term.h TerminalWindow.hpp lib.h
	$(CXX) -o obj/term.o $(CXXFLAGS) $(CFLAGS) term.cpp

obj/TerminalWindow.o: TerminalWindow.cpp TerminalWindow.hpp
	$(CXX) -o obj/TerminalWindow.o $(CXXFLAGS) $(CFLAGS) TerminalWindow.cpp

clean:
	rm obj/*
