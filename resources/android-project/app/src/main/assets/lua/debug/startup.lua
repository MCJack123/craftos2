-- SPDX-FileCopyrightText: 2019 JackMacWindows
--
-- SPDX-License-Identifier: MPL-2.0

if debugger.useDAP and debugger.useDAP() then
    shell.run("debug/adapter.lua")
else
    shell.openTab("debug/showfile.lua")
    shell.openTab("debug/profiler.lua")
    shell.openTab("debug/console.lua")
    shell.run("debug/debugger.lua")
end
shell.exit()