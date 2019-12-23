# New feature documentation
This file provides documentation for the new APIs in CraftOS-PC.

## Creating new computers
Computers can be created with `periphemu.create("computer_<id>", "computer")`, where `<id>` is the ID of the new computer. If a computer with the ID is already open, it will not create a new one. The `computer` peripheral for the new computer will also be attached.  
If the peripheral is detached with `periphemu.remove`, the peripheral will be detached but the computer window will stay open. To close the computer from Lua, call `peripheral.call("computer_<id>", "shutdown")`, and the peripheral will automatically be detached.  
Computers can also be attached and detached from the shell when using the CraftOS ROM with `attach computer_<id> computer` and `detach computer_<id>`, respectively.

## Command line interface
On supported systems, CraftOS-PC can be run from the terminal with the `--cli` flag. You can use Shift+Arrow Keys to change the currently open window. In this mode, some features are not supported, including:
* Holding keys: CLI mode cannot detect key releases, and thus sends both a `key` and `key_up` event at the same time. Because of this, it cannot detect if you are holding any keys down.
* Using modifier keys: CLI mode cannot detect pressing modifier keys, so CraftOS-PC works around that by using the Home key as Control and the End key as Alt. To send Home/End to CraftOS, hold down Shift while pressing the key.

## Using custom fonts
The font used for CraftOS-PC can be changed in `<save dir>/config/global.json`, with the `customFontPath` option. To set the font, set `customFontPath` to the absolute path to a BMP file containing the font glyphs. Each glyph must be exactly 6*s* x 9*s* px with 2*s* pixels between each glyph, where *s* is a number representing the scale of the font. `customFontScale` must also be set to a number representing the size of the font (1 = HD font (12x18), 2 = normal font (6x9), 3 = 1/2 size font (4x6)).

For example, if the CCEmuX `hdfont.bmp` is placed at `/usr/share/craftos/hdfont.bmp`, we can add this to `global.json`:
```json
"customFontPath": "/usr/share/craftos/hdfont.bmp",
"customFontScale": 1
```

## `periphemu`
Creates and removes peripherals from the registry.
### Functions
* *boolean* create(*string* side, *string* type\[, *string* path\]): Creates a new peripheral.
  * side: The side of the new peripheral
  * type: One of `computer`, `drive`, `modem`, `monitor`, `printer`
  * path: If creating a printer, the local path to the output file
  * Returns: `true` on success, `false` on failure (already exists)
* *boolean* remove(*string* side): Removes a peripheral.
  * side: The side to remove
  * Returns: `true` on success, `false` on failure (already removed)
* *table* names(): Returns a list of available peripherals.

## `drive` peripheral
Floppy drive emulator that supports loading mounts (see mounter API), floppy disks (by ID), and audio files.
See [disk API](computercraft.info/wiki/Disk_(API)) for other functions.
### Methods
* *nil* insertDisk(*string/number* path): Replaces the loaded disk with the specified resource.
  * path: Either a disk ID or path to load
	* If number: Mounts the floppy disk (`<save dir>/computer/disk/<id>`) to /disk[n]
	* If path to directory: Mounts the real path specified to /disk[n]
	* If path to file: Loads the file as an audio disc (use `disk.playAudio` or the "dj" command)
  
## `config`
Changes ComputerCraft configuration variables in ComputerCraft.cfg.
### Functions
* *any* get(*string* name): Returns the value of a configuration variable.
  * name: The name of the variable
  * Returns: The value of the variable
* *void* set(*string* name, *any* value): Sets the value of a configuration variable.
  * name: The name of the variable
  * value: The new value of the variable
* *table* list(): Returns a list of all configuration variable names.
* *number* getType(*string* name): Returns the type of a variable.
  * name: The name of the variable
  * Returns: 0 for boolean, 1 for string, 2 for number, 3 for table

## `mounter`
Mounts and unmounts real directories.
### Functions
* *nil* mount(*string* name, *string* path\[, *boolean* readOnly\]): Mounts a real directory to a ComputerCraft directory.
  * name: The local directory to mount to
  * path: The absolute directory to mount from
* *nil* unmount(*string* name): Unmounts a previously mounted directory.
  * name: The local directory to unmount
* *table* list(): Returns a key-value table of all current mounts on the system.

## `term`
Graphics mode extension in the `term` API.
### Functions
* *nil* setGraphicsMode(*boolean* mode): Sets whether the terminal is in pixel-graphics mode
  * mode: `2` for 256-color graphics mode, `true` or `1` for 16-color graphics mode, `false` or `0` for text mode
* *boolean* getGraphicsMode(): Returns the current graphics mode setting (false for text mode, number for graphics mode).
* *nil* setPixel(*number* x, *number* y, *color* color): Sets a pixel at a location.
  * x: The X coordinate of the pixel
  * y: The Y coordinate of the pixel
  * color: The color of the pixel
    * In mode 1, this should be a color in `colors`
    * In mode 2, this should be an index from 0-255
* *color* getPixel(*number* x, *number* y): Returns the color of a pixel at a location.
  * x: The X coordinate of the pixel
  * y: The Y coordinate of the pixel
  * Returns: The color of the pixel
* *nil* setPaletteColor(*number* color, *number* r[, *number* g, *number* b]): Sets the RGB values for a color. (Override)
  * color: The color to change
    * In mode 1, this should be a color in `colors`
    * In mode 2, this should be an index from 0-255
  * r: Either the red value from 0.0 to 1.0, or an RGB hex value
  * g: The green value from 0.0 to 1.0
  * b: The blue value from 0.0 to 1.0
