# CraftOS-PC 2 [![Build Status](https://travis-ci.com/MCJack123/craftos2.svg?branch=v2.3)](https://travis-ci.com/MCJack123/craftos2) [![Actions Status](https://github.com/MCJack123/craftos2/workflows/CI/badge.svg?branch=v2.3)](https://github.com/MCJack123/craftos2/actions)
A rewrite of [CraftOS-PC (Classic)](https://github.com/MCJack123/craftos) using C++ and the original Lua API, as well as SDL for drawing.

![Screenfetch](image1.png)

## Requirements for released builds
* Supported operating systems:
  * Windows Vista x64 or later
  * macOS 10.9+
  * Ubuntu 18.04, 19.04, 19.10
  * Arch Linux with AUR helper
* Administrator privileges
* 20 MB free space

## Installing
### Windows
1. Download CraftOS-PC-Setup.exe from the latest release
2. Follow the instructions in the setup program
3. Open CraftOS-PC from the Start Menu

### Mac
#### __*Catalina Users: READ THIS*__
macOS Catalina adds a new policy requiring all apps to be notarized with a Developer ID. Because I don't have a paid dev account, CraftOS-PC cannot be notarized, meaning Catalina users can't just double click on the app at first launch. **When opening CraftOS-PC for the first time, make sure to right-click on the app and click Open, instead of double-clicking the app as usual.**
#### Homebrew Cask
```bash
$ brew tap MCJack123/CraftOSPC
$ brew cask install craftos-pc
$ open /Applications/CraftOS-PC.app
```
#### Manual
1. Download CraftOS-PC.dmg from the latest release
2. Drag and drop into Applications (or not)
3. Double-click CraftOS-PC(.app)

### Ubuntu (PPA)
```bash
$ sudo add-apt-repository ppa:jackmacwindows/ppa
$ sudo apt update
$ sudo apt install craftos-pc
$ craftos
```

### Arch Linux
Install the `craftos-pc` package using your chosen AUR helper (e.g. `yay -S craftos-pc`).

## v2.2: Where are my files?
CraftOS-PC v2.2 moves the save directory to be more appropriate for each platform. Your files are not gone; they're automatically moved over before launching if the old folder is still present. You can find the computer data files at these locations:
* Windows: `%appdata%\CraftOS-PC` (`C:\Users\<user>\AppData\Roaming\CraftOS-PC`)
* Mac: `~/Library/Application Support/CraftOS-PC`
* Linux: `$XDG_DATA_HOME/craftos-pc` or `~/.local/share/craftos-pc`

## Building
### Requirements
* [CraftOS ROM package](https://github.com/MCJack123/craftos2-rom)
* Compiler supporting C++11
  * Linux: G++ 4.9+, make
  * Mac: Xcode CLI tools (xcode-select --install)
  * Windows: Visual Studio 2019
* SDL 2.0.8+ (may work on older versions on non-Linux)
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

#### Optional
* libpng 1.6 & png++ 0.2.7+
  * Can be disabled with `--without-png`, will save as BMP instead
* [libharu/libhpdf](https://github.com/libharu/libharu)
  * Can be disabled with `--without-hpdf`, `--with-html` or `--with-txt`
* ncurses
  * Can be disabled with `--without-ncurses`, will disable CLI support
* SDL_mixer 2.0+
  * Can be disabled with `--without-sdl_mixer`, will disable audio disc support
  * For MP3 support, libmpg123 is required
  * For FLAC support, libFLAC is required
  * For SF2 support, SDL_mixer must be built manually with fluidsynth support
* The path to the ROM package can be changed with `--prefix=<path>`, which will store the ROM at `<path>/share/craftos`
* Standalone builds can be enabled with `--with-standalone-rom=<fs_standalone.cpp>`, with `<fs_standalone.cpp>` referring to the path to the packed standalone ROM file.
  * The latest packed ROM can be downloaded as an artifact from the latest CI build, found by following the top link [here](https://github.com/MCJack123/craftos2-rom/actions).

You can get all of these dependencies with:
  * Windows: The VS solution includes all packages required except POCO and png (build yourself)
  * Mac (Homebrew): `brew install sdl2 sdl2_mixer png++ libharu poco ncurses; git clone https://github.com/MCJack123/craftos2-rom`
  * Ubuntu: `sudo apt install git build-essential libsdl2-dev libsdl2-mixer-dev libhpdf-dev libpng++-dev libpoco-dev libncurses5-dev; git clone https://github.com/MCJack123/craftos2-rom`
  * Arch Linux: `sudo pacman -S sdl2 sdl2_mixer openssl-1.0 png++ libharu poco ncurses`

### Windows Nightly Builds
Nightly builds of CraftOS-PC are available [on the website](https://www.craftos-pc.cc/nightly/). These builds are provided to allow Windows users to test new features without having to build the entire solution and dependencies. New builds are posted at midnight EST, unless there were no changes since the last build. The download page lists the three latest builds, but older builds are available by direct link. Note that these files are just the raw executable; if there were changes to the ROM you must pull them in manually.

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
3. `git submodule update --init --recursive`
4. `make -C craftos2-lua macosx`
5. `./configure`
6. `make macapp`
7. Open the repository in a new Finder window
8. Right click on CraftOS-PC.app => Show Package Contents
9. Open Contents -> Resources
10. Copy the ROM package inside
11. Run CraftOS-PC.app

#### Linux
1. Open a new terminal
2. `cd` to the cloned repository
3. `git submodule update --init --recursive`
4. `make -C craftos2-lua linux`
5. `./configure`
6. `make`
7. `sudo mkdir /usr/local/share/craftos`
8. Copy the ComputerCraft ROM into `/usr/local/share/craftos/`
9. `./craftos`

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
