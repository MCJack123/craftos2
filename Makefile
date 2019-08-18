CC=gcc
CXX=g++
PRINT_TYPE?=pdf
CFLAGS:=$(CFLAGS) -g -c -I/usr/include/lua5.1 -I/usr/include/jsoncpp
CXXFLAGS:= $(CXXFLAGS) -std=c++11 -DPRINT_TYPE=$(PRINT_TYPE)
ODIR=obj
SDIR=src
LIBS=-L/usr/local/include -llua5.1 -lm -ldl -lSDL2 -lSDL2main -lpthread -lcurl -ljsoncpp

ifeq ($(PRINT_TYPE), pdf)
LIBS:=$(LIBS) -lhpdf
endif
ifndef NO_PNG
LIBS:=$(LIBS) -lpng
endif
ifdef NO_PNG
CXXFLAGS:=$(CXXFLAGS) -DNO_PNG
endif

_OBJ=Computer.o config.o fs_handle.o fs.o http_handle.o http.o http_server.o lib.o main.o mounter.o os.o periphemu.o peripheral.o term.o TerminalWindow.o peripheral_monitor.o peripheral_printer.o peripheral_computer.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

craftos: $(OBJ) $(ODIR)/platform.o
	$(CXX) -o $@ $^ $(LIBS)

macapp: $(OBJ) $(ODIR)/platform_macapp.o
	mkdir -p CraftOS-PC.app/Contents/MacOS
	mkdir -p CraftOS-PC.app/Contents/Resources
	clang++ -o CraftOS-PC.app/Contents/MacOS/craftos $^ $(LIBS) -framework Foundation
	cp Info.plist CraftOS-PC.app/Contents/
	cp craftos.bmp CraftOS-PC.app/Contents/Resources/

$(ODIR):
	mkdir obj

$(ODIR)/main.o: $(SDIR)/main.cpp $(SDIR)/Computer.hpp
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/platform_macapp.o: $(SDIR)/platform_macapp.mm $(SDIR)/platform.hpp
	clang++ -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/platform.o: $(SDIR)/platform.cpp $(SDIR)/platform.hpp $(SDIR)/platform_linux.cpp $(SDIR)/platform_darwin.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/peripheral.o: $(SDIR)/peripheral/peripheral.cpp $(SDIR)/peripheral/peripheral.hpp $(SDIR)/lib.hpp
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/http_server.o: $(SDIR)/http_server.cpp
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/%.o: $(SDIR)/%.c $(SDIR)/%.h
	$(CC) -o $@ $(CFLAGS) $<

$(ODIR)/%.o: $(SDIR)/%.cpp $(SDIR)/%.hpp $(SDIR)/lib.hpp
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

$(ODIR)/peripheral_%.o: $(SDIR)/peripheral/%.cpp $(SDIR)/peripheral/%.hpp $(SDIR)/peripheral/peripheral.hpp
	$(CXX) -o $@ $(CXXFLAGS) $(CFLAGS) $<

clean:
	rm obj/*

rebuild: clean craftos
