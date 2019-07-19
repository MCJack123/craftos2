craftos: obj/fs_handle.o obj/fs.o obj/keys.o obj/lib.o obj/main.o obj/os.o obj/term.o obj/TerminalWindow.o
	g++ -o craftos obj/fs_handle.o obj/fs.o obj/keys.o obj/lib.o obj/main.o obj/os.o obj/term.o obj/TerminalWindow.o -llua -lm -ldl -lSDL2 -lpthread

obj/fs_handle.o: fs_handle.c fs_handle.h
	gcc -o obj/fs_handle.o -c -g fs_handle.c

obj/fs.o: fs.c fs.h fs_handle.h lib.h
	gcc -o obj/fs.o -c -g fs.c

obj/keys.o: keys.c keys.h
	gcc -o obj/keys.o -c -g keys.c

obj/lib.o: lib.c lib.h
	gcc -o obj/lib.o -c -g lib.c

obj/main.o: main.c lib.h fs.h os.h bit.h redstone.h
	gcc -o obj/main.o -c -g main.c

obj/os.o: os.cpp os.h lib.h
	g++ -o obj/os.o -c -g os.cpp

obj/term.o: term.cpp term.h TerminalWindow.hpp lib.h
	g++ -o obj/term.o -c -g term.cpp

obj/TerminalWindow.o: TerminalWindow.cpp TerminalWindow.hpp
	g++ -o obj/TerminalWindow.o -c -g TerminalWindow.cpp
