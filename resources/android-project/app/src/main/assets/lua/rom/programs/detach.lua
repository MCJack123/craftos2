-- SPDX-FileCopyrightText: 2019 JackMacWindows
--
-- SPDX-License-Identifier: MPL-2.0

if periphemu == nil then error("Attaching peripherals is not supported in vanilla mode.") end
if type(({...})[1]) ~= "string" then print("Usage: detach <side>") else if not periphemu.remove(({...})[1]) then printError("Could not detach peripheral") end end