-- SPDX-FileCopyrightText: 2019 JackMacWindows
--
-- SPDX-License-Identifier: MPL-2.0

if config == nil then error("Accessing the configuration is not supported in vanilla mode.") end
local args = { ... }

if args[1] == "list" then
    local settings = config.list()
    for k,v in pairs(settings) do
        print(v .. " = " .. tostring(config.get(v)))
    end
elseif args[1] == "get" and args[2] ~= nil then
    local setting = config.get(args[2])
    if setting == nil then error("Unknown setting " .. args[2])
    elseif type(setting) == "table" then print(textutils.serialize(setting))
    else print(tostring(setting)) end
elseif args[1] == "set" and args[3] ~= nil then
    local type = config.getType(args[2])
    if args[2] == "mount_mode" and _G.type(args[3]) == "string" then type = "string" end
    local res
    if type == "boolean" then res = config.set(args[2], args[3] == "true" or args[3] == "1")
    elseif type == "string" then res = config.set(args[2], args[3])
    elseif type == "number" then res = config.set(args[2], tonumber(args[3]))
    else error("Unknown setting " .. args[2]) end
    if res then print(res) end
else
    term.setTextColor(colors.red)
    print("Usage: config <get|set|list> [setting] [value]")
    term.setTextColor(colors.white)
end