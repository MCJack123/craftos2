craftos: obj/fs_handle.o obj/fs.o obj/lib.o obj/main.o obj/os.o obj/term.o
	g++ -o craftos obj/fs_handle.o obj/fs.o obj/lib.o obj/main.o obj/os.o obj/term.o -llua -lm -ldl -lncurses

obj/fs_handle.o: fs_handle.c fs_handle.h
	gcc -o obj/fs_handle.o -c fs_handle.c

obj/fs.o: fs.c fs.h fs_handle.h lib.h
	gcc -o obj/fs.o -c fs.c

obj/lib.o: lib.c lib.h
	gcc -o obj/lib.o -c lib.c

obj/main.o: main.c lib.h fs.h os.h bit.h redstone.h
	gcc -o obj/main.o -c main.c

obj/os.o: os.cpp os.h lib.h
	g++ -o obj/os.o -c os.cpp

obj/term.o: term.c term.h lib.h
	gcc -o obj/term.o -c term.c