* *number*, *number*, *number* getPaletteColor(*number* color): Returns the RGB values for a color. (Override)
  * color: The color to change
    * In mode 1, this should be a color in `colors`
    * In mode 2, this should be an index from 0-255
  * Returns: The RGB values for the color, each from 0.0 to 1.0
* *nil* screenshot([*string* path]): Takes a screenshot.
  * path: The real path to save to (defaults to `<save dir>/screenshots/<date>_<time>.<bmp|png>`)

## `http`
HTTP server extension in the `http` API.
### Functions
* *nil* addListener(*number* port): Adds a listener on a port.
  * port: The port to listen on
* *nil* removeListener(*number* port): Frees a port to be listened on again later.
  * port: The port to stop listening on
* *nil* listen(*number* port, *function* callback): Starts a server on a port and calls a function when a request is made.
  * port: The port to listen on
  * callback(*table* req, *table* res): The callback to call
    * req: A read file handle to the request data, with the following extra functions:
      * getURL(): Returns the URI endpoint of the request
      * getMethod(): Returns the HTTP method of the request
      * getRequestHeaders(): Returns a table of headers sent by the client
    * res: A write file handle to the response data, with the following extra functions:
      * setStatusCode(*number* code): Sets the HTTP response code to send
      * setResponseHeader(*string* key, *string* value): Sets a header value to send
    * **ALWAYS** call `res.close()` before returning from the callback 
## Events
* http_request: Sent when an HTTP request is made.
  * *number*: The port the request was made on
  * *table*: The request table
  * *table*: The response table
* server_stop: Send this inside an `http.listen()` callback to stop the server

## `os`
* *string* os.about(): Returns a string with the version as well as a license notice.
* *nil* os.setHaltOnLongRunMode(*boolean* mode) **CraftOS-PC Accelerated only**: Sets whether the computer should throw an error if it runs for too long without yielding. This is necessary on CraftOS-PC Accelerated since the method used to do this can drastically slow the computer down on LuaJIT.
  * mode: `true` to enable error, `false` to disable error

## Plugin API
CraftOS-PC 2 features a new plugin API that allows easy addition of new C APIs into the environment. 
A plugin consists of a shared library (`.dll`, `.dylib`, `.so`) that contains a function named `luaopen_<name>`, where `<name>` is the filename of the plugin.
Upon loading a new computer, the `plugins` folder inside the install directory is scanned for plugins to load.
For each plugin found, CraftOS-PC loads and calls the plugin's `luaopen` function. This function should have the same signature as a `lua_CFunction`:
```
int luaopen_plugin(lua_State *L);
```
The function will recieve the ROM path and the base path, and should return one value: the API that will be exported to the global table.  
Plugins must also have an exported function named `version`, which tells CraftOS-PC what version of the API it's using.
As of v2.2, the version is currently `2`. You can use the template `version` function in the example below.
If this version number changes, make sure to update the version in your plugin and recompile to ensure full compatibility.

If your plugin needs access to the CraftOS-PC `Computer` object, copy the `get_comp(L)` function in `lib.cpp`.
This function takes in a `lua_State` and returns a pointer to the `Computer` object associated with it.
You can then access the properties of the computer.
The `Computer` object also has a userdata property that allows temporary per-computer storage if necessary.  

When compiling a plugin, make sure to link in the custom `craftos2-lua` library instead of the original Lua library.
  
Here's an example of a C plugin (from https://github.com/mostvotedplaya/Lua-C-Module):
```c
#include <lua.h>
#include <lauxlib.h>
#define PLUGIN_VERSION 2

int addition(lua_State *L) {
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "Expecting first parameter to be typeof: number");
    }

    if (!lua_isnumber(L, 2)) {
        return luaL_error(L, "Expecting second parameter to be typeof: number");
    }

    int addition = lua_tonumber(L, 1) + lua_tonumber(L, 2);
    lua_pushnumber(L, addition);
    return 1;
}

int multiply(lua_State *L) {
    if (!lua_isnumber(L, 1)) {
        return luaL_error(L, "Expecting first parameter to be typeof: number");
    }

    if (!lua_isnumber(L, 2)) {
        return luaL_error(L, "Expecting second parameter to be typeof: number");
    }

    int multiply = lua_tonumber(L, 1) * lua_tonumber(L, 2);
    lua_pushnumber(L, multiply);
    return 1;
}

#ifdef _WIN32
_declspec(dllexport) 
#endif
int luaopen_example(lua_State *L) {
    struct luaL_reg M[] =
    {
      {"addition", addition},
      {"multiply", multiply},
      {NULL,NULL}
    };

    luaL_register(L, "example", M);
    return 1;
}

#ifdef _WIN32
_declspec(dllexport) 
#endif
int version(lua_State *L) {lua_pushinteger(L, PLUGIN_VERSION); return 1;}
```
Compile as a shared library and copy to:
* Windows: `C:\Program Files\CraftOS-PC\plugins\example.dll`
* Mac: `CraftOS-PC.app/Contents/Resources/plugins/example.dylib`
* Linux: `/usr/share/craftos/plugins/example.so`

When booting a new computer, the `example` API will be available in the global table.  
I've also added a binding for the CCEmuX API in `examples/ccemux.cpp`.  