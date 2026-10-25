## v2.8.3 - August 16, 2024
* Updated CC:T version to 1.112.0
  * Add `r+`/`w+` support to the `io` library.
  * Report a custom error when using `!` instead of `not`.
  * Add `cc.strings.split` function.
  * Add missing bounds check to `cc.strings.wrap` (Lupus950).
* Security: Fixed a vulnerability allowing filesystem sandbox escape (GHSA-hr3w-wc83-6923)
* Security: Fixed potential data leakage by enhancing type checks (CVE-2024-39840)
* Fixed error when `[[` appears in a `[[` long string
* Fixed `setfenv` not returning the function passed to it
* Fixed regression causing debugger to not set hook on all coroutines in stack
* Fixed exception in `fs.list` when non-ASCII names are present
* Added status parameter to HTTP server `res.setStatusCode`
* Fixed HTTP server request handles converting line endings
* `file.readAll` now correctly returns `nil` on a second call
* Fixed an exception when sending an empty WebSocket message (#365)

## v2.8.2 - April 22, 2024
* Updated CC:T version to 1.110.2
  * Discard characters being typed into the editor when closing `edit`'s `Run` screen.
  * Improve several Lua parser error messages.
  * `colour.toBlit` correctly errors on out-of-bounds values.
  * Round non-standard colours in `window`, like `term.native()` does.
  * Fix `speaker` program not resolving files relative to the current directory.
  * Add `speaker sound` command (fatboychummy).
  * Improve error when calling `speaker play` with no path (fatboychummy).
  * Prevent playing music discs with `speaker.playSound`.
  * Various documentation fixes (cyberbit).
* Fix compilation on Linux with MUSL libc (Ocawesome101, #341)
* Fixed Homebrew Cask release action
* Added log message for queue overflows
* Fixed some argument parsing issues in `http.request`
* Fixed some issues with timers
* Fix string comparison errors (migeyel, MCJack123/craftos2-lua#6)
* Fixed an inconsistency with Cobalt with `table.remove`
* lua_Unsigned uses 64-bit ints again, fixing many bugs (MCJack123/craftos2-lua#7)
* Fixed crash when trying to halt computer
* Fixed race condition in debug adapter
* Fixed some missing `os.date` specifiers on Windows
* Fix for "Port already in use" error when trying to reopen WebSocket server on the same port. (simadude, #343)
* Fixed seeking not working on HTTP handles in standards mode only
* Chest Inventory peripheral list returns entries for empty slots (NuclearMonk, #340)
* Fixed drives not emitting `disk` events
* Fixed issue in inventories when `toSlot` isn't provided
* Added warning message when the configuration file can't be saved
* Fixed `speaker_audio_empty` not firing on `speaker.stop`
* Fixed bounds checking on computer IDs (#350)
* Fixed issues when providing concatenated strings to some functions

## v2.8.1 - January 21, 2024
* Updated CC:T version to 1.109.3
  * Fix trailing-comma on method calls (e.g. `x:f(a, )`) not using our custom error message.
* Fixed high memory usage caused by ropes & substrings
* Fixed crash when closing WebSocket server
* Fixed CCEmuX plugin failing to load

## v2.8 - January 1, 2024
* Update CC:T version to 1.109.2
  * Update to Lua 5.2
    * `getfenv`/`setfenv` now only work on Lua functions.
    * Add support for `goto`.
    * Remove support for dumping and loading binary chunks.
      * Only disabled in standards mode
  * File handles, HTTP requests and websocket messages now use raw bytes rather than converting to UTF-8.
  * `fs.open` now supports `r+`/`w+` modes.
  * Add `allow_repetitions` option to `textutils.serialiseJSON`.
  * `math.random` now uses Lua 5.4's random number generator.
  * `tostring` now correctly obeys `__name`.
* Rewrote WebSocket server API (#337)
  * Use `server = http.websocketServer(port)` to create a server handle
  * `server.listen()` waits for a new connection, and returns a new WebSocket handle
    * Handles have an additional `clientID` field for identifying the client connection
  * `server.close()` closes the server
  * Events are now under the `websocket_server_` domain
    * `websocket_server_connect <port> <handle>`: Sent when a client connects to the server
    * `websocket_server_message <clientID> <message> <binary>`: Sent when a client sends a message
    * `websocket_server_closed <clientID> [closeCode] [closeMessage]`: Sent when a client disconnects from the server
* Bumped plugin *major* version to 12
  * Lua 5.2 breaks old plugins - please reinstall/rebuild any plugins (ccemux is updated)
* WebSocket close events now send the close code if available
* Fixed WebSocket ping messages causing the socket to close
* Fixed many memory corruption issues around ropes
* Fixed some issues with debug hooks and yielding
* Fixed crash when erroring from a debug hook (#326)
* Debuggers now inherit the mount list from the original computer (#327)
* Fixed memory reporting when using `string.rep` (#328)
* Fixed `fs.getFreeSpace` not checking parent directories if the path doesn't exist (#330)
* Fixed crash when using HTTP in the VS Code extension (#332)
* Fixed repeated `websocket.close` calls causing a crash (#336)
* Mobile: Added onboarding screen for navigation bar instructions

## v2.7.5 - August 2, 2023
* Updated CC:T version to 1.106.1
  * Optimise JSON string parsing.
  * Add `colors.fromBlit` (Erb3).
  * Add custom timeout support to the HTTP API.
  * The speaker program now errors when playing HTML files.
  * `edit` now shows an error message when editing read-only files.
  * Port `fs.find` to Lua. This also allows using `?` as a wildcard.
  * Add option to serialize Unicode strings to JSON (MCJack123).
  * Small optimisations to the `window` API.
  * Lua REPL no longer accepts `)(` as a valid expression.
  * Fix several inconsistencies with `require`/`package.path` in the Lua REPL (Wojbie).
* Added `term.relativeMouse` function, which converts `mouse_move` events into `mouse_move_relative` events with relative velocities
* Fixed `modem.getNameLocal` not existing
* Fixed abort timeouts firing after the computer goes into sleep mode
* Fixed stack corruption when using `string.format("%q")`
* Fixed a niche case crash when the computer turns off while prompting for abort timeout
* Fixed SSL errors in AppImage builds

## v2.7.4 - May 12, 2023
* **Fixed critical crashing issue on Windows systems relating to ropes**
* Updated CC:T version to 1.104.0
  * The shell now supports hashbangs (`#!`) (emmachase).
  * Error messages in edit are now displayed in red on advanced computers.
  * Improvements to the display of errors in the shell and REPL.
  * Fix `rednet` queueing the wrong message when sending a message to the current computer.
  * Fix the Lua VM crashing when a `__len` metamethod yields.
  * `table` methods and `ipairs` now use metamethods.
  * Argument errors now follow the standard "X expected, got Y" format.
  * Add `coroutine.isyieldable`.
  * Type errors now use the `__name` metatag.
  * `xpcall` now accepts arguments after the error function.
  * `speaker` program now reports an error on common unsupported audio formats.
  * multishell now hides the implementation details of its terminal redirect from programs.
  * `settings.load` now ignores malformed values created by editing the .settings file by hand.
  * Ignore metatables in `textutils.serialize`.
  * Fix `gps.locate` returning `nan` when receiving a duplicate location (Wojbie).
* Added plugin support on iOS through in-app purchases
  * Use the `plugins` program to buy new plugin packs
  * Only one is available at the moment, featuring `ccemux`, `joystick`, and `sound`
* Native `load` now uses Lua 5.2 syntax, matching Cobalt's behavior
* Fixed crashing when calling `monitor.blit` when the cursor is off-screen
* Fixed compilation error on newer Linux systems

## v2.7.3 - January 26, 2023
* Updated CC:T version to 1.102.2
  * Moved Lua portions of `fs` and `http` outside the BIOS
  * Trim spaces from filesystem paths.
  * Fix `import.lua` failing to upload a file.
  * Reduce inconsistency with the table length operator in some cases
* Linux builds are now officially published as AppImages
  * These will be provided in conjunction with normal packages for Ubuntu/Arch/Raspbian
  * Arch users will be able to use `craftos-pc-bin` package to install the AppImage version, avoiding slow local compilation
* Replaced rope and substring allocation with clusters
  * This should hopefully fix performance and memory allocation issues with string concatenation
* Added new multi-touch events on mobile platforms
  * `_CCPC_finger_touch`, `_CCPC_finger_up`, `_CCPC_finger_drag`
  * All events get finger ID, X, and Y as parameters
  * These events co-exist with single-touch mouse events
* iOS now has Page Up/Down keys on the arrow toolbar for quicker navigation
* Added function authentication for C functions to mitigate bytecode vulnerabilities
* Improved performance of binary `file.readAll`
* `unpack` no longer uses a table's `n` field
* Fixed error when setting palette colors on windows in 256-color graphics mode
* Fixed more crashing when filesystem functions fail (#280)
* Fixed crashes due to stack overflows
* Fixed WebSocket pings sending a close response (#214)
* Fixed crashes in `websocket.receive()` when receiving a message that's too large (#297, #214)
* Fixed vague error message responses when an HTTP request fails (#292)
* Fixed DAP crash from setting a breakpoint in an unknown file
* Fixed breakpoints activating on the wrong line
* Fixed first line of a file not being able to trigger a breakpoint
* Fixed debugger errors when invalid arguments are used on the ccdb command line (#294)
* Fixed precision issues with `os.epoch("nano")` (#299)
* Fixed `rawlen` and `__ipairs` being missing from the global table
* Fixed Windows crash reporter being disabled
* Windows builds will no longer attempt to prompt for crash reporting when run in console-based modes
* Fixed some inconsistencies with seeking beyond a file's limits (#310)

## v2.7.2 - November 7, 2022
* Updated CC:T version to 1.101.1
  * File drag-and-drop now queues a file_transfer event on the computer. The built-in shell or the import program must now be running to upload files.
  * The peripheral now searches for remote peripherals using any peripheral with the peripheral_hub type, not just wired modems.
  * Add include_hidden option to fs.complete, which can be used to prevent hidden files showing up in autocomplete results. (IvoLeal72)
  * Add shell.autocomplete_hidden setting. (IvoLeal72)
  * Prevent edit's "Run" command scrolling the terminal output on smaller screens.
  * Mention WAV support in speaker help (MCJack123).
  * Add http programs to the path, even when http is not enabled.
  * Fix example in textutils.pagedTabulate docs (IvoLeal72).
  * Fix help program treating the terminal one line longer than it was.
* Fixed a syntax error in the CCEmuX plugin's `emu` program (#271)
* Fixed a bug causing copies out of virtual mounts to fail (#272)
* Adjusted some behavior of ropes to hopefully make them faster & use less memory
  * Further fixes are in progress to fully optimize memory usage
* Fixed issues when reading from a bad file handle
* Fixed crashing when some filesystem functions fail
  * This was causing autocompletion on `config` to fail on Windows (#273), as well as crashing when using `fs.getSize` on a folder (#281)
* Fixed failure to launch on Apple Silicon Macs due to incorrectly named libraries (#287)
* Fixed missing `chest.getItemLimit` method (#291)
* Adjusted some logic in WebSocket handles to fix some potential issues
* Fixed window palettes being broken in graphics mode

## v2.7.1 - September 1, 2022
* Fixed an issue causing `fs.getFreeSpace` and `fs.getCapacity` to return invalid values on Windows
* Fixed an issue causing `fs.attributes` to report read-only status incorrectly on Windows
* (Re-)Fixed a bug causing `\27` characters in files to mark end-of-files on Windows (#226)
* Fixed `fs.getName` returning the wrong result in some cases
* Fixed a crash when HTTP `handle.readLine()` returns an empty string (#270)
* Fixed an issue causing the debug adapter to fail to work over stdio, causing `launch` requests to fail
* Keystrokes now automatically reset modifier keys on mobile platforms

## v2.7 - August 25, 2022
* Bumped CC:T version to 1.100.9
  * Added basic WAV support to speaker.lua
* Added debug adapter for Visual Studio Code
  * This makes it possible to use VS Code's debugger interface with CraftOS-PC
  * Install the [CraftOS-PC extension](https://marketplace.visualstudio.com/items?itemName=jackmacwindows.craftos-pc) to use the debugger
* Added basic generic peripheral emulation
  * New peripheral types: `chest`/`minecraft:chest` (`inventory`), `tank` (`fluid_storage`), `energy` (`energy_storage`)
  * `tank` and `energy` may also have custom types set on them (for emulating compatible blocks from other mods)
  * See the full documentation for more info
* Rewrote filesystem code to use C++17 filesystem library
* Added ability to resize monitors programmatically, including by block size (#261)
* Added mounter sandboxing/path restriction (#104)
  * Use the `mounter_whitelist` and `mounter_blacklist` config options to set paths that are allowed and blocked
  * The deepest match of both lists takes priority
  * `mounter_no_ask` can be used to disable mount prompts for specific paths
* Added `istailcall` field to `debug.getinfo`
* CLI mode now uses the Symbols for Legacy Computing block for bitmap characters (requires a font supporting Unicode 13)
* Adjusted configuration loading code to avoid crashes from invalid files
* Upgraded Windows project to Visual Studio 2022
* Fixed an issue on macOS causing HTTPS connections to fail
* Fixed constant crashes when launching the Android app
* Fixed "not enough memory" error when calling `string.rep` with a negative length
* Fixed require not working in --script/--exec
* Fixed a crash from invalid WebSocket data
* Fixed an HTTP issue causing the `Host` header to be set incorrectly when connecting to `localhost`
* Fixed a crash if a computer is closed before an HTTP request finishes
* Fixed a rendering issue when using a custom font with the hardware renderer
* Fixed `string.format("%q")` not accepting non-string arguments (#251)
* Fixed an issue causing raw mode to hang and use 100% CPU while exiting
* Fixed a typo in the `setGraphicsMode` argument checking code
* Fixed an issue causing the debugger to error after a reboot
* Fixed a crash when using `debug.getinfo(f, ">")`
* Fixed an overflow in `os.epoch` when using a 32-bit architecture
* Linux: Fixed occasional crashes from X11 while resizing, moving, etc.
* CCEmuX plugin: Improved error message when `emu open` fails

## v2.6.6 - May 5, 2022
* Bumped CC:T version to 1.100.5
* Enabled proper HTTPS certificate verification
* Added support for URLs to `cc.http.gist`/`gist` program
* Added `seek` method to binary HTTP handles
* Added strip option to `string.dump` from Lua 5.3
* Fixed `paintutils.drawBox` not working properly in graphics mode
* Improved DFPWM audio quality
* Small compatibility fixes to drives
* Fixed type of `http_max_websocket_message` config setting
* Fixed crash when passing negative values to `monitor.scroll` (#244)
* Fixed crash in `string.rep` when allocation fails (crash report)
* Fixed possible race condition in WebSockets (crash report)
* Fixed a crash when `http_response_handle.write` throws an error (crash report)
* Fixed a possible crash when a rogue event is sent (crash report)
* Fixed a crash when an event is sent to a computer that was just destroyed (crash report)
* Fixed a possible crash when the abort timer triggers right as the computer shuts down (crash report)

## v2.6.5.1 - February 22, 2022
* Bumped CC:T version to 1.100.2 (v2.6.5 should have had this)
* Added `nano` clock to `os.epoch` (@SkyTheCodeMaster)
* Added Fedora repo courtesy of @LeMoonStar
* **Fixed crashes, memory corruption caused by rope strings being freed**
* Fixed fragments being included in HTTP requests (#239)
* Fixed speaker code not working properly by adding a queue limit

## v2.6.5 - February 5, 2022
* Bumped CC:T version to 1.100.1
* Bumped structure version to 8
  * Functions are now pre-declared to ensure type compatibility
  * New macro `DLLEXPORT`
  * New class `TerminalFactory`
  * New fields in `configuration`:
    * `useDFPWM`
  * New fields in `PluginFunctions`:
    * `registerTerminalFactory`
    * `commandLineArgs`
    * `setListenerMode`
    * `pumpTaskQueue`
  * New fields in `Terminal`:
    * `factory`
* Fixed an issue causing an invalid string to be returned as the parameter for `speaker_audio_empty`
* `speaker.playAudio` now always returns `false` to tell programs to wait for `speaker_audio_empty` as expected
* Added caching to rope resolution to avoid concatenating the same rope multiple times
* Modified `string.rep` to work more like Cobalt
* Fixed hang when using CLI mode
* Fixed possible race condition in `mouse_move`
* Fixed an issue preventing the debugger console from scrolling
* Added an indicator showing whether the debugger console is no longer auto-scrolling
* Disabled loading of original `package` and `io` libraries to reduce possible vulnerabilities

## v2.6.4 - December 25, 2021
* Updated CC:T version to 1.100.0
  * Speakers can now play arbitrary PCM audio.
  * Add support for encoding and decoding DFPWM streams, with the cc.audio.dfpwm module.
  * Fix the "repeat" program not repeating broadcast rednet messages.
* Added `useDFPWM` config setting to toggle use of DFPWM playback emulation
* Added `speaker.setPosition(x, y, z)` to emulate positioning of speaker audio
* Standards mode now controls the behavior of the new `speaker.playAudio`
  * When disabled, all audio is added to a queue with no latency, and `speaker.playAudio` never fails
  * When enabled:
    * `speaker_audio_empty` is queued when the audio is 0.5 seconds before it's expected to finish, emulating latency
    * `speaker.playAudio` returns `false` if there is more than 0.5 seconds of audio in the buffer
    * `useDFPWM` is forced to `true`
* Renamed `speaker.stopSounds` to `speaker.stop`
  * `stopSounds` still exists for backwards compatibility, but is deprecated
* `speaker.playLocalMusic` is now deprecated in favor of `speaker.playAudio`
  * It is recommended you load the audio files yourself instead of relying on the system to decode it
* CraftOS-PC Online is now working better (#222)
  * CraftOS-PC Online now supports Safari on iOS 15.2+/macOS 12.2+
  * There is currently a huge memory leak bug in some browsers that can cause the page to crash on low-memory systems
  * Hopefully CraftOS-PC Online will be fully working in the near future
* Fixed some string comparisons not working as expected
* Fixed incorrect documentation on mobile gestures (#230)
* Fixed keyboard on iOS being dismissed when closing the app (#231)
* Fixed screen glitches when opening a new terminal after changing `useHDFont` (SkyTheCodeMaster)
* Fixed some issues with textutils.serializeJSON

## v2.6.3 - December 11, 2021
* Updated CC:T version to 1.99.1
  * Peripherals can now have multiple types. `peripheral.getType` now returns multiple values, and `peripheral.hasType` checks if a peripheral has a specific type.
  * Add feature introduction/changed version information to the documentation. (MCJack123)
  * Rednet can now handle computer IDs larger than 65535. (Ale32bit)
  * Optimise peripheral calls in `rednet.run`. (xAnavrins)
  * Add `cc.pretty.pretty_print` helper function (Lupus590).
  * Fix `textutils.serialize` not serialising infinity and nan values. (Wojbie)
  * Add `package.searchpath` to the `cc.require` API. (MCJack123)
* Bumped structure version to 7
  * New fields in `Computer`:
    * `shouldDeleteDebugger`
  * New fields in `peripheral`:
    * `getTypes`
* Added ability to set printer ink color
* Added support for hexadecimal floating-point numbers
* Unwritable data directories now throw an error
* Default peripherals now throw an error when calling a method that doesn't exist
* Terminal frozen status is now reset after rebooting
* X11 and Wayland libraries are no longer required when building for Linux
* `\0` characters in a string are now treated as a space by `load`
* Fixed websocket.receive not functioning properly
* Fixed `fs.copy` stopping at EOF bytes on Windows (#226)
* Fixed hardware renderer not showing anything on screen (#227)
* Fixed crash when using `detach` on a debugger
* Fixed crash when an error occurs in `drive.insertDisk` on Windows
* Fixed coroutine metatable getting overwritten by coroutine.create
* Fixed `keepOpenOnShutdown` using 100% CPU
* Fixed Ctrl+R not working with `keepOpenOnShutdown` after two successive reboots
* Fixed `getCursorBlink` being missing from monitors
* Fixed monitor events not working properly in raw mode
* Fixed a possible race condition when shutting down on exit
* Fixed race condition in timer erasure
* Fixed various other small race conditions

## v2.6.2 - October 31, 2021
* Bumped structure version to 6
  * New fields in `Computer`:
    * `openWebsocketServers`
  * New fields in `configuration`:
    * `dropFilePath`
  * New fields in `PluginFunctions`:
    * `registerPeripheralFn`
* Added ability to drop files to paste their paths instead of copying the file
  * Enabled using `dropFilePath` config setting
* Added single-window mode
  * Displays all computers on the same window
  * Activated with `--single` flag on CLI
  * Ctrl+Alt+Left/Right (Cmd+Option+L/R on Mac) to switch windows in GUI
* Added ability to copy screenshots on Linux (X11, Wayland)
* Added `registerPeripheralFn` capability to allow passing a function object
  * `registerPeripheral` is now deprecated
* Added brand-new UI on mobile devices (beta only)
  * Redesigned UI with navigation bar & hotkey toolbar above keyboard
  * Added support for computer, debugger, and monitor peripherals
  * Change windows using the arrows in the navigation bar
  * Automatic shell resizing makes sure you can see what you type
  * Arrow key gestures now work on Android as well as iOS
  * Added new launch screen (iOS)
  * The new UI is only available in beta builds at the moment due to bugginess
    * iOS: https://testflight.apple.com/join/SiuXlijR
    * Android: https://www.craftos-pc.cc/nightly/
* Improved WebSocket server functionality
  * Servers can now be properly opened by calling `http.websocket` with a port argument
  * Multiple clients to the same server now get unique identifiers as userdata values
  * Added `websocket_server_closed` event to notify when an opened WebSocket server is no longer accepting connections
* Replaced Windows nightly builds with CI build artifacts
  * Windows builds will be available within minutes of the latest commit being pushed
  * Nightly builds for Windows will no longer be posted on the website
  * Go to https://github.com/MCJack123/craftos2/actions for the latest build
* CLI mode now uses Unicode characters for non-ASCII characters
  * Drawing characters are approximated using Braille characters; this may not show up well on a real terminal
* Improved quality of CCEmuX plugin
* WebSocket text messages are now sent in UTF-8
* Improved Rednet deduplication efficiency (part of CC:T 1.99.0)
* cash no longer saves duplicate history entries
* Deprecated `peripheral::update` as it was never used
* Fixed crashing whenever opening a debugger (from crash reports)
* Fixed a crash when comparing two identical substrings (#218)
* Fixed a crash caused by force-closing a computer after it's already gone (from crash reports)
* Fixed a crash in `term.drawPixels` when passing a negative value (#224)
* Fixed occasional crashes from `get_comp` cache duplication (from crash reports)
* Fixed crashes and incorrect behavior when using `string.format` with substrings (from crash reports: @Creepi)
* Fixed strings in modem messages not being sent properly on Linux (@BytecodeEli)
* Fixed binary support in WebSocket messages
* Fixed an issue causing input to stop working in raw mode on Linux
* Fixed stack overflow when an error handler attempts to yield (from crash reports)
* Fixed a bug causing old `os` functions to be exposed (!)
* Fixed an incorrect error message when concatenating a concatenated string or substring with a value of an invalid type (@9551)
* Fixed an issue with completion for boolean config settings

## v2.6.1 - August 31, 2021
* Bumped CC:T version to 1.98.2
  * Add motd for file uploading.
  * Fix `settings.define` not accepting a nil second argument (SkyTheCodeMaster).
  * Add a missing type check for `http.checkURL`.
  * Prevent `parallel.*` from hanging when no arguments are given.
  * Prevent issue in rednet when the message ID is NaN.
  * Fix `help` program crashing when terminal changes width.
  * Prevent `wget` crashing when given an invalid URL and no filename.
  * Correctly wrap string within `textutils.slowWrite`.
* Bumped structure version to 5
  * New fields in `configuration`:
    * `useWebP`
  * New fields in `Computer`:
    * `httpRequestQueue`
    * `httpRequestQueueMutex`
* Added support for screenshots and recordings in WebP format
  * WebP is an image format that is much smaller than PNG/GIF and supports animation
  * All modern web browsers and OSes support WebP images
    * Unfortunately, Discord does not support WebP recordings at the moment.
  * Recordings can be up to 20x smaller than their GIF counterparts
  * Disabled by default; enable the `useWebP` config option to use WebP instead of PNG/GIF
* Added support for delta installers on Windows
  * These are stripped-down versions of the installer that only contain core CraftOS-PC files
  * This reduces the size of data to download when updating
  * Some versions may not have delta installers if libraries need to be updated
* Added update download progress window on Windows; made progress bar determinate on macOS
* Implemented limits for HTTP options that were present but unfunctional
* Parameters to events are now copied when standards mode is enabled
* Opening a file when the maximum file count is reached now creates the file as expected
* Rewrote `websocket.receive` function in C to perform better
* Fixed WebSockets not sending PONG packets, causing sockets to randomly close after a while
* Fixed crashes when trying to use a WebSocket handle after closing it
* Fixed a race condition in HTTP requests that caused a crash
* Fixed a race condition causing crashes when running a task on the main thread
* Fixed a race condition causing functions (like `term.write`) to be run on the wrong computer
* Fixed an issue causing the first frame of GIFs to be darker than the rest of the recording
* Fixed the close button not working in some cases
* Fixed `fs` API allowing illegal characters on Windows
* Fixed modems not checking if channel numbers are in range
* Fixed `utf8.charpattern` not existing
* Fixed crash error message not appearing in standards mode
* Fixed `os.clock()` not resetting on reboot (#215)
* Fixed crash reporter failing to upload files + some websites failing to connect
  * TLS 1.3 was disabled (pocoproject/poco#3395), so HTTPS connections may be slightly less secure
* Fixed duplication of default black/whitelist entries
* Fixed `pairs` returning incorrect values after a call hook

## v2.6 - July 11, 2021
* CraftOS-PC is now available on Android and iOS
  * Apps are available on the iOS App Store (Play Store in the future; please download the APK manually)
  * Supports same feature set as desktop CraftOS-PC
    * Monitors and multi-computer support is missing, but will be added in a future version
  * Pinch in to open keyboard, out to close keyboard
  * Extra mobile-centric features:
    * `mobile` API with `openKeyboard(open: boolean)` and `isKeyboardOpen()` functions
    * `_CCPC_mobile_keyboard_open <height>` event when keyboard is opened, with an argument for visible height
    * `_CCPC_mobile_keyboard_close` event when keyboard is closed
* Bumped CC:T version to 1.97.0
  * 1.96.0:
    * Use lightGrey for folders within the "list" program.
    * Add cc.expect.range (Lupus590).
    * Allow calling cc.expect directly (MCJack123).
    * Fix paintutils.drawLine incorrectly sorting coordinates (lilyzeiset).
    * Correctly handle sparse arrays in cc.pretty.
  * 1.97.0:
    * Add scale subcommand to `monitor` program (MCJack123).
      * This is a modification of the already-existing `resolution` subcommand.
    * Add option to make `textutils.serialize` not write an indent (magiczocker10).
    * Allow comparing vectors using `==` (fatboychummy).
    * Allow `craft` program to craft unlimited items (fatboychummy).
    * Add program subcompletion to several programs (Wojbie).
    * Update the `help` program to accept and (partially) highlight markdown files.
    * Remove config option for the debug API.
      * It still exists internally, but is always set to `true`.
    * Allow uploading files by dropping them onto a computer.
    * Update the `wget` to be more resiliant in the face of user-errors.
    * Fix `exiting` paint typing "e" in the shell.
* Bumped structure version to 4
  * New fields in `PluginFunctions`:
    * `addEventHook`
    * `setDistanceProvider`
  * New fields in `Computer`:
    * `eventHooks`
    * Deprecated fields:
      * `nextMouseMove`
      * `lastMouse`
      * `mouseMoveDebounceTimer`
  * New fields in `Terminal`:
    * `nextMouseMove`
    * `lastMouse`
    * `mouseMoveDebounceTimer`
  * New types:
    * `event_hook`
* Upgraded raw mode protocol to version 1.1
  * New filesystem access ability
  * Computer windows now send the ID of the computer
  * Changed meaning of raw cursor blink field to indicate blinking, not showing
  * Small improvements to the protocol
  * Official protocol specification at https://www.craftos-pc.cc/docs/rawmode
* Improved performance of string concatenation by using ropes
  * Final concatenation of strings is not completed until the string's value needs to be read
  * This was implemented in CC:T 1.91.0
  * Expect repeated concatenation operations to be around 100x faster
* Improved performance of `string.sub` by using efficient substring views
  * Getting a substring no longer has to reallocate the string
  * Instead, it reuses the original string with the offset and length required
* Added HTTP whitelist & blacklist
  * Emulates configuration of CC:T up until 1.87.0 (before rule-based system)
* Added command-line option to connect to a remote WebSocket server in raw mode
* Changed cursor blink speed to 0.4s to match CC's behavior
* Rewrote main thread task queuer to be more efficient
* Setting `abortTimeout` to 0 now disables abort timeouts
* The close button no longer needs to be clicked twice to exit when `keepOpenOnShutdown` is enabled
* Fixed "400 Bad Request" error on HTTP requests when the path is empty
* Fixed crash when a bad URL is passed to HTTP functions
* Fixed an issue causing encoded slashes in URLs being decoded prematurely (#199)
* Fixed some memory leaks in HTTP handles
* Fixed HTTP not working properly in CraftOS-PC Online
* Fixed random crashes while sending messages over a modem (#205)
* Fixed sending recursive tables over modems
* Fixed old abort timer firing after reboot, causing spurious "Too long without yielding" errors
* Fixed crash when canceling a timer that doesn't exist
* Fixed `os.epoch "local"` not accounting for Daylight Savings Time
* Fixed files being truncated in text mode on Windows when a `\x0A` character is found (#204)
* Software rendering now reuses the same surface to reduce memory pressure
* Fixed an issue causing inconsistent speeds when recording to GIF
* Fixed blit only allowing gray colors on grayscale terminals
* Fixed monitors in raw mode sending close events to the wrong window ID
* Fixed some issues with setting monitor scale
* Fixed mouse event debouncing on monitors
* Fixed monitors not reporting a second `monitor_touch` event when clicked twice at the same point
* Fixed `mouse_move` leave event on monitors not sending the side
* Fixed a bug causing `mouse_move` events to stop being sent after a while
* Fixed behavior of `term.blit` when passing an invalid character to color strings
* Fixed raw client mouse events not being sent properly
* Fixed an issue causing crashes when creating certain peripherals
* Fixed a possible crash when the BIOS cannot be found (#208)
* `__lt` metamethods can now yield from inside `table.sort`
* Fixed a possible memory leak in `table.sort`
* Fixed an issue causing `__lt` metamethods that yield to return the wrong result from `<=`
* Fixed various errors in yielding from debug hooks
* Fixed stack not being resized when > 0x08000000 entries are required

## v2.5.5 - April 17, 2021
* Disabled locks when modems aren't attached
  * This can improve speeds by up to 50%
  * If any modem is attached, speeds will drop back to pre-v2.5.5 levels
* Rewrote HTTP handle read functions to improve reliability
* Removed Origin header from WebSocket requests
* Fixed behavior of table length to work more like CC:T
* Fixed paste contents not being cut at the first newline
* Fixed memory leak in file.readAll in binary mode
* Fixed incorrect modulo result when {(a < 0 | b < 0) & |a| % |b| = 0}
* Fixed hard crash on startup when a custom font file doesn't exist
* Fixed crash when passing non-string in header table
* Fixed crash when halting computer after it already closed
* Fixed crash when an exception occurs while closing WebSocket in the middle of catching another exception
* Possibly fixed a crash happening when connecting to a WebSocket
* Readded Lua features that were advertised in v2.5.4 but not actually present on Windows
  * Added `debug.upvalue{id,join}` from Lua 5.3
  * Fixed a race conditions with modems causing a crash
  * Fixed some random crashes on an odd memory error
  * Fixed crash when passing bad argument #1 to `table.foreach`

## v2.5.4 - March 27, 2021
* Bumped CC:T version to 1.95.3
  * Correctly serialise sparse arrays into JSON (livegamer999)
  * Programs run via edit are now a little better behaved (Wojbie)
  * Add User-Agent [and Accept-Charset] to a websocket's headers.
* Bumped structure version to 3
  * New fields in `configuration`:
    * `keepOpenOnShutdown`
  * New fields in `computer_configuration`:
    * `computerWidth`
    * `computerHeight`
  * New fields in `PluginFunctions`:
    * `attachPeripheral`
    * `detachPeripheral`
* Added `keepOpenOnShutdown` setting to keep computers open after shutdown
  * Use the X button to close the window (or your OS's respective key combo)
  * Use Ctrl+R to start the computer again
* Added per-computer default size options
* Added two arguments when attaching monitors to specify their size
* ~~Added `debug.upvalue{id,join}` from Lua 5.3~~
* Added the ability to set host ports for WebSocket servers
* Added support for KMSDRM backends on Linux
* Disabled WebSocket servers in vanilla mode
* Adding duplicate virtual mounts now returns `false` without adding it again
* Moved `romReadOnly` to hidden options
* Fixed macOS installation instructions for Homebrew
* ~~Fixed a race conditions with modems causing a crash~~
* Fixed abort timeout occasionally firing when running in a quickly yielding loop
* Fixed WebSockets not being closed soon after close is called
* Fixed race condition on timer add/remove
* ~~Fixed some random crashes on an odd memory error~~
* Fixed WebSocket binary messages not being implemented correctly
* Fixed cursor not being reset on shutdown
* Fixed URL checks being missing from `http.get`
* Fixed read past EOF in `file.read(n)` on text handles
* ~~Fixed crash when passing bad argument #1 to `table.foreach`~~
* Fixed `arg` table being missing when running programs with cash
* Fixed missing `http.listen` function
* Fixed `window.getSize` not working with the new arguments
* Fixed cash being enabled by default
* Fixed shell failing to run in vanilla mode

## v2.5.3 - February 21, 2021
* Added automatic crash log uploading behind `snooperEnabled`
  * A prompt will appear on first boot asking to allow this
  * See https://www.craftos-pc.cc/docs/privacy for info about how your data is kept secure
* Added `snooperEnabled` config setting
  * This is currently only used for crash uploading, but more (very basic) analytics may be added in the future
* The `debug` API is now enabled by default
  * This is possible due to the `debug` API no longer requiring hooks
  * The availability of `debug` resolves a long-standing compatibility issue when run with the default config
* Bumped structure version to 2
  * New fields in `configuration`:
    * `snooperEnabled`
  * New fields in `PluginFunctions`:
    * `registerConfigSetting`
  * New fields in `Terminal`:
    * `mouseButtonOrder`
* Added new `registerConfigSetting` capability to allow access to plugin settings from `config` API
* Added ability to yield from line and count hooks
* Debug hooks now work as expected when no debugger is attached
  * This breaks `logErrors` and non-debugger breakpoints, but neither feature is used much
    * They may be brought back in the future, but they are not working for now
* Functions passed to `load` can now yield in standards mode only
  * This is behind standards mode due to the possible performance hit being higher than its usefulness
* Decreased mixer chunk size to 512
  * This should help with latency in things like the [sound plugin](https://gist.github.com/MCJack123/34ae1ca1a962504f32b34f2771f92326)
* Fixed HTTP handles not being closed when going out of scope/being deleted
* Fixed deadlock/high memory usage when resizing window to 0x0
* Fixed DPI issues in hardware renderer on Mac when switching between Retina and non-Retina displays
* Fixed deadlock when maximizing window with `snapToSize` enabled
* Fixed mouse buttons > 3 being returned as 1
* Mouse buttons > 3 no longer report in standards mode
* Fixed the order of mouse buttons reported with mouse_drag when multiple are down at the same time
* Fixed crash when loading plugin config
* Fixed plugin config not saving properly
* Fixed `\` not being treated as a directory separator
* Fixed `/\.{3,}/` not being treated the same as `.`
* Fixed `fs.getCapacity` in standards mode
* Rewrote abort timeout handling to no longer use debug hooks
* Changed `size_t` size in binary Lua chunks to 4 bytes for compatibility
* Added `lua_externalerror` to throw errors from another thread
* Improved performance of internal string split function

## v2.5.2 - January 23, 2021
* Bumped CC:T version to 1.95.2
  * Add `isReadOnly` to `fs.attributes` (Lupus590)
  * Many more programs now support numpad enter (Wojbie)
  * Hopefully improve edit's behaviour with AltGr on some European keyboards.
  * Fix the id program crashing on non-disk items (Wojbie).
* Bumped plugin API structure version to 1
  * New fields in `Computer`:
    * `forceCheckTimeout`
    * `redstoneInputs`
    * `redstoneOutputs`
    * `bundledRedstoneInputs`
    * `bundledRedstoneOutputs`
  * New fields in `configuration`:
    * `http_proxy_server`
    * `http_proxy_port`
    * `extendMargins`
    * `snapToSize`
  * New fields in `Terminal`:
    * `frozen`
* Added a proper implementation of the `redstone` API
* Added ability to mount computers with disk drives using `computer:<id>`
* Added automatic CCEmuX migration on first run, deprecating the old Java applet
  * This can be triggered manually with `--migrate`
* Added `extendMargins` config option to enable margins without standards mode
* Added `snapToSize` config option to automatically snap the window size to the nearest character size, removing extra margins (on by default)
* Added proper update verification via `sha256-sums.txt`
* Added HTTP proxy configuration
* Plugins now load in alphabetical order as expected
* Premature BIOS exits now error in standards mode
* `mouse_scroll` no longer reverses scroll direction if scroll direction is reversed in the OS (#185)
* Errors now appear if mounting/unmounting fails
* Increased maximum call stack height to 2000, allowing more nested coroutines/function calls
* `__len` metamethods can now yield as expected
* `getNextEvent` now checks the parameter stack's size before creating a new stack
* Made some minor improvements to `fixpath`
* Fixed memory corruption in raw mode
  * This is the REAL issue that v2.5.1.1 was supposed to fix - the fix didn't work on macOS
* Fixed `key_up` not firing for some keys in raw mode
* Fixed modulo operator returning unexpected results
* Fixed `os.startTimer` firing immediately when time < 0.05 in standards mode
* Fixed `os.epoch("local")` returning UTC time
* Fixed infinite recursion in some calls of `fs.getFreeSpace`/`fs.getCapacity`
* Fixed race condition when firing `term_resize`
* Fixed duplicate `term_resize` events being queued
* Fixed deadlock when passing invalid arguments to `term.setPixel`
* Fixed mounting the same path twice
* Fixed `mounter.unmount` returning failure on success
* Fixed `showFPS` type being set to integer
* Online: Hardware rendering is now enabled by default on macOS only to mitigate a scaling bug

## v2.5.1.1 - January 5, 2021
* Fixed a bug causing raw mode to crash, which breaks the extension
* Fixed a memory corruption bug that can happen when resuming coroutines
* Added link to new Discord server to MOTDs

## v2.5.1 - January 3, 2021
* Added `term.setFrozen` and `term.getFrozen` to stop rendering the terminal during critical sections (#165, #177)
* Added ability for `term.getPixels` to return a list of strings (#176)
* Added optional mode argument to `term.getSize` to return size of graphics mode screen
* Added Ctrl+F8 (Cmd+F8 on Mac) hotkey to keep current window on top of other applications (#175)
* Added "attach list" command to list available peripheral types
* Added extra error message explaining why a peripheral couldn't be attached
* Added some new MOTDs
* Margins in standards mode now stretch to the edge of windows if resized
* Improved performance of pixels in the hardware renderer by using a single streaming texture
* `fs.find` no longer uses `table.sort` for sorting, instead using C++ `std::list::sort`
* Replaced instances of `lua_newtable` with `lua_createtable` where possible for performance
* Show last C function for `std::exception`s (#170)
* Fixed `websocket.receive()` returning a bunch of junk data at the beginning
* Fixed `file.close`/`http_handle.close` not blocking access to other handle methods (#168)
* Fixed WebSockets not being closed on reboot (#169)
* Fixed `term.drawPixels` crashing with invalid arguments (#171)
* Fixed `fs.find` sometimes returning duplicate entries
* Fixed bug allowing mounts to be moved and deleted
* Fixed cursor color not being reset on reboot
* Fixed crash when unable to convert text input to CC charset (#181)
* Fixed wrong cursor coordinates being reported in the margins
* Fixed WASM building
* Removed legacy 3ms delay on timers

## v2.5 - December 25, 2020
* Reorganized code structure for easier maintenance of the codebase
  * Code has been divided between APIs, peripherals, renderers, and general functions
  * Resolved a large number (>1000) of warnings
  * Reduced usage of `extern` to headers only
  * Made all non-global variables `static`
* Rebased ROM on the CC: Tweaked ROM
  * The ROM is now equivalent to the CC: Tweaked ROM with additional CraftOS-PC features from the old ROM
  * With this, the CC:T Edition has been discontinued
    * It wasn't really necessary anyway
  * This should help improve compatibility in the ROM
* Updated CC:T version to 1.95.0
  * 1.94.0
    * Add getter for window visibility (devomaa)
    * Use term.blit to draw boxes in paintutils (Lemmmy).
    * Fix several programs using their original name instead of aliases in usage hints (Lupus590).
  * 1.95.0
    * Clear gets an option to reset the palette (Luca0208)
    * Use term.blit on initial paint render.
    * Add option to disable setting globals (Lupus590).
    * Fixed length check on function name in `expect` (MCJack123)
    * Allow strings or numbers in textutils.*tabulate.
    * Make fs.combine accept multiple arguments.
    * Added improved help viewer (MCJack123)
    * Added numpad enter support (TheWireLord).
    * Add functions to wrap text (Lupus590)
* Added new plugin API
  * API version has now been bumped to 10
  * New plugin init/deinit functions: `PluginInfo * plugin_init(const PluginFunctions * functions, const path_t& path);` & `void plugin_deinit(PluginInfo * info);`
  * Capabilities in the old API are now present as function pointers in the `PluginFunctions` structure passed to `plugin_init`
  * Additional functions are now available as well:
    * Access to the configuration, including custom settings for plugins
    * SDL event hooks
    * Virtual mounts
    * Running tasks on the main thread
  * See https://www.craftos-pc.cc/docs/plugins for more info on how to write plugins
* Added release note viewer
* Added support for CLI mode on Windows through PDCurses
* Added optional width and height options to `term.drawPixels`
* Added `term.getPixels` to read a region of pixels (LoganDark)
* Added optional solid color fill argument to `term.drawPixels` (LoganDark)
* Added some missing HTTP configuration options (besides black/whitelist)
* Added a panic handler that is more like ComputerCraft's in standards mode
* Added force-shutdown functionality when a computer refuses to close
* Added error handlers when an uncaught exception occurs
  * This should not happen, but if it does CraftOS-PC will no longer fully crash
* Added tracing of the last C Lua function called, hopefully helping memory corruptions be able to be fixed
* Optimized scroll and clear routines to directly copy memory
* Fixed `os.setAlarm` implementation to no longer use 100% CPU
* Fixed unknown config options being deleted
* Fixed close button and hotkeys not functioning when there are too many events in the queue (#154)
* Fixed `monitor.setTextScale` not functioning properly with non-6x9 fonts (#150)
* Fixed `term.getPixel` returning the wrong values in mode 1 (#159)
* Fixed a race condition while resizing when using the hardware renderer
* Fixed a race condition in `os.startTimer`
* Fixed a possible race condition when firing a timer
* Reduced the number of event timeout timers started when pulling events (#158)
  * This fixes an issue causing the timer thread to lock up trying to process start/cancel events
* Fixed some locking issues in terminals
* Fixed missing range checks in `term.drawPixels`
* Fixed `term.getPixel` crashing when accessing pixel at edge of screen (LoganDark)
* Fixed some issues with resizing the debugger
* Fixed computers hanging when closing the debugger on Linux (#157)
* Fixed debugger `locals` table sometimes not functioning properly
* Fixed some CraftOS-PC term functions being redirected (LoganDark)
* The craftos2-lua library can now be used in programs other than CraftOS-PC

## v2.4.5 - November 28, 2020
* Bumped CC:T version to 1.93.1
  * 1.94.0 support will be coming in v2.5
* CC:T Edition builds will now update to the standard ROM due to deprecation in v2.5
  * v2.5 is merging the CC:T ROM into the main ROM, so CC:T Edition downloads will be removed
  * Further auto-updates will download the standard version regardless of whether CC:T Edition was installed
  * v2.5 will be the last version with a CC:T Edition download available - after that, updating from CC:T Edition will fail
* Added some previously missing checks from `term` API functions
* Fixed `term.setGraphicsMode(false)` not working properly
* Fixed a possible crash in `file.readLine` on empty lines
* Fixed a memory leak in `os.startTimer`
* Fixed `ccemux.openDataDir` not working on Windows
* Fixed monitor_touch events not being sent when monitorsUseMouseEvents is disabled
* Fixed update message appearing when switching between standard and Accelerated versions

## v2.4.4 - October 18, 2020
* Bumped CC:T version to 1.93.0
* Added computer size counting to `fs.getFreeSpace`
  * This is only enabled with `standardsMode` on
  * Without `standardsMode`, old behavior is kept
  * May reduce performance of `fs.getFreeSpace`
* Fixed path resolution on Windows systems using non-ASCII characters in paths
  * This involved rewriting all of the path-related code to accept wide strings on Windows
    * Why did Windows have to keep using UTF-16??? >:(
* Fixed keyboard layouts not being respected (#138)
* Fixed string pattern matching with NUL bytes in patterns
  * This was previously done in the default ROM, but now Lua itself has been modified to fix the issue
  * Alternate ROMs (such as the CC:T ROM) no longer have this issue
* Fixed automatic directory creation reversing the new directory names (#137)
* Fixed some crashes related to the printer
* Fixed HTTP >= 400 response codes not being returned as failures
* Fixed `terminate` event not being sent unconditionally when a filter is set
* `term.screenshot` can now be called with a boolean value specifying whether to copy the image to the clipboard
* Added error message to `periphemu.create`
* Fixed name of scripts run through `wget`
* Added some extra argument checks to `shell` functions in `cash`
* Fixed `__tostring` metamethod missing from `cc.pretty` objects
* Fixed standalone CI builds

## v2.4.3 - September 12, 2020
* Updated CC:T version to 1.92.0
  * Add support for the `__pairs` metamethod.
  * `string.format` now uses the `__tostring` metamethod.
  * Correctly handle tabs within `textutils.unserializeJSON`.
* Fixed a crash when using plugins
* Fixed crashing when a Lua panic occurs
* Fixed a bug causing lesser performance in C CraftOS functions
* JIT: No longer using LuaJIT BitOp library as it is incompatible with `bit32`
  * Now using PUC Lua `bit32` implementation instead

## v2.4.2 - September 8, 2020
* Updated CC:T version to 1.91.0
  * Add string.{pack,unpack,packsize} (MCJack123)
  * Escape non-ASCII characters in JSON strings (neumond)
  * Make field names in fs.attributes more consistent (abby)
  * Fix textutils.formatTime correctly handle 12 AM (R93950X)
* Speakers are now emulated closer to how they work in ComputerCraft
  * Each speaker has its own channel pool
  * Channels are allocated as needed
  * The `maxNotesPerTick` option now properly caps notes every 1/20 s
  * Only one sound can be played at once
  * Channel numbers are now returned as a second argument
    * These can be passed to `stopSounds` to stop one channel
* Replaced custom bit32 library with standard Lua 5.2 implementation
  * This fixes a bug where functions can't take more than two arguments
  * This should also fix any other possible bugs in the bit32 library
* Fixed yielding from callbacks of certain functions:
  * `string.gsub`
  * `table.sort`
  * debug hooks
  * The only function that can't yield from a callback is `load`
* Added `--mc-save` argument to load computers from a Minecraft save
  * This reads from the default Minecraft save directory
  * If the save isn't stored there, you can't use this argument (use `--computers-dir` instead)
* Added date-specific MOTDs
  * One of these appears today (September 8)!
* Added support for hardware renderer in raw client mode
* Functions can now return an unlimited* number of values
* Updated cash to use `cc.require` module
* Fixed a bug causing the same MOTD to appear on each boot
* Fixed a bug causing a failure to connect to `localhost` over HTTP
* Fixed a bug with the \x escape code
* Modified internals of plugin loader, fixing a memory leak in the process
* JIT: Updated LuaJIT to 2.1.0-beta3
* JIT: Moved plugin path back to `plugins-luajit` as expected
* JIT: Added FFI library (available with `jit_ffi_enable` set to true)
* JIT: Removed `os.setHaltOnLongRunMode`, count hooks are no longer used
* JIT: Fixed a bug causing `io.read` to not work
* Windows: Added error dialog when using console-only options with non-console builds
* Mac: Application is now built as a Universal binary (excluding CraftOS-PC Accelerated, as LuaJIT does not fully support ARM64 yet)

## v2.4.1 - August 22, 2020
* Grayscale terminals now render using 16 grays instead of forcing colors to 4 grays (#132)
  * For example, you can now set the color to `colors.red` and the color will render as the grayscale version of red
* Fixed a crash when using the profiler (#129)
* Fixed incorrect cursor color in hardware terminal (#130)
* Last version check now saves right after boot
* Fixed up a whole bunch of stuff for WASM ports
* Re-introduced CraftOS-PC Accelerated for v2.4.1

## v2.4 - August 15, 2020
* Added new GUI terminal that uses hardware rendering (experimental)
  * It uses the GPU to accelerate drawing graphics
  * Uses the same base code that was used prior to v2.1, with updates that have been added since
  * Support can be enabled with the `useHardwareRenderer` config option or the `--hardware` or `-r hardware-sdl` CLI flag
  * A test script is available to see which is better for your system
    * You can run it with `gist run 802f64508a1f51b3244f5bcc0414ca22`
  * Vsync can be enabled with the `useVsync` option
  * The driver to use can be selected with the `preferredHardwareDriver` config option or the `-r` CLI flag
    * These are all of the possible drivers:
      * `direct3d`
      * `direct3d11`
      * `directfb`
      * `metal`
      * `opengl`
      * `opengles`
      * `opengles2`
      * `software`
    * Some drivers may not be available on your system
    * Use `craftos -r` to list all available drivers
* Updated CC:T version to 1.90.2
  * Add cc.image.nft module, for working with nft files. (JakobDev)
  * Add configuration to control the sizes of monitors and terminals. (`defaultWidth`/`defaultHeight`)
  * Fix string serialization error in textutils.serializeJSON.
* Brand-new app icon
  * Designed especially for CraftOS-PC
  * Inspired by the aesthetic of macOS Big Sur
  * High-resolution, textured, modern
* Added proper double-buffering to the terminal
  * This allows you to use the `term` API while CraftOS-PC is rendering
  * This speeds up programs that end up running lower than the render FPS
* Added introduction & update MOTD messages
  * When run for the first time (or in a new directory), an introduction message is displayed
  * When run after an update, a notification message is displayed, and suggests running `help whatsnew`
* Added support for `monitor_touch` event as expected (#119)
  * This removes the `mouse_*` events for monitors
  * The previous behavior can be restored by setting the `monitorsUseMouseEvents` config setting
    * This will also add the monitor's side as a fourth parameter
* Added `standardsMode` config option for strict compatibility with CC:T/CCEmuX
  * Forces time readings & timer resolution to be rounded to 50 ms (1 tick)
  * Adds back colored margins as shown in CC
  * More compatibility tweaks may be added under this option
* Added `startFullscreen` per-computer config option
* Disabled access to `romReadOnly` from CraftOS
* Fixed ABI compatibility with standard Lua libraries
  * This means you can now properly use Lua libraries (e.g. from LuaRocks) as plugins
  * Prefix the library name with `lua_` to tell CraftOS-PC to skip loading plugin metadata
* Updated plugin API version to 4
* Added ID to per-computer config error messages
* Modems now act as wired modems and can be used to access non-local peripherals (#122)
  * Non-local peripherals can now be listed with `peripheral.getNames()` when a modem is attached
* Fixed a crash when opening a debugger twice (#120)
* Fixed a rare race condition while getting an event (#123)
* Fixed an issue with saving screenshots to disk
* Fixed a bug where function key hotkeys don't work when Caps or Num lock is enabled
* Fixed a bug causing the `locals` table in the debugger to be missing
* Fixed `bit32.lrotate` returning incorrect results
* Fixed crash when closing CraftOS-PC while a WebSocket is shutting down
* Fixed crash when using too many nested coroutines (#124)
  * This was due to a bug that didn't update the C call count properly
  * Calling too many nested coroutines will result in a "C stack overflow" error
* Fixed a race condition in `periphemu.detach`
* Fixed incorrect prettification of non-contiguous tables (#125)
* Fixed raw terminals getting the same ID
* Added some more locks on timers to prevent crashes
* Fixed abort timeout not working before first yield
* Fixed missing UK spelling of term.nativePaletteColour
* Fixed modem hang due to locks not waiting
* Fixed missing modem distance field in `modem_message` event
* Fixed utf8.char returning `%U` for each character
* Fixed a random crash when calling a NULL C function (this shouldn't happen)
* Fixed styling of message dialogs on Windows
* Fixed events going to the wrong window in raw mode
* Fixed Ctrl-R/S/T shortcuts not working in non-GUI modes
* Disabled fullscreen support in WASM to avoid crashes (#100)
* Mac: macOS 10.13 or later is now required for official builds
  * Users on older versions are still able to build and run CraftOS-PC themselves
* Windows: The solution now uses vcpkg for dependencies, so NuGet and separate builds are no longer required
* Windows: PDF printing is now supported in all builds
* Windows: Changed Visual C++ Runtime and other libraries to dynamic linking
  * This reduces executable size, but the VCRT redistributable is now strictly required
  * More DLLs will be required as well

## v2.3.4 - June 29, 2020
* Updated CC:T version to 1.89.1
  * This includes full testing on the CC:T test set
  * CraftOS-PC now fully passes all CC:T tests
* Added an error dialog when an error occurs reading the configuration files
  * The dialog includes a message describing where the JSON parsing error occurred
  * If an error occurs, the defaults will be used, and no changes will be saved
* Added proper `ingame` locale for `os.day/time/epoch`
  * This is based on a 20-minute clock that starts when the computer boots
  * This change is to improve compatibility with CCEmuX/CC:T
* Added CCEmuX command-line flags
  * `--assets-dir`, `--computers-dir`, `--start-dir`, `--data-dir` (alias of `--directory`), `--plugin`, `--renderer`
  * The `--start-dir` flag only affects the initial computer (specified by `--id`)
* Headless mode now reports a color terminal (for advanced testing)
* `os.time` now properly parses tables returned by `os.date("*t")`
* CraftOS-PC now reports its real version and CC version in HTTP User-Agent strings
* `fs.delete` no longer errors when deleting a non-existent file
* Fixed `io.open` creating a directory instead of opening a file in write mode (#116)
* Fixed `fs.getDrive` returning the wrong results
* Fixed a whole bunch of warnings
* `fs.getSize` now returns 0 when used on a directory instead of erroring
* Fixed a bug causing audio glitches when playing low notes through the speaker
* Fixed an error when writing `nil` or a number to a file (#118)
* Fixed a crash when an invalid format string is passed to `os.date`
* Fixed missing cc.completion and cc.shell.completion modules
* Fixed programs getting the path to the program in arg[0] instead of the command as it was run
* Raised maximum Lua call depth to 32768 (from 256)
* `loadstring` now automatically adds an `=` sign to the chunk name
* Fixed `debug.getlocal` not returning function parameter names
  * This fixes the `cc.pretty` module's parameter detection function
* Fixed vulnerability in `io` library
* Fixed `io.lines()` with no arguments
* Fixed missing `*L` option for `io.read`/`file:read`
* Fixed `io.write`/`file:write` not returning file handle
* Fixed `string.format` erroring when using nil as a string parameter
* `fs.copy` can now copy folders as expected
* Fixed `fs.find("/")` returning an empty table
* Fixed `fs.attributes` having the wrong name
* Fixed `fs.makeDir` not erroring when it tries to create a directory where a file is already present
* Fixed a boatload of string differences to comply with CC: Tweaked
* Fixed a bunch more small inconsistencies
* Fixed `io.open` not creating all parent directories if they're missing
* Added support for custom options to `io.lines`
* Fixed support for '+' modes in `io.open`
* Fixed errors in Lua prompt showing `[string "lua"]:` instead of `lua:`

## v2.3.3 - June 6, 2020
* Added a `record` domain to drives
  * Inserting a disk in the format `record:<name>` will insert a music disc from `minecraft:music_disc.*` in the speaker sound data
  * `disk.insertDisk("left", "record:cat")` will insert the "cat" music disc from the sound files as a playable music disc
* Updated `gist` (again)
* Fixed an error preventing the debugger from starting up
* Fixed `fs.readLine` returning weird strings on empty lines (#113)
* Fixed an error when writing numbers to the screen
* Fixed a bug that overrode User-Agent and Content-Type HTTP headers
* Fixed audio clipping when playing pitched sound through the speaker
* Fixed a crash when playing some notes on Windows
* Fixed stack corruption in the debugger
* Fixed origin point of paintutils calls in graphics mode
* Fixed a bug that may lead to events being sent with the wrong parameters
* Computers now shut down automatically if the BIOS exits

## v2.3.2 - May 23, 2020
* Feature parity with CC: Tweaked 1.88.1
  * Add peripheral.getName - returns the name of a wrapped peripheral.
  * The Lua REPL warns when declaring locals (lupus590, exerro)
  * Add fs.isDriveRoot - checks if a path is the root of a drive.
  * cc.pretty can now display a function's arguments and where it was defined. The Lua REPL will show arguments by default.
  * Move the shell's require/package implementation to a separate cc.require module.
* CraftOS-PC builds with the CC: Tweaked ROM are now available
  * These builds use the stock ROM, with CraftOS-PC-specific programs included
  * Some ROM features (notably, autocomplete for CCPC programs) may not be available
  * CC:T Edition will replace the standard CraftOS-PC ROM, so they can't be used alongside each other
    * This does not apply to Mac builds, which are distributed as single apps
  * Ubuntu: Install the `craftos-pc-cct-data` package to use the CC:T ROM
* `bit32` is now the default bit API
  * The BIOS will set up a wrapper to allow programs to continue using `bit`
* Added `file.readLine` to file handles opened in binary mode
* Added `getLabel` method to computer peripherals
* Rewrote Gist program again
  * This will be changing once again in the next version
* Added new plugin capabilities
  * `register_queueTask`: Returns a function of the form `void* queueTask(std::function<void*(void*)> func, void* userdata, bool async)`
  * `register_getComputerById`: Returns a function of the form `Computer * getComputerById(int id)`
  * `get_selectedRenderer`: Returns a number representing the currently selected renderer
* Capability callbacks now receive the name of the function it's called for
* `plugin_info` can now safely throw errors
  * If a plugin throws an error here, its API will not be loaded and the error will be reported to the user on boot
* Added SDLTerminal::resizeWholeWindow, which resizes a terminal and its window
* Fixed `file.readLine` only returning up to 255 characters
* Fixed a possible race condition

## v2.3.1 - May 9, 2020
* CraftOS-PC is now available as a notarized build on Mac
  * This means it is no longer necessary to right-click the app to open it for the first time
* Added new crash handler that outputs a stack trace for debugging
  * Mac & Linux builds will output the stack trace to the console
  * Windows builds will only show a dialog showing a crash occurred; the actual stack trace will be saved in a minidump
  * Mac app builds will show a crash report dialog as usual
* Added mouse_move event (disabled by default)
  * The `mouse_move_throttle` config option sets the amount of time between events sent
  * It is disabled by default because SquidDev-CC/CC-Tweaked#434 is still in progress
    * Set `mouse_move_throttle` to a whole number (50 is recommended) to enable
* Added file.readAll for binary file handles
* Added maxOpenPorts config option to cap maximum number of open ports
* Added a couple of upcoming CraftOS features from CC: Tweaked upstream
* Added `term.showMouse` to toggle whether the real mouse cursor should appear
* Added startComputer capability to plugins
* Added return value from `config.set` specifying when the change will take effect
* Improved error message reporting across the board
* Using non-vanilla programs in vanilla mode now shows a descriptive error
* Fixed crash when creating directories
* Fixed term.drawPixels not working properly for table arguments when in mode 1
* Fixed writing text files with binary by writing in UTF-8
* Fixed crash when an HTTP request times out
* Fixed a security vulnerability involving the drive peripheral
* Fixed multiple mouse_drag events being sent for the same position (#107)
* Fixed computer not unpausing when closing debugger
* Fixed mount dialog showing incorrect text relating to read-write mount
* Fixed some bugs with the modem
* Fixed a bunch of small memory leaks & issues
* Fixed a race condition when taking a screenshot
* Fixed a bug where the size operator of a table may not represent its actual size
* Fixed a crash when using a modem or debugger after rebooting
  * This was done by adding a `reinitialize` method to peripherals
    * This method is called after restarting a computer while having the peripheral attached
    * Use this if your peripheral holds any references to the computer's Lua state or a sub-thread
* Fixed a bug where coroutines created before attaching a debugger won't be able to be paused from the debugger when resumed
* Fixed `http.get` and `http.request` not allowing using a table as a parameter
* Fixed a bug where the computer wouldn't boot when `disable_lua51_features` is enabled (#110)

## v2.3 - April 26, 2020
* Massively improved performance of emulation
  * Speed issues are caused by the debugger's hooks (specifically line hooks)
  * This version only enables the required hooks depending on what features are required
  * In general, more debugging features -> lower performance
  * Depending on the machine, expect between 4-20x faster emulation
* Added official support for WASM, including an online client (CraftOS-PC Online)
  * CraftOS-PC Online is available at https://www.craftos-pc.cc/online/
  * Supports Chrome/Chrome-based browsers, and new versions of Firefox with the following options enabled in `about:config`:
    * `javascript.options.shared_memory`
    * `dom.postMessage.sharedArrayBuffer.withCOOP_COEP`
    * `browser.tabs.remote.useCrossOriginEmbedderPolicy`
    * `browser.tabs.remote.useCrossOriginOpenerPolicy`
* Added speaker peripheral
  * Includes open-source sounds for notes
  * Users can manually add in custom sounds (including offical Minecraft ones)
    * See https://www.craftos-pc.cc/docs/periphemu#speaker-sounds for more info
* Updated CC:T compatibility to 1.87.1
  * Added `fs.getAttributes` and `fs.getCapacity` functions
  * Added optional timeout argument to `websocket.receive`
  * Ported pretty printer to `lua.lua`
  * Added `__len` metamethod for tables
  * Strings now accept `\xNN`, `\u{NNN}`, and `\z` escape codes
  * Added `utf8` library
  * Added base argument to `math.log`
  * Moved `table.pack` and `table.unpack` out of the ROM and into C
  * Added `textutils.unserializeJSON`
  * Rewrote `settings` API
  * Enabled MOTD by default
  * `http.post` now accepts a table as an argument as expected
* Added raw terminal renderer
  * See the documentation (https://www.craftos-pc.cc/docs/rawmode.html) for more info
* Added TRoR (Terminal Redirect over Rednet) terminal renderer
* Added the ability to place modems on multiple separate networks
  * This can be done by adding a third argument when attaching the peripheral, specifying the network ID
  * Modems with different network IDs won't be able to communicate with each other
* Added prompt when mounting a real directory, for security
  * This is to mitigate the possibility that a malicious script deletes arbitrary files
  * A prompt also appears when changing the `mount_mode` option
  * To disable this (not recommended), set `showMountPrompt` to false in global.json
    * This option cannot be changed inside CraftOS-PC
* Added filesystem merge mount support
* Added standalone executable option for Windows & Linux
* Added scrollback to debugger console
* Added pop-up when a computer is hung and ignores terminate events
* Revamped `gist` program with new commands
  * Added `edit`, `delete`, and `info` commands
  * Gist logins now use Personal Access Tokens instead of a private server with OAuth2
* Improved command-line usage
* Cursor color can now be changed as expected
* Fixed 256-color graphics mode on monitors
* Fixed crash when receiving tables over a modem connection
* Fixed a crash relating to timers
* Fixed memory leak when taking a screenshot on Mac
* Fixed missing Content-Type and Content-Length headers in HTTP requests
* Fixed mouse event location in graphics mode when using an HD font
* Fixed a crash when an error occurs while creating a peripheral
* Fixed missing error messages when an HTTP connection fails
* Accessing the string metatable is no longer blocked
* Removed path argument to `term.screenshot()`
* Added a rate limit to `term.screenshot()`

## v2.2.6 - March 15, 2020
* Added gist.lua to the default ROM
* Reboot now resets graphics mode
* Added `--rom` flag to allow changing ROM directory (#84)
* Fixed extended ASCII character input conversion (#85)
* Fixed key events not being sent when Ctrl+[TSR] are pressed (#86)
* Fixed bug in fs.getFreeSpace
* Fixed term.drawPixels cutting off bottom
* Fixed crash when rebooting with events in queue
* Fixed disk mount paths
* Fixed deadlock when a peripheral method causes an error
* Fixed getPixel boundaries being too small
* Fixed paste events sending \r characters on Windows (#95)

## v2.2.5 - February 24, 2020
* Added `--directory` argument to specify custom data directory
* Added `--id` argument to specify launch computer, overriding `initialComputer` config option
* Rewrote `--script` argument to no longer shadow real `startup.lua` (#83)
* Added header option to `http.websocket` to match CC: Tweaked behavior (#82)
* Fixed crash when writing below screen
* Fixed crash when closing WebSocket
* Fixed terminal background not being affected by palette changes

## v2.2.4 - February 14, 2020
* Computer label now displays in title bar (including the terminal emulator's title in CLI mode)
* Added cliControlKeyMode config option (#78, #79)
* Added --args argument to command line to pass arguments to --script (#78)
* Added UTF-8 support over HTTP
* Unicode characters are now replaced with '?' (rather than being dropped)
* file.read() now accepts a length in text mode
* Insecure WebSockets ('ws://') no longer attempt to use secure WebSockets
* Fixed WebSocket connection failure when no path is provided
* Fixed crash when closing computer with open WebSockets
* Fixed crash in fs.list() when a non-string argument is passed (#77)
* Fixed crash when closing computer with running timers
* Fixed crash when reading a string with an invalid UTF-8 codepoint
* Fixed crash when too many events are queued
  * This also fixes a crash when pasting long text in CLI mode
* Fixed glitch/crash when resizing CLI window (#78, #80)
* Fixed issue with clicking the mouse in CLI mode (#78)
* Fixed 100% CPU usage when using CLI mode
* Fixed term.setCursorPos clamping the cursor to [0, width)
* Fixed term.clear filling with black/white instead of selected BG/FG combo
* Fixed file.readAll() on Windows when the file has no end newline
* Fixed issue when reading single character at a time from text files
* Fixed POST data stopping at NUL characters
* Made some HTTP error messages more descriptive
* Fixed HTTP implementation in WASM port
* Added --enable-wasm flag to ./configure to allow building for WASM
* Added window switcher for WASM builds
* Fixed debugger locals once again
* table.concat() now works with non-contiguous tables
* Fixed crash when a computer's config file was corrupt or empty

## v2.2.3 - January 13, 2020
* Added `useHDFont` virtual config option
* Added support for reading UTF-8 files
* Pressing enter in the debugger's Call Stack tab now keeps file open
* Stepping multiple lines in the debugger now works as expected
* Fixed accessing locals in the debugger
* Updated cash to latest version
* NUL bytes in string patterns no longer mark the end of the pattern
* `io.input()`/`io.output()` no longer fail when called with `io.stdin`/`io.stdout`, `io.stderr` (#75)
* Fixed bug that caused `file.read(x)` on binary files to always return one byte
* Fixed segmentation fault when Lua crashes
* Fixed some bugs relating to vanilla mode (#76)
* Added preliminary Emscripten/WASM support (#73)

## v2.2.2 - January 2, 2020
* Feature parity with CC: Tweaked 1.86.0
* Added MOTD (disabled by default)
* Added Ctrl+S and Ctrl+R hotkeys (#70)
* Added configurable recording length and framerate
* Fixed crash when resizing window to a smaller size (#71)
* fs.makeDir no longer errors when creating a directory that already exists
* io.write no longer adds a newline to the end of the string (#68)
* os.setComputerLabel no longer errors when no label is provided (#69)
* Fixed string length bug in debugger.print

## v2.2.1 - December 21, 2019
* Added cash 0.3 as an optional shell (set bios.use_cash in the settings)
* Added help files for CraftOS-PC features
* Improved speed of terminal writing
* Reduced processing time used by terminal hook
* Verified compatibility with CC: Tweaked using official tests
* Fixed a security flaw that allowed modification of read-only mounts
* Resolved an issue that prevented monitors from drawing
* Fixed a bug where secure WebSockets failed to connect with "WebSocket Exception"
* Fixed a possible deadlock when stepping the debugger
* Removed turtle, pocket, command programs; removed bmpview
* Added --small flag to screenfetch

## v2.2 - December 15, 2019
* Added F12 hotkey to copy screenshot to clipboard (Windows & Mac only)
  * Calling `term.screenshot("clipboard")` will have the same effect
* Fixed default IO handles (stdin/out/err)
* Changed `_HOST` variable to more accurately represent ComputerCraft
* Fixed `delete.lua`
* Added `register_termQueueProvider` capability, which recieves the address of the `void termQueueProvider(Computer *comp, const char *(*callback)(lua_State*, void*), void* data)` function
## v2.2p1 - December 1, 2019
* Added new debugger peripheral
  * Opens in separate window
  * Four tabs available
    * Debugger: GDB-style debugging terminal
    * Call Stack: Visual backtrace viewer, shows files
    * Profiler: Measures function call count & time
    * Console: Allows printing debug messages outside of the main terminal
  * Computer can be paused with `debugger.break()`, `debug.debug()`, or Ctrl-T in debugger
  * See [the documentation](https://www.craftos-pc.cc/docs/debugger.html) for more information
* Added breakpoints
  * Can be set with `debugger.setBreakpoint(file, line)` or `debug.setbreakpoint(file, line)`
  * Does not require debugger to be attached
    * When no debugger is available, `lua.lua` is executed
* Fixed #1 ("attempt to yield across metamethod/C-call boundary" when using pcall)
  * Tracebacks/errors now function normally
  * This was accomplished with a patch that allows yielding across pcall's in Lua 5.1.0
    * Thanks to [EveryOS](https://github.com/JasonTheKitten) for sharing the patch and for sending me a fixed version for 5.1.5
* Moved per-user save directory to more platform-appropriate locations
  * Windows: `%appdata\CraftOS-PC`
  * Mac: `~/Library/Application Support/CraftOS-PC`
  * Linux: `$XDG_DATA_HOME/craftos-pc` (usually `~/.local/share/craftos-pc`)
  * Data will be migrated when running CraftOS-PC v2.2 for the first time
* Added brand-new plugin API
  * Plugins should now provide a `plugin_info` function
  * This function pushes a table with the API version and requested capabilities onto the Lua stack
  * Capabilities are requested by adding a callback function for a function in CraftOS-PC
  * These callbacks will recieve the requested function as Lua userdata as the first value on the stack
  * Available capabilities in v2.2:
    * `register_getLibrary`: Recieves the address of the `library_t * getLibrary(std::string name)` function
    * `register_registerPeripheral`: Recieves the address of the `void registerPeripheral(std::string name, peripheral_init initializer)` function
    * `register_addMount`: Recieves the address of the `bool addMount(Computer * comp, const char * real_path, const char * comp_path, bool read_only)` function
* Added new config options
  * `vanilla`: Set to `true` to disable all CraftOS-PC features
  * `initialComputer`: The first computer that starts when opening CraftOS-PC
* Added `periphemu.names()` which returns a list of all available peripherals (unsorted)
* Added `term.drawPixels(x, y, lines)` which draws a table of lines at a position
  * Each line can either be a string or a table of individual pixels
* Fixed `debug.sethook`, `debug.gethook`
* `debug.debug()` now functions the same as hitting a breakpoint
* Added hooks for errors, coroutine resumptions and yields
  * Running `catch error` in the debugger will pause execution when an error occurs
  * Errors are logged to stdout if the `logErrors` config option is enabled
* Fixed a bug when creating directories
* Fixed timers with <= 0 length
* Fixed printing extended characters in the console
* Rewrote implementation of the modem peripheral
  * Tables and strings are now properly copied
  * Fixed `rednet` API
* Replaced Lua plugin loader with custom loader
* Made SDL_mixer an optional dependency
* Fixed many more bugs

## v2.1.3 - November 3, 2019
* Added two new configuration variables
  * `mount_mode` allows restricting mounting to read-only or disabled
    * `none` (0) disables mounting altogether
    * `ro strict` or `ro_strict` (1) forces mounts to be read-only
    * `ro` (2) sets mounts to read-only by default (default option)
    * `rw` (3) sets mounts to read-write by default (v2.1.2 behavior)
  * `configReadOnly` disables `config.set()`
* Bugfixes
  * #57: config.get("readFail") returns not real nil
  * #58: Crash on macOS on rebooting
  * #60: `edit` causes segfault when a line goes off the screen if there are more than ~100 lines in a file
  * Fixed crash when non-ASCII characters are present in computer label
    * Computer labels are now stored in Base64 by default; if you want to change the label in the JSON file, simply set `computerLabel` to the ASCII label and remove the `base64` key.

## v2.1.2 - October 23, 2019
* Fixed some crashes on reboot/quit
* Fixed list subcommand of config
* Changed return value of `config.getType()` to type-string
* Added PNG screenshot support for Windows builds
* Fixed "not supported" icon on macOS versions below Catalina
* Added launcher icon on Linux

## v2.1.1 - October 16, 2019
* Fixed http handle methods when using binary mode
  * `wget` now works as expected
* Fixed `os.clock()` implementation
* Fixed a bug regarding embedded `\0` characters
  * `term.write()` can now print strings with NUL characters
  * String patterns do not work with NUL characters in the query string
* Removed auto-updater on platforms other than Windows & Mac
  * These systems can either update through a package manager or rebuild from the release source
* Ubuntu PPA: Fixed CLI support
* Mac: Moved license to `LICENSE.txt` in disk image
  * Requiring the license to be agreed to breaks the auto-updater

## v2.1 - October 13, 2019
* Added 256 color mode
  * This can be activated with `term.setGraphicsMode(2)`
  * Use numbers 0-255 to choose color instead of `colo(u)rs` API
  * Colors 0-15 are set to default colors, use `term.setPaletteColor([0-255], r, g, b)` to set color palette
* Added auto-updater, allowing one-click installation of new updates (Windows/Mac only)
* Added custom fonts
  * Set `customFontPath` to the (real) path to the font (BMP format only)
  * Set `customFontScale` to the font scale
    * 1 = HD font (12x18), 2 = normal font (6x9), 3 = 2/3 size font (4x6)
* Fixed graphics issues on Linux with NVIDIA graphics
  * Renderer has been rewritten to fix this
  * Should improve performance when using a reasonable number of windows
    * May degrade performance when using >1000 windows at once on a slow system
* Added CLI mode
  * This can be activated with the `--cli` option
  * Not available on Windows (use WSL if necessary)
* Added more HTTP methods
* Added fullscreen hotkey (F11)
* Added `romReadOnly` config option
* Added Ubuntu 19.10 & Arch packages
* Added autoconf script to compile
* Many bugfixes
  * Fixed compositor disabling on KDE
  * Fixed `monitor.setTextScale`
  * Fixed `term.screenshot`
  * Fixed `fs.getDrive`
  * Fixed window resizing on Linux
  * Fixed many compilation warnings
  * Cleaned up code to be more C++-like

## v2.0.1 - September 13, 2019
* Added automatic update checking
  * Pops up a window alerting the user about the new version
  * Can be disabled with the `checkUpdates` config option
  * Will be expanded into an auto-updater in v2.1
* Added `term.setPaletteColor(color, hex)` syntax
* Fixed plug-in loading on Mac
* Fixed computer label always getting reset to `true`
* Fixed `ignoreHotkeys` not being saved

## v2.0 - September 8, 2019
* Fixed high CPU usage, now uses ~5% CPU at idle
* Moved plugin directory on Mac to `CraftOS-PC.app/Contents/PlugIns` 
* Fixed HTTP requests

## v2.0p1 - August 25, 2019
* Added WebSocket support
  * `http.websocket(url)` will open a client WebSocket connecting to `url`
  * `http.websocket()` will open a server WebSocket that can be connected to
* Added plugin/module system
  * Add plugins in the `<installation directory>/plugins` folder
  * See [`DOCUMENTATION.md`](https://github.com/MCJack123/craftos2/blob/master/DOCUMENTATION.md) for more details
* Added `file.seek` method
* Added `io` library with proper filename redirects
* Added `drive` peripheral
  * Use `disk.insertDisk(path)` to mount a folder or audio file
  * Use `disk.insertDisk(id)` to mount a floppy disk from `~/.craftos/computer/disk/<id>`
* Added a whole bunch of features from CC: Tweaked, making CraftOS-PC mostly compatible with CC:T
* Added new ROM features
* Moved font into executable (no longer requires `craftos.bmp`)
* Redid HTTP client/server code, removing some dependencies (expect bugs)

## v2.0b3 - August 20, 2019
* Rewrote codebase to pure C++
* Added multi-computer support
  * Computers can be added with `periphemu.create(<id>, "computer")`
  * Can be added from the shell with `attach <id> computer`
  * Both forms attach the computer peripheral to the current computer
* Added modem peripheral
  * Still WIP, expect some bugs
* Added GIF recording support
  * Press F3 to toggle recording
  * Shows red circle in corner while recording
  * Limited to 15 seconds for performance reasons
  * Saves to `~/.craftos/screenshots/<time>.gif`
* Added coroutine override to partially solve some problems in #1 
* Moved Windows install directory to 64-bit Program Files
* `.craftos` directory is now created on boot
* Reimplemented fs.find, fixing #4 (fs.find returns {} on Windows)
* Added some CC: Tweaked features
  * #16 (Add monitor.getTextScale())
  * #24 (Add automatic HTTPS requests)
  * #25 (Add term.nativePaletteColor())
  * #29 (Make os.time() and os.date() case-insensitive)
* Moved all dynamic libraries into Mac app (fixes dyld errors)
* Added switch to auto-run script (`--script <file path>`)
* Added switch to run headless from the console (--headless) (does not work on Windows build, recompile for the console subsystem to use)

## v2.0b2 - August 13, 2019
* `mounter` API
* HTTP server listeners
* Terminal & monitor resizing
* Window icons
* Holding Ctrl-T sends `terminate` event
* Added built-in screenshots
  * Take a screenshot with F2 or call `term.screenshot(path)`
    * `path` is a global path outside of CraftOS and is optional
  * Saved to `~/.craftos/screenshots` by default
* Added some configuration variables
  * `debug_enable (false)`: Set to true and reboot to enable debug API (or disable removal)
  * `ignoreHotkeys (false)`: Set to true to disable hotkeys (F2)
  * `isColor (true)`: Toggles whether the computer is an Advanced Computer or Standard Computer
* Distributed builds now use the CraftOS-PC ROM instead of the ComputerCraft ROM
* Moved `~/.craftos/config.json` to `~/.craftos/config/global.json`
* Big changes under the hood
  * Switched to software rendering from hardware rendering (see #5)
  * Rearranged a bunch of the code in preparation for multiple computers
  * Changed event queuing system to put parameter threads under a separate state rather than the main state
    * This should a) reduce memory leakage or b) improve event stability

## v2.0b1 - August 7, 2019
* Near full compatibility with ComputerCraft 1.8
* `config` API - adjust ComputerCraft configuration
* `periphemu` API - attach peripherals to the computer
  * monitor
  * printer
* Terminal graphics mode
* debug API access

# ComputerCraft History

New Features in ComputerCraft 1.80:

* Added .getResponseHeaders() to HTTP responses.
* Return a HTTP response when a HTTP error occurs.
* Added a GUI to change ComputerCraft config options.
* os.time() and os.day() now accept parameters to give the real world time.
* Added os.epoch()
* Monitor text now glows in the dark.
* Added a "Pocket Computer upgrade API" so mod developers can add their own pocket upgrades.
* Added pocket.equipBack()/pocket.unequipBack() to add/remove pocket upgrades.
* Added term.setPaletteColor()/term.getPaletteColor() to change/check colors
* Added colors.rgb8()/colours.rgb8() 
* Performance improvements to fs.find
* Requires the player to be interacting with the computer when typing
* Disk labels are limited to 32 characters
* Labels can now only include characters within the printable range ( to ~)
* Various model improvements
* There is now a configurable file descriptor limit
* Threads are now daemon threads
* Termination signals are now sent unless the computer is off
* Fixed compilation errors
* Now handles tile entity changes
* GPS coordinates now have to be numbers
* Turtle upgrades now act as tools and peripherals
* The Filesystem.list result is now sorted
* The number of values to unpack can now be manually specified
* Small terminal & monitor rendering improvements
* General improvements to the documentation
* Redstone inputs are no longer reset when adding peripherals
* Turtles now use tinting
* shell.resolveProgram now picks up on *.lua files
* Fixed a handful of bugs in ComputerCraft
* Added speaker block, turtle upgrade, pocket upgrade, and peripheral api
* Startup can now be a directory containing multiple startup files
* Added .getLabel to the computer peripheral

New Features in ComputerCraft 1.79:

* Ported ComputerCraftEdu to Minecraft 1.8.9
* Fixed a handful of bugs in ComputerCraft

New Features in ComputerCraft 1.77:

* Ported to Minecraft 1.8.9
* Added "settings" API
* Added "set" and "wget" programs
* Added settings to disable multishell, startup scripts, and tab completion on a per-computer basis. The default values for these settings can be customised in ComputerCraft.cfg
* All Computer and Turtle items except Command Computers can now be mounted in Disk Drives

New Features in ComputerCraft 1.76:

* Ported to Minecraft 1.8
* Added Ender Modems for cross-dimensional communication
* Fixed handling of 8-bit characters. All the characters in the ISO 8859-1 codepage can now be displayed
* Added some extra graphical characters in the unused character positions, including a suite of characters for Teletext style drawing
* Added support for the new commands in Minecraft 1.8 to the Command Computer
* The return values of turtle.inspect() and commands.getBlockInfo() now include blockstate information
* Added commands.getBlockInfos() function for Command Computers
* Added new "peripherals" program
* Replaced the "_CC_VERSION" and "_MC_VERSION" constants with a new "_HOST" constant
* Shortened the length of time that "Ctrl+T", "Ctrl+S" and "Ctrl+R" must be held down for to terminate, shutdown and reboot the computer
* textutils.serialiseJSON() now takes an optional parameter allowing it to produce JSON text with unquoted object keys. This is used by all autogenerated methods in the "commands" api except for "title" and "tellraw"
* Fixed many bugs

New Features in ComputerCraft 1.75:

* Fixed monitors sometimes rendering without part of their text.
* Fixed a regression in the "bit" API.

New Features in ComputerCraft 1.74:

* Added tab completion to "edit", "lua" and the shell.
* Added textutils.complete(), fs.complete(), shell.complete(), shell.setCompletionFunction() and help.complete().
* Added tab completion options to read().
* Added "key_up" and "mouse_up" events.
* Non-advanced terminals now accept both grey colours.
* Added term.getTextColour(), term.getBackgroundColour() and term.blit().
* Improved the performance of text rendering on Advanced Computers.
* Added a "Run" button to the edit program on Advanced Computers.
* Turtles can now push players and entities (configurable).
* Turtles now respect server spawn protection (configurable).
* Added a turtle permissions API for mod authors.
* Implemented a subset of the Lua 5.2 API so programs can be written against it now, ahead of a future Lua version upgrade.
* Added a config option to disable parts of the Lua 5.1 API which will be removed when a future Lua version upgrade happens.
* Command Computers can no longer be broken by survival players.
* Fixed the "pick block" key not working on ComputerCraft items in creative mode.
* Fixed the "edit" program being hard to use on certain European keyboards.
* Added "_CC_VERSION" and "_MC_VERSION" constants.

New Features in ComputerCraft 1.73:

* The "exec" program, commands.exec() and all related Command Computer functions now return the console output of the command.
* Fixed two multiplayer crash bugs.

New Features in ComputerCraft 1.7:

* Added Command Computers
* Added new API: commands
* Added new programs: commands, exec
* Added textutils.serializeJSON()
* Added ILuaContext.executeMainThreadTask() for peripheral developers
* Disk Drives and Printers can now be renamed with Anvils
* Fixed various bugs, crashes and exploits
* Fixed problems with HD texture packs
* Documented the new features in the in-game help

New Features in ComputerCraft 1.65:

* Fixed a multiplayer-only crash with turtle.place()
* Fixed some problems with http.post()
* Fixed fs.getDrive() returning incorrect results on remote peripherals

New Features in ComputerCraft 1.64:

* Ported to Minecraft 1.7.10
* New turtle functions: turtle.inspect(), turtle.inspectUp(), turtle.inspectDown(), turtle.getItemDetail()
* Lots of bug and crash fixes, a huge stability improvement over previous versions

New Features in ComputerCraft 1.63:

* Turtles can now be painted with dyes, and cleaned with water buckets
* Added a new game: Redirection - ComputerCraft Edition
* Turtle label nameplates now only show when the Turtle is moused-over
* The HTTP API is now enabled by default, and can be configured with a whitelist of permitted domains
* http.get() and http.post() now accept parameters to control the request headers
* New fs function: fs.getDir( path )
* Fixed some bugs

New Features in ComputerCraft 1.62:

* Added IRC-style commands to the "chat" program
* Fixed some bugs and crashes

New Features in ComputerCraft 1.6:

* Added Pocket Computers
* Added a multi-tasking system for Advanced Computers and Turtles
* Turtles can now swap out their tools and peripherals at runtime
* Turtles can now carry two tools or peripherals at once in any combination
* Turtles and Computers can now be labelled using Name Tags and Anvils
* Added a configurable fuel limit for Turtles
* Added hostnames, protocols and long distance routing to the rednet API
* Added a peer-to-peer chat program to demonstrate new rednet capabilities
* Added a new game, only on Pocket Computers: "falling" by GopherATL
* File system commands in the shell now accept wildcard arguments
* The shell now accepts long arguments in quotes
* Terminal redirection now no longer uses a stack-based system. Instead: term.current() gets the current terminal object and term.redirect() replaces it. term.restore() has been removed.
* Added a new Windowing API for addressing sub-areas of the terminal
* New programs: fg, bg, multishell, chat, repeat, redstone, equip, unequip
* Improved programs: copy, move, delete, rename, paint, shell
* Removed programs: redset, redprobe, redpulse
* New APIs: window, multishell
* New turtle functions: turtle.equipLeft() and turtle.equipRight()
* New peripheral functions: peripheral.find( [type] )
* New rednet functions: rednet.host( protocol, hostname ), rednet.unhost( protocol ), rednet.locate( protocol, [hostname] )
* New fs function: fs.find( wildcard )
* New shell functions: shell.openTab(), shell.switchTab( [number] )
* New event "term_resize" fired when the size of a terminal changes
* Improved rednet functions: rednet.send(), rednet.broadcast() and rednet.receive() now take optional protocol parameters
* turtle.craft(0) and turtle.refuel(0) now return true if there is a valid recipe or fuel item, but do not craft of refuel anything
* turtle.suck( [limit] ) can now be used to limit the number of items picked up
* Users of turtle.dig() and turtle.attack() can now specify which side of the turtle to look for a tool to use (by default, both will be considered)
* textutils.serialise( text ) now produces human-readable output
* Refactored most of the codebase and fixed many old bugs and instabilities, turtles should never ever lose their content now
* Fixed the "turtle_inventory" event firing when it shouldn't have
* Added error messages to many more turtle functions after they return false
* Documented all new programs and API changes in the "help" system

New Features in ComputerCraft 1.58:

* Fixed a long standing bug where turtles could lose their identify if they travel too far away
* Fixed use of deprecated code, ensuring mod compatibility with the latest versions of Minecraft Forge, and world compatibility with future versions of Minecraft

New Features in ComputerCraft 1.57:

* Ported to Minecraft 1.6.4
* Added two new Treasure Disks: Conway's Game of Life by vilsol and Protector by fredthead
* Fixed a very nasty item duplication bug

New Features in ComputerCraft 1.56:

* Added Treasure Disks: Floppy Disks in dungeons which contain interesting community made programs. Find them all!
* All turtle functions now return additional error messages when they fail.
* Resource Packs with Lua Programs can now be edited when extracted to a folder, for easier editing.

New Features in ComputerCraft 1.55:

* Ported to Minecraft 1.6.2
* Added Advanced Turtles
* Added "turtle_inventory" event. Fires when any change is made to the inventory of a turtle
* Added missing functions io.close, io.flush, io.input, io.lines, io.output
* Tweaked the screen colours used by Advanced Computers, Monitors and Turtles
* Added new features for Peripheral authors
* Lua programs can now be included in Resource Packs

New Features in ComputerCraft 1.52:

* Ported to Minecraft 1.5.1

New Features in ComputerCraft 1.51:

* Ported to Minecraft 1.5
* Added Wired Modems
* Added Networking Cables
* Made Wireless Modems more expensive to craft
* New redstone API functions: getAnalogInput(), setAnalogOutput(), getAnalogOutput()
* Peripherals can now be controlled remotely over wired networks. New peripheral API function: getNames()
* New event: "monitor_resize" when the size of a monitor changes
* Except for labelled computers and turtles, ComputerCraft blocks no longer drop items in creative mode
* The pick block function works in creative mode now works for all ComputerCraft blocks
* All blocks and items now use the IDs numbers assigned by FTB by default
* Fixed turtles sometimes placing blocks with incorrect orientations
* Fixed Wireless modems being able to send messages to themselves
* Fixed turtle.attack() having a very short range
* Various bugfixes

New Features in ComputerCraft 1.5:

* Redesigned Wireless Modems; they can now send and receive on multiple channels, independent of the computer ID. To use these features, interface with modem peripherals directly. The rednet API still functions as before
* Floppy Disks can now be dyed with multiple dyes, just like armour
* The "excavate" program now retains fuel in it's inventory, so can run unattended
* turtle.place() now tries all possible block orientations before failing
* turtle.refuel(0) returns true if a fuel item is selected
* turtle.craft(0) returns true if the inventory is a valid recipe
* The in-game help system now has documentation for all the peripherals and their methods, including the new modem functionality
* A romantic surprise

New Features in ComputerCraft 1.48:

* Ported to Minecraft 1.4.6
* Advanced Monitors now emit a "monitor_touch" event when right clicked
* Advanced Monitors are now cheaper to craft
* Turtles now get slightly less fuel from items
* Computers can now interact with Command Blocks (if enabled in ComputerCraft.cfg)
* New API function: os.day()
* A christmas surprise

New Features in ComputerCraft 1.45:

* Added Advanced Computers
* Added Advanced Monitors
* New program: paint by nitrogenfingers
* New API: paintutils
* New term functions: term.setBackgroundColor, term.setTextColor, term.isColor
* New turtle function: turtle.transferTo

New Features in ComputerCraft 1.43:

* Added Printed Pages
* Added Printed Books
* Fixed incompatibility with Forge 275 and above
* Labelled Turtles now keep their fuel when broken

New Features in ComputerCraft 1.42:

* Ported to Minecraft 1.3.2
* Added Printers
* Floppy Disks can be dyed different colours
* Wireless Crafty Turtles can now be crafted
* New textures
* New forge config file
* Bug fixes

New Features in ComputerCraft 1.4:

* Ported to Forge Mod Loader. ComputerCraft can now be ran directly from the .zip without extraction
* Added Farming Turtles
* Added Felling Turtles
* Added Digging Turtles
* Added Melee Turtles
* Added Crafty Turtles
* Added 14 new Turtle Combinations accessible by combining the turtle upgrades above
* Labelled computers and turtles can now be crafted into turtles or other turtle types without losing their ID, label and data
* Added a "Turtle Upgrade API" for mod developers to create their own tools and peripherals for turtles
* Turtles can now attack entities with turtle.attack(), and collect their dropped items
* Turtles can now use turtle.place() with any item the player can, and can interact with entities
* Turtles can now craft items with turtle.craft()
* Turtles can now place items into inventories with turtle.drop()
* Changed the behaviour of turtle.place() and turtle.drop() to only consider the currently selected slot
* Turtles can now pick up items from the ground, or from inventories, with turtle.suck()
* Turtles can now compare items in their inventories
* Turtles can place signs with text on them with turtle.place( [signText] )
* Turtles now optionally require fuel items to move, and can refuel themselves
* The size of the the turtle inventory has been increased to 16
* The size of the turtle screen has been increased
* New turtle functions: turtle.compareTo( [slotNum] ), turtle.craft(), turtle.attack(), turtle.attackUp(), turtle.attackDown(), turtle.dropUp(), turtle.dropDown(), turtle.getFuelLevel(), turtle.refuel()
* New disk function: disk.getID()
* New turtle programs: craft, refuel
* "excavate" program now much smarter: Will return items to a chest when full, attack mobs, and refuel itself automatically
* New API: keys
* Added optional Floppy Disk and Hard Drive space limits for computers and turtles
* New fs function: fs.getFreeSpace( path ), also fs.getDrive() works again
* The send and receive range of wireless modems now increases with altitude, allowing long range networking from high-altitude computers (great for GPS networks)
* http.request() now supports https:// URLs
* Right clicking a Disk Drive with a Floppy Disk or a Record when sneaking will insert the item into the Disk Drive automatically
* The default size of the computer screen has been increased
* Several stability and security fixes. LuaJ can now no longer leave dangling threads when a computer is unloaded, turtles can no longer be destroyed by tree leaves or walking off the edge of the loaded map. Computers no longer crash when used with RedPower frames.

New Features in ComputerCraft 1.31:

* Ported to Minecraft 1.2.3
* Added Monitors (thanks to Cloudy)
* Updated LuaJ to a newer, less memory hungry version
* rednet_message event now has a third parameter, "distance", to support position triangulation.
* New programs: gps, monitor, pastebin.
* Added a secret program. Use with large monitors!
* New apis: gps, vector
* New turtle functions: turtle.compare(), turtle.compareUp(), turtle.compareDown(), turtle.drop( quantity )
* New http functions: http.post().
* New term functions: term.redirect(), term.restore()
* New textutils functions: textutils.urlEncode()
* New rednet functions: rednet.isOpen()
* New config options: modem_range, modem_rangeDuringStorm
* Bug fixes, program tweaks, and help updates

New Features in ComputerCraft 1.3:

* Ported to Minecraft Forge
* Added Turtles
* Added Wireless Modems
* Added Mining Turtles
* Added Wireless Turtles
* Added Wireless Mining Turtles
* Computers and Disk Drives no longer get destroyed by water.
* Computers and Turtles can now be labelled with the label program, and labelled devices keep their state when destroyed.
* Computers/Turtles can connect to adjacent devices, and turn them on and off
* User programs now give line numbers in their error messages
* New APIs: turtle, peripheral
* New programs for turtles: tunnel, excavate, go, turn, dance
* New os functions: os.getComputerLabel(), os.setComputerLabel()
* Added "filter" parameter to os.pullEvent()
* New shell function: shell.getCurrentProgram()
* New textutils functions: textutils.serialize(), textutils.unserialize(), textutils.tabulate(), textutils.pagedTabulate(), textutils.slowWrite()
* New io file function: file:lines()
* New fs function: fs.getSize()
* Disk Drives can now play records from other mods
* Bug fixes, program tweaks, and help updates

New Features in ComputerCraft 1.2:

* Added Disk Drives and Floppy Disks
* Added Ctrl+T shortcut to terminate the current program (hold)
* Added Ctrl+S shortcut to shutdown the computer (hold)
* Added Ctrl+R shortcut to reboot the computer (hold)
* New Programs: alias, apis, copy, delete, dj, drive, eject, id, label, list, move, reboot, redset, rename, time, worm.
* New APIs: bit, colours, disk, help, rednet, parallel, textutils.
* New color functions: colors.combine(), colors.subtract(), colors.test()
* New fs functions: fs.getName(), new modes for fs.open()
* New os functions: os.loadAPI(), os.unloadAPI(),
os.clock(), os.time(), os.setAlarm(),
os.reboot(), os.queueEvent()
* New redstone function: redstone.getSides()
* New shell functions: shell.setPath(), shell.programs(), shell.resolveProgram(), shell.setAlias()
* Lots of updates to the help pages
* Bug fixes

New Features in ComputerCraft 1.1:

* Added Multiplayer support throughout.
* Added connectivity with RedPower bundled cables
* Added HTTP api, enabled via the mod config, to allow computers to access the real world internet
* Added command history to the shell.
* Programs which spin in an infinite loop without yielding will no longer freeze minecraft
* Help updates and bug fixes

New Features in ComputerCraft 1.0:

* First Release!
