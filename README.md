# CraftOS-PC 2 [![Actions Status](https://github.com/MCJack123/craftos2/workflows/CI/badge.svg)](https://github.com/MCJack123/craftos2/actions)
A rewrite of [CraftOS-PC (Classic)](https://github.com/MCJack123/craftos) using C++ and a modified version of PUC Lua, as well as SDL for drawing.

Visit the website at https://www.craftos-pc.cc/ for more information, including documentation.

![Screenfetch](resources/image1.png)

## Requirements for released builds
* Supported operating systems:
  * Windows Vista x64 or later
  * macOS 10.9.5+
  * Ubuntu 18.04, 20.04, 21.04
  * Arch Linux with AUR helper
  * iOS 11.0+
  * Android 7.0+
* Administrator privileges
* 20-50 MB free space

## Installing
### Windows
1. Download CraftOS-PC-Setup.exe from the latest release
2. Follow the instructions in the setup program
3. Open CraftOS-PC from the Start Menu

### Mac
#### Homebrew Cask
```bash
$ brew tap MCJack123/CraftOSPC
$ brew install --cask craftos-pc
$ open /Applications/CraftOS-PC.app
```
#### Manual
1. Download CraftOS-PC.dmg from the latest release
2. Drag and drop into Applications (or not)
3. Double-click CraftOS-PC(.app)

### Linux
#### Ubuntu (PPA)
```bash
$ sudo add-apt-repository ppa:jackmacwindows/ppa
$ sudo apt update
$ sudo apt install craftos-pc
$ craftos
```

#### Fedora (COPR)
```sh
sudo dnf copr enable lemoonstar/CraftOS-PC
sudo dnf install craftos-pc
```

Fedora support is maintained by [LeMoonStar](https://github.com/LeMoonStar). For any issues with the Fedora package itself, please contact them [on their GitHub repo](https://github.com/LeMoonStar/craftos2-rpm).

#### Arch Linux
Install the `craftos-pc` package using your chosen AUR helper (e.g. `yay -S craftos-pc`).

### iOS
[Visit the App Store](https://apps.apple.com/us/app/craftos-pc/id1565893014) to download CraftOS-PC. Or you can [join the TestFlight beta](https://testflight.apple.com/join/SiuXlijR) to get access to the latest versions before they're released.

### Android
1. Download CraftOS-PC.apk from the latest release
2. Open the file and tap "Install"
3. Open CraftOS-PC from the app drawer or home screen

### v2.2: Where are my files?
CraftOS-PC v2.2 moves the save directory to be more appropriate for each platform. Your files are not gone; they're automatically moved over before launching if the old folder is still present. You can find the computer data files at these locations:
* Windows: `%appdata%\CraftOS-PC` (`C:\Users\<user>\AppData\Roaming\CraftOS-PC`)
* Mac: `~/Library/Application Support/CraftOS-PC`
* Linux: `$XDG_DATA_HOME/craftos-pc` or `~/.local/share/craftos-pc`

## Building
### Requirements
* [CraftOS ROM package](https://github.com/MCJack123/craftos2-rom)
* Compiler supporting C++14
  * Linux: G++ 4.9+, make
  * Mac: Xcode CLI tools (xcode-select --install)
  * Windows: Visual Studio 2019
* SDL 2.0.8+ (may work on older versions on non-Linux)
* OpenSSL 1.1.1 (for POCO)
* POCO 1.5.0+: NetSSL & JSON libraries + dependencies
  * Foundation
  * Util
  * Crypto
  * XML
  * JSON
  * Net
  * NetSSL
    * On Windows, you'll need to modify the `poco` port to use OpenSSL. Simply open `vcpkg\ports\poco\portfile.cmake`, find `ENABLE_NETSSL_WIN`, and replace it with `FORCE_OPENSSL`. Then install as normal.
* Windows: dirent.h (install with NuGet OR vcpkg)
* Windows: [vcpkg](https://github.com/microsoft/vcpkg)

#### Optional
* libpng 1.6 & png++ 0.2.7+
  * Can be disabled with `--without-png`, will save as BMP instead
* libwebp
  * Can be disabled with `--without-webp`, will disable WebP support (`useWebP` option will always be off)
* [libharu/libhpdf](https://github.com/libharu/libharu)
  * Can be disabled with `--without-hpdf`, `--with-html` or `--with-txt`
* ncurses or PDCurses
  * Can be disabled with `--without-ncurses`, will disable CLI support
* SDL_mixer 2.0+
  * Can be disabled with `--without-sdl_mixer`, will disable audio disc and speaker support
  * For MP3 support, libmpg123 is required
  * For FLAC support, libFLAC is required
  * For SF2 support, SDL_mixer must be built manually with fluidsynth support (or with the `fluidsynth` feature in vcpkg since July 9, 2021)
* The path to the ROM package can be changed with `--prefix=<path>`, which will store the ROM at `<path>/share/craftos`
* Standalone builds can be enabled with `--with-standalone-rom=<fs_standalone.cpp>`, with `<fs_standalone.cpp>` referring to the path to the packed standalone ROM file.
  * The latest packed ROM can be downloaded as an artifact from the latest CI build, found by following the top link [here](https://github.com/MCJack123/craftos2-rom/actions).

You can get all of these dependencies with:
  * Windows: `vcpkg --feature-flags=manifests install --triplet x64-windows` inside the repository directory
    * Visual Studio will do this for you automatically (as long as vcpkg integration is installed)
  * Windows (manual): `vcpkg install sdl2:x64-windows sdl2-mixer[dynamic-load,libflac,mpg123,libmodplug,libvorbis,opusfile,fluidsynth]:x64-windows pngpp:x64-windows libwebp:x64-windows libharu:x64-windows poco[netssl]:x64-windows dirent:x64-windows pdcurses:x64-windows`
  * Mac (Homebrew): `brew install sdl2 sdl2_mixer png++ webp libharu poco ncurses; git clone https://github.com/MCJack123/craftos2-rom`
  * Ubuntu: `sudo apt install git build-essential libsdl2-dev libsdl2-mixer-dev libhpdf-dev libpng++-dev libwebp-dev libpoco-dev libncurses5-dev; git clone https://github.com/MCJack123/craftos2-rom`
  * Arch Linux: `sudo pacman -S sdl2 sdl2_mixer png++ libwebp libharu poco ncurses`

### Windows artifact builds
Builds of each commit are automatically uploaded for Windows in the Actions tab. These builds are provided to allow Windows users to test new features without having to build the entire solution and dependencies. Note that these files are just the raw executable. You must drop the file into a pre-existing CraftOS-PC install directory for it to work properly. Depending on changes made in the latest version, you may also have to download the latest [ROM](https://github.com/MCJack123/craftos2-rom). You can download the latest file directly [here](https://nightly.link/MCJack123/craftos2/workflows/main/master/CraftOS-PC-Artifact.zip).

Old nightly builds, as well as Android betas, are available [on the website](https://www.craftos-pc.cc/nightly/).

### Instructions
#### Windows
1. Download [Visual Studio 2019](https://visualstudio.microsoft.com/) if not already installed
2. `git submodule update --init --recursive`
3. Open `CraftOS-PC 2.sln` with VS
4. Build solution
5. Copy all files from the ROM into the same directory as the new executable (ex. `craftos2\x64\Release`)
6. Run solution

The solution has a few different build configurations:
* Debug: for debugging, no optimization
* Release: standard Windows application build with optimization (same as installed `CraftOS-PC.exe`)
* ReleaseC: same as Release but with console support (same as installed `CraftOS-PC_console.exe`)
* ReleaseStandalone: same as Release but builds a standalone build; requires `fs_standalone.cpp` to be present in `src`

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

#### Linux (or Mac as non-app binary)
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
