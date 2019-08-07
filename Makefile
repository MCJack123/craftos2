CC=gcc
CXX=g++
CFLAGS=-c -g -I/usr/include/lua5.1 -I/usr/include/jsoncpp
CXXFLAGS= -std=c++11
ODIR=obj
SDIR=src
LIBS=-L/usr/local/include -llua5.1 -lm -ldl -lSDL2 -lSDL2main -lpthread -lcurl -lhpdf -ljsoncpp

_OBJ=config.o fs_handle.o fs.o http_handle.o http.o lib.o main.o os.o periphemu.o peripheral.o term.o TerminalWindow.o peripheral_monitor.o peripheral_printer.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

craftos: $(OBJ) $(ODIR)/platform.o
	$(CXX) -o $@ $^ $(LIBS)

macapp: $(OBJ) $(ODIR)/platform_macapp.o
	mkdir -p CraftOS-PC.app/Contents/MacOS
	mkdir -p CraftOS-PC.app/Contents/Resources
	clang++ -o CraftOS-PC.app/Contents/MacOS/craftos $^ $(LIBS) -framework Foundation
	cp Info.plist CraftOS-PC.app/Contents/
	cp craftos.bmp bios.lua CraftOS-PC.app/Contents/Resources/

$(ODIR):
	mkdir obj

$(ODIR)/main.o: $(SDIR)/main.c $(SDIR)/bit.h $(SDIR)/config.h $(SDIR)/fs.h $(SDIR)/http.h $(SDIR)/os.h $(SDIR)/term.h $(SDIR)/redstone.h $(SDIR)/peripheral/peripheral.h $(SDIR)/periphemu.h $(SDIR)/platform.h
	$(CC) -o $@ $(CFLAGS) $<

$(ODIR)/platform_macapp.o: $(SDIR)/platform_macapp.mm $(SDIR)/platform.h
	clang++ -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/platform.o: $(SDIR)/platform.cpp $(SDIR)/platform.h $(SDIR)/platform_linux.cpp $(SDIR)/platform_darwin.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/TerminalWindow.o: $(SDIR)/TerminalWindow.cpp $(SDIR)/TerminalWindow.hpp
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/peripheral.o: $(SDIR)/peripheral/peripheral.cpp $(SDIR)/peripheral/peripheral.h $(SDIR)/lib.h
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/%.o: $(SDIR)/%.c $(SDIR)/%.h $(SDIR)/lib.h
	$(CC) -o $@ $(CFLAGS) $<

$(ODIR)/%.o: $(SDIR)/%.cpp $(SDIR)/%.h $(SDIR)/lib.h
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/peripheral_%.o: $(SDIR)/peripheral/%.cpp $(SDIR)/peripheral/%.hpp $(SDIR)/peripheral/peripheral.h
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

clean:
	rm obj/*
