# New feature documentation
This file provides documentation for the new APIs in CraftOS-PC.

## `periphemu`
Creates and removes peripherals from the registry.
### Functions
* *boolean* create(*string* side, *string* type\[, *string* path\]): Creates a new peripheral.
  * side: The side of the new peripheral
  * type: One of `monitor`, `speaker`, `printer`
  * path: If creating a printer, the local path to the output file
  * Returns: `true` on success, `false` on failure (already exists)
* *boolean* remove(*string* side): Removes a peripheral.
  * side: The side to remove
  * Returns: `true` on success, `false` on failure (already removed)
  
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
  * mode: `true` for graphics, `false` for text
* *boolean* getGraphicsMode(): Returns the current graphics mode setting.
* *nil* setPixel(*number* x, *number* y, *color* color): Sets a pixel at a location.
  * x: The X coordinate of the pixel
  * y: The Y coordinate of the pixel
  * color: The color of the pixel
* *color* getPixel(*number* x, *number* y): Returns the color of a pixel at a location.
  * x: The X coordinate of the pixel
  * y: The Y coordinate of the pixel
  * Returns: The color of the pixel

## `font`
Draws bitmap character fonts in graphics mode. Implements `write`, `getCursorPos`, `setCursorPos`, and `blit` from term.

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
