-- SPDX-FileCopyrightText: 2019 JackMacWindows
--
-- SPDX-License-Identifier: MPL-2.0

if mounter == nil then error("Mounting directories is not supported in vanilla mode.") end
local args = { ... }
if args[1] ~= nil then if not mounter.unmount(args[1]) then printError("Could not unmount") end
else print("Usage: unmount <name>") end
