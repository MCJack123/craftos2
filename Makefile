CC=gcc
CXX=g++
CFLAGS=-c -g
CXXFLAGS= -std=c++11
ODIR=obj
LIBS=-L/usr/local/include -llua -lm -ldl -lSDL2 -lpthread -lcurl

_OBJ=config.o fs_handle.o fs.o http_handle.o http.o lib.o main.o os.o periphemu.o platform.o peripheral.o term.o TerminalWindow.o peripheral_monitor.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

craftos: $(OBJ)
	$(CXX) -o $@ $^ $(LIBS)

$(ODIR):
	mkdir obj

$(ODIR)/main.o: main.c bit.h config.h fs.h http.h os.h term.h redstone.h peripheral/peripheral.h periphemu.h platform.h
	$(CC) -o $@ $(CFLAGS) $<

$(ODIR)/platform.o: platform.cpp platform.h platform_linux.cpp platform_darwin.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/TerminalWindow.o: TerminalWindow.cpp TerminalWindow.hpp
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/peripheral.o: peripheral/peripheral.cpp peripheral/peripheral.h lib.h
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/%.o: %.c %.h lib.h
	$(CC) -o $@ $(CFLAGS) $<

$(ODIR)/%.o: %.cpp %.h lib.h
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/peripheral_%.o: peripheral/%.cpp peripheral/%.hpp peripheral/peripheral.h
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

clean:
	rm obj/*
