# CraftOS-PC 2
A rewrite of [CraftOS-PC (Classic)](https://github.com/MCJack123/craftos) using a mixture of C/C++ and the original Lua API, as well as SDL for drawing.

## Tasklist
- [x] Port ComputerCraft API (from [craftos-native](https://github.com/MCJack123/craftos-native))
- [x] Fix paths
- [x] Add HTTP API
- [x] Add configuration
- [x] Add Windows support
- [x] Add peripheral emulation
- [x] Ensure full compliance with CraftOS
- [x] Clean up repository, finish readme
- [x] Fix graphics mode
- [x] Add screenshot support
- [x] Add mount support
- [x] Add HTTP server
- [ ] Add GIF recording support
- [ ] Implement CCEmuX and CC: Tweaked functionality 
  - [ ] Multiple computers
  - [ ] Modem & Computer peripherals
  - [ ] WebSockets
- [ ] Read CC:T changelog & implement applicable features

## Building
### Requirements
* Compiler
  * Linux: GCC, G++, make
  * Mac: Xcode CLI tools
  * Windows: Visual Studio 2019
* liblua 5.1
* SDL 2.0+
* libcurl with SSL
* jsoncpp
* png++ 0.2.7+ (+libpng)
  * Can be disabled with NO_PNG=1, will save as BMP instead
  * Is disabled by default on Windows (since all of the NuGet pkgs suck on VS2019)
* [libharu/libhpdf](https://github.com/libharu/libharu)
  * This library is optional if built with PRINT_TYPE=1 (html) or PRINT_TYPE=2 (txt)
* Windows: dirent.h
* Ubuntu: `sudo apt install build-essential liblua5.1-0-dev libsdl2-dev libcurl4-openssl-dev libjsoncpp-dev libhpdf-dev libpng++-dev`
* The VS solution includes all packages required except libcurl (build yourself)

### Instructions
#### Windows
1. Download the ComputerCraft ROM directory from the [official repo](https://github.com/dan200/ComputerCraft/tree/master/src/main/resources/assets/computercraft/lua/rom)
2. [Build libcurl from source](https://medium.com/@chuy.max/compile-libcurl-on-windows-with-visual-studio-2017-x64-and-ssl-winssl-cff41ac7971d).
3. Download [Visual Studio 2019](https://visualstudio.microsoft.com/) if not already installed.
4. Open `CraftOS-PC 2.sln` with VS.
5. Ensure all NuGet packages are installed.
6. Right click on CraftOS-PC 2.vcxproj -> CraftOS-PC 2 Properties... -> Linker -> General -> Additional Library Search Paths -> Add the path to the libcurl/lib directory
7. Ensure the project is set to the Debug configuration.
8. Build (Ctrl-Shift-B)
9. Open a new Explorer window in your home directory
10. Go to AppData/Local
11. Create a directory named `craftos`
12. Copy bios.lua and craftos.bmp from this repo into `craftos`
13. Copy the ComputerCraft `rom` into `craftos`
14. Build & Run

#### Mac
1. Download the ComputerCraft ROM directory from the [official repo](https://github.com/dan200/ComputerCraft/tree/master/src/main/resources/assets/computercraft/lua/rom)
2. Open a new Terminal window
3. `cd` to the cloned repository
4. `make macapp`
5. Open the repository in a new Finder window
6. Right click on CraftOS-PC.app -> Show Package Contents
7. Open Contents -> Resources
8. Copy the `rom` directory inside
9. Run CraftOS-PC.app

#### Linux
1. Download the ComputerCraft ROM directory from the [official repo](https://github.com/dan200/ComputerCraft/tree/master/src/main/resources/assets/computercraft/lua/rom)
2. Open a new terminal
3. `cd` to the cloned repository
4. `make`
5. `sudo mkdir /usr/share/craftos`
6. `sudo cp bios.lua craftos.bmp /usr/share/craftos/`
7. Copy the ComputerCraft `rom` into `/usr/share/craftos/`

## FAQ
### Why is the ComputerCraft ROM/BIOS not included with the source?
ComputerCraft and its assets are licensed under a copyleft license that requires anything using its code to be under the same license. Since I want CraftOS-PC 2 to remain under only the MIT license, I will not be distributing any original ComputerCraft files with the CraftOS-PC 2 source.

### Why did you choose C/C++?
Since the original ComputerCraft code is written in Java, it may seem like a better idea to create an emulator based on the original mod code. But I found that using native C and C++ lets the emulator run much better.

**1. It runs much faster**
One of the biggest issues I had with CraftOS-PC Classic was that it *ran too slow*. The Java VM adds much overhead to the program which, frankly, is unnecessary. As a native program, CraftOS-PC 2 runs (%d)% faster than CraftOS-Classic. The barebones nature of native code allows this speed boost to exist.

**2. It uses less memory**
Another problem with CraftOS-PC Classic was that it used much more memory than necessary. At startup, CraftOS-PC Classic used well over 150 MB of memory, which could grow to nearly a gigabyte with extensive use. CraftOS-PC 2 only uses 40 MB at startup, and under my testing has never gone over 100 MB. This is due to C's manual memory management and the absence of the entire JVM.

**3. It's the language Lua's written in**
Using the same language that Lua uses guarantees compatibility with the base API. LuaJ has many known issues that can hinder development and cause much confusion while writing programs. Writing CraftOS-PC 2 using liblua guarantees that Lua will behave as it should.

**4. It doesn't rely on any single platform**
I wanted to keep CraftOS-PC Classic's wide compatibility in CraftOS-PC 2. Using other languages such as C# or Swift are platform-dependent and are not guaranteed to work on any platform. C is a basic language that's always present and maintains a portable library that works on all platforms. I've moved all platform-specific code into the platform_*.cpp files so the rest of the code can remain as independent as possible.
