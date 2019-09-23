# CraftOS-PC 2 [![Build Status](https://travis-ci.com/MCJack123/craftos2.svg?branch=master)](https://travis-ci.com/MCJack123/craftos2) [![Actions Status](https://github.com/MCJack123/craftos2/workflows/CI/badge.svg)](https://github.com/MCJack123/craftos2/actions)
A rewrite of [CraftOS-PC (Classic)](https://github.com/MCJack123/craftos) using C++ and the original Lua API, as well as SDL for drawing.

![Screenfetch](image1.png)

## Requirements for released builds
* 64-bit operating system
  * Windows 7+ (might work on Vista)
  * macOS 10.9+
  * Ubuntu 18.04, 19.04 (i386 is supported)
* Administrator privileges
* 7 MB free space

## Installing
### Windows
1. Download CraftOS-PC-Setup.exe from the latest release
2. Follow the instructions in the setup program
3. Open CraftOS-PC from the Start Menu

### Mac
#### Homebrew Cask
```bash
$ brew tap MCJack123/CraftOSPC
$ brew install craftos-pc
$ open /Applications/CraftOS-PC.app
```
#### Manual
1. Download CraftOS-PC.dmg from the latest release
2. Drag and drop into Applications (or not)
3. Double-click CraftOS-PC(.app)

### Ubuntu (18.04 & 19.04)
```bash
$ sudo add-apt-repository ppa:jackmacwindows/ppa
$ sudo apt update
$ sudo apt install craftos-pc
$ craftos
```

## Building
### Requirements
* [CraftOS ROM package](https://github.com/MCJack123/craftos2-rom)
* Compiler supporting C++11
  * Linux: G++ 4.9+, make
  * Mac: Xcode CLI tools (xcode-select --install)
  * Windows: Visual Studio 2019
* Lua 5.1
* SDL 2.0+
* SDL_mixer 2.0+
* OpenSSL 1.0.x
* Windows: dirent.h
* POCO NetSSL + JSON libraries + dependencies
  * Foundation
  * Util
  * Crypto
  * XML
  * JSON
  * Net
  * NetSSL

### Optional
* libpng 1.6 & png++ 0.2.7+
  * Can be disabled with `--without-png`, will save as BMP instead
* [libharu/libhpdf](https://github.com/libharu/libharu)
  * Can be disabled with `--without-hpdf`, `--with-html` or `--with-txt`
* ncurses
  * Can be disabled with `--without-ncurses`, will disable CLI support
* The path to the ROM package can be changed with `--prefix=<path>`

You can get all of these dependencies with:
  * Windows: The VS solution includes all packages required except POCO (build yourself)
  * Mac (Homebrew): `brew install lua@5.1 sdl2 sdl2-mixer png++ libharu poco; git clone https://github.com/MCJack123/craftos2-rom`
  * Ubuntu: `sudo apt install git build-essential liblua5.1-0-dev libsdl2-dev libsdl2-mixer-dev libhpdf-dev libpng++-dev libpoco-dev; git clone https://github.com/MCJack123/craftos2-rom`

### Instructions
#### Windows
1. Download [Visual Studio 2019](https://visualstudio.microsoft.com/) if not already installed
2. [Build Poco from source](https://pocoproject.org/download.html#visualstudio)
3. Open a new Explorer window in %ProgramFiles% (Win-R, %ProgramFiles%)
4. Create a directory named `CraftOS-PC`
5. Copy the contents of the CraftOS ROM into the directory
6. Open `CraftOS-PC 2.sln` with VS
7. Ensure all NuGet packages are installed
8. Right click on CraftOS-PC 2.vcxproj -> CraftOS-PC 2 Properties... -> Linker -> General -> Additional Library Search Paths -> Add the path to the poco/lib directory
9. Build & Run

#### Mac
1. Open a new Terminal window
2. `cd` to the cloned repository
3. `./configure`
4. `make macapp`
5. Open the repository in a new Finder window
6. Right click on CraftOS-PC.app => Show Package Contents
7. Open Contents -> Resources
8. Copy the ROM package inside
9. Run CraftOS-PC.app

#### Linux
1. Open a new terminal
2. `cd` to the cloned repository
3. `./configure`
4. `make`
5. `sudo mkdir /usr/share/craftos`
6. Copy the ComputerCraft ROM into `/usr/share/craftos/`
7. `./craftos`

## FAQ
### Why is the ComputerCraft ROM/BIOS not included with the source?
ComputerCraft and its assets are licensed under a copyleft license that requires anything using its code to be under the same license. Since I want CraftOS-PC 2 to remain under only the MIT license, I will not be distributing any original ComputerCraft files with the CraftOS-PC 2 source. You can still aquire the ROM [separately](https://github.com/MCJack123/craftos2-rom).

### Why did you choose C++?
Since the original ComputerCraft code is written in Java, it may seem like a better idea to create an emulator based on the original mod code. But I found that using native C++ lets the emulator run much better than if it was in Java.

**1. It runs much faster**
One of the biggest issues I had with CraftOS-PC Classic was that it *ran too slow*. The Java VM adds much overhead to the program which, frankly, is unnecessary. As a native program, CraftOS-PC 2 runs 2x faster than CraftOS-Classic. The barebones nature of native code allows this speed boost to exist.

**2. It uses less memory**
Another problem with CraftOS-PC Classic was that it used much more memory than necessary. At startup, CraftOS-PC Classic used well over 150 MB of memory, which could grow to nearly a gigabyte with extensive use. CraftOS-PC 2 only uses 40 MB at startup on Mac (10 on Windows!), and under my testing has never gone over 100 MB. This is due to C++'s manual memory management and the absence of the entire JVM.

**3. It's the language Lua's written in**
Using the same language that Lua uses guarantees compatibility with the base API. LuaJ has many known issues that can hinder development and cause much confusion while writing programs. Writing CraftOS-PC 2 using liblua guarantees that Lua will behave as it should.

**4. It doesn't rely on any single platform**
I wanted to keep CraftOS-PC Classic's wide compatibility in CraftOS-PC 2. Using other languages such as C# or Swift are platform-dependent and are not guaranteed to work on any platform. C++ is a basic language that's always present and maintains a portable library that works on all platforms. I've moved all platform-specific code into the platform_*.cpp files so the rest of the code can remain as independent as possible.
