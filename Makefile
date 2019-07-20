CC=gcc
CXX=g++
CFLAGS=-c -g
CXXFLAGS= -std=c++11
LIBS=-L/usr/local/include -llua -lm -ldl -lSDL2 -lpthread

craftos: obj obj/fs_handle.o obj/fs.o obj/lib.o obj/main.o obj/os.o obj/platform.o obj/term.o obj/TerminalWindow.o
	$(CXX) -o craftos obj/fs_handle.o obj/fs.o obj/lib.o obj/main.o obj/os.o obj/platform.o obj/term.o obj/TerminalWindow.o $(LIBS)

obj:
	mkdir obj

obj/fs_handle.o: fs_handle.c fs_handle.h
	$(CC) -o obj/fs_handle.o $(CFLAGS) fs_handle.c

obj/fs.o: fs.c fs.h fs_handle.h lib.h platform.h
	$(CC) -o obj/fs.o $(CFLAGS) fs.c

obj/lib.o: lib.c lib.h
	$(CC) -o obj/lib.o $(CFLAGS) lib.c

obj/main.o: main.c lib.h fs.h os.h bit.h redstone.h
	$(CC) -o obj/main.o $(CFLAGS) main.c

obj/os.o: os.cpp os.h lib.h
	$(CXX) -o obj/os.o $(CXXFLAGS) $(CFLAGS) os.cpp

obj/platform.o: platform.cpp platform.h platform_linux.cpp platform_darwin.cpp
	$(CXX) -o obj/platform.o $(CXXFLAGS) $(CFLAGS) platform.cpp

obj/term.o: term.cpp term.h TerminalWindow.hpp lib.h
	$(CXX) -o obj/term.o $(CXXFLAGS) $(CFLAGS) term.cpp

obj/TerminalWindow.o: TerminalWindow.cpp TerminalWindow.hpp
	$(CXX) -o obj/TerminalWindow.o $(CXXFLAGS) $(CFLAGS) TerminalWindow.cpp
