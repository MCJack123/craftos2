-- SPDX-FileCopyrightText: 2019 JackMacWindows
--
-- SPDX-License-Identifier: MPL-2.0

if periphemu == nil then error("Attaching peripherals is not supported in vanilla mode.") end
local args = { ... }
if args[1] == "list" then
    print("Available peripheral types:")
    for _,p in ipairs(periphemu.names()) do print(p) end
elseif type(args[1]) ~= "string" or type(args[2]) ~= "string" then
    print("Usage: attach <side> <type> [options...]\n       attach list")
else 
    if peripheral.isPresent(args[1]) and peripheral.getType(args[1]) == args[2] then 
        print("Peripheral already attached")
        return
    end
    if tonumber(args[3]) ~= nil then args[3] = tonumber(args[3]) end
    local ok, err = periphemu.create(args[1], args[2], args[3])
    if not ok then printError("Could not attach peripheral" .. (err and ": " .. err or "")) end
end