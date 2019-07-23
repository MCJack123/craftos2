# CraftOS-PC 2
A rewrite of [CraftOS-PC](https://github.com/MCJack123/craftos) using C and the original Lua API, as well as SDL for drawing.

## Tasklist
- [x] Port ComputerCraft API (from [craftos-native](https://github.com/MCJack123/craftos-native))
- [x] Fix paths
- [x] Add HTTP API
- [x] Add configuration
- [ ] Add peripheral emulation
- [ ] Fix graphics mode
- [ ] Add mount support
- [ ] Add Windows support

## Building
### Requirements
* GCC, G++, make
* liblua 5.1
* SDL 2.0+
* libcurl with SSL
* Currently only supports Mac & Linux
  * Ubuntu: `sudo apt install build-essential liblua5.1-0-dev libsdl2-dev libcurl4-openssl-dev`

### Instructions
```bash
# If on Mac, replace /usr/share/craftos with /usr/local/share/craftos
$ sudo mkdir /usr/share/craftos
$ sudo cp bios.lua /usr/share/craftos/
$ git clone https://github.com/dan200/ComputerCraft
$ sudo cp -R ComputerCraft/src/main/assets/computercraft/lua/rom /usr/share/craftos/
$ make
```