New Features in CraftOS-PC v2.8.3:

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

Type "help changelog" to see the full version history.
