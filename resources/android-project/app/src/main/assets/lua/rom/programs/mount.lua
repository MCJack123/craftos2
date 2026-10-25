-- SPDX-FileCopyrightText: 2019 JackMacWindows
--
-- SPDX-License-Identifier: MPL-2.0

if mounter == nil then error("Mounting directories is not supported in vanilla mode.") end
local args = { ... }

if args[2] ~= nil then
    local ro = nil
    if args[3] == "readOnly" or args[3] == "true" then ro = true
    elseif args[3] == "false" then ro = false end
    if config.get("showMountPrompt") then print("A prompt will appear asking to confirm mounting. Press Allow to continue mounting.") end
    if not mounter.mount(args[1], args[2], ro) then printError("Could not mount") end
elseif args[1] == "--help" then
    term.setTextColor(colors.red)
    print("Usage: mount <name> <path> [readOnly]")
    print("       mount --help")
    print("       mount")
    term.setTextColor(colors.white)
else
    local mounts = mounter.list()
    print("/ on computer/" .. os.getComputerID())
    for k,v in pairs(mounts) do
        write("/" .. k .. " on " .. (#v == 1 and v[1] or "(\n  " .. table.concat(v, ",\n  ") .. "\n)"))
        if mounter.isReadOnly(k) then print(" (read-only)") else print() end
    end
end