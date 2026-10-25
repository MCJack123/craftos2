-- SPDX-FileCopyrightText: 2019 JackMacWindows
--
-- SPDX-License-Identifier: MPL-2.0

multishell.setTitle(multishell.getCurrent(), "Debugger")
local ok, err = pcall(function()
local pretty = require "cc.pretty"
local history = {}
local function split(inputstr, sep)
    sep = sep or "%s"
    local t = {}
    for str in string.gmatch(inputstr, "([^"..sep.."]+)") do table.insert(t, str) end
    return t
end
term.setTextColor(colors.yellow)
print("CraftOS-PC Debugger")
local advanceTemp
while true do
    debugger.waitForBreak()
    if advanceTemp then debugger.unsetBreakpoint(advanceTemp); advanceTemp = nil end
    local info = debugger.getInfo()
    if string.sub(info.source, -8) == "bios.lua" then info.source = "@/bios.lua" end
    term.setTextColor(colors.blue)
    print("Break at " .. (info.short_src or "?") .. ":" .. (info.currentline or "?") .. " (" .. (info.name or "?") .. "): " .. debugger.getReason())
    if info.source and info.currentline and fs.exists(string.sub(info.source, 2)) then
        local file = fs.open(string.sub(info.source, 2), "r")
        for i = 1, info.currentline - 1 do file.readLine() end
        term.setTextColor(colors.lime)
        write("--> ")
        term.setTextColor(colors.white)
        local str = string.gsub(file.readLine(), "^[ \t]+", "")
        print(str)
        file.close()
    end
    local loop = true
    while loop do
        term.setTextColor(colors.yellow)
        write("(ccdb) ")
        term.setTextColor(colors.white)
        local cmd = read(nil, history)
        if cmd == "" then cmd = history[#history] 
        else table.insert(history, cmd) end
        local action = split(cmd)
        if action[1] == "step" or action[1] == "s" then debugger.step(action[2] and tonumber(action[2])); loop = false
        elseif action[1] == "finish" or action[1] == "fin" then debugger.stepOut(); loop = false
        elseif action[1] == "continue" or action[1] == "c" then debugger.continue(); loop = false
        elseif action[1] == "b" or action[1] == "break" then
            if action[2] == nil or not action[2]:match "[^:]+:%d+" then printError("Usage: break <source>:<line>")
            else print("Breakpoint " .. debugger.setBreakpoint(string.sub(action[2], 1, string.find(action[2], ":") - 1), tonumber(string.sub(action[2], string.find(action[2], ":") + 1))) .. " set at " .. string.sub(action[2], 1, string.find(action[2], ":") - 1) .. ":" .. string.sub(action[2], string.find(action[2], ":") + 1)) end
        elseif action[1] == "breakpoint" and action[2] == "set" then
            if action[3] == nil or not action[3]:match "[^:]+:%d+" then printError("Usage: break <source>:<line>")
            else print("Breakpoint " .. debugger.setBreakpoint(string.sub(action[3], 1, string.find(action[3], ":") - 1), tonumber(string.sub(action[3], string.find(action[3], ":") + 1))) .. " set at " .. string.sub(action[3], 1, string.find(action[3], ":") - 1) .. ":" .. string.sub(action[3], string.find(action[3], ":") + 1)) end
        elseif action[1] == "catch" then
            if action[2] == "catch" or action[2] == "error" or action[2] == "throw" then debugger.catch("error")
            elseif action[2] == "load" then debugger.catch("load")
            elseif action[2] == "exec" or action[2] == "run" then debugger.catch("run")
            elseif action[2] == "resume" then debugger.catch("resume")
            elseif action[2] == "yield" then debugger.catch("yield") end
        elseif action[1] == "clear" then debugger.unsetBreakpoint(tonumber(action[2]))
        elseif action[1] == "delete" then
            if action[2] == "catch" then
                if action[2] == "catch" or action[2] == "error" or action[2] == "throw" then debugger.uncatch("error")
                elseif action[2] == "load" then debugger.uncatch("load")
                elseif action[2] == "exec" or action[2] == "run" then debugger.uncatch("run")
                elseif action[2] == "resume" then debugger.uncatch("resume")
                elseif action[2] == "yield" then debugger.uncatch("yield") end
            else debugger.unsetBreakpoint(tonumber(action[2])) end
        elseif action[1] == "edit" and debugger.getInfo().source and fs.exists(string.sub(debugger.getInfo().source, 2)) then shell.run("edit", debugger.getInfo().source)
        elseif action[1] == "advance" then
            if action[2] == nil or not action[2]:match "[^:]+:%d+" then printError("Usage: advance <source>:<line>")
            else
                advanceTemp = debugger.setBreakpoint(string.sub(action[2], 1, string.find(action[2], ":") - 1), tonumber(string.sub(action[2], string.find(action[2], ":") + 1)))
                debugger.continue()
                loop = false
            end
        elseif action[1] == "info" then
            if action[2] == "breakpoints" then
                local breakpoints = debugger.listBreakpoints()
                local keys = {}
                for k,v in pairs(breakpoints) do table.insert(keys, k) end
                table.sort(keys)
                local lines = {}
                for _,i in ipairs(keys) do table.insert(lines, {i, breakpoints[i].file, breakpoints[i].line}) end
                textutils.tabulate(colors.blue, {"ID", "File", "Line"}, colors.white, table.unpack(lines))
            elseif action[2] == "frame" then
                term.setTextColor(colors.blue)
                print("Break at " .. (info.short_src or "?") .. ":" .. (info.currentline or "?") .. " (" .. (info.name or "?") .. "): " .. debugger.getReason())
                if info.source and info.currentline and fs.exists(string.sub(info.source, 2)) then
                    local file = fs.open(string.sub(info.source, 2), "r")
                    for i = 1, info.currentline - 1 do file.readLine() end
                    term.setTextColor(colors.lime)
                    write("--> ")
                    term.setTextColor(colors.white)
                    local str = string.gsub(file.readLine(), "^[ \t]+", "")
                    print(str)
                    file.close()
                end
            elseif action[2] == "locals" then
                local lines = {}
                for k,v in pairs(debugger.getLocals()) do table.insert(lines, {k, tostring(v)}) end
                textutils.tabulate(colors.blue, {"Name", "Value"}, colors.white, table.unpack(lines))
            else printError("Usage: info <breakpoints|frame|locals>") end
        elseif action[1] == "print" or action[1] == "p" then 
            table.remove(action, 1)
            local s = table.concat(action, " ")
            local nForcePrint = 0
            local sf, func, e = s, load( s, "lua", "t", {} )
            local sf2, func2, e2 = "return _echo("..s..");", load( "return _echo("..s..");", "lua", "t", {} )
            if not func then
                if func2 then
                    func = func2
                    sf = sf2
                    e = nil
                    nForcePrint = 1
                end
            else
                if func2 then
                    func = func2
                    sf = sf2
                end
            end
            if func then
                local tResults = table.pack( debugger.run( sf ) )
                if tResults[1] then
                    local n = 1
                    while n < tResults.n or (n <= nForcePrint) do
                        local value = tResults[ n + 1 ]
                        pretty.print(pretty.pretty(value))
                        n = n + 1
                    end
                else
                    printError( tResults[2] )
                end
            else
                printError( e )
            end
        --[[elseif action[1] == "x" then
            table.remove(action, 1)
            local s = table.concat(action, " ")
            local tEnv = setmetatable({_echo = function( ... ) return ... end}, {__index = _ENV})
            local nForcePrint = 0
            local func, e = load( s, "lua", "t", tEnv )
            local func2, e2 = load( "return _echo("..s..");", "lua", "t", tEnv )
            if not func then
                if func2 then
                    func = func2
                    e = nil
                    nForcePrint = 1
                end
            else
                if func2 then
                    func = func2
                end
            end
            if func then
                local tResults = table.pack( pcall( func ) )
                if tResults[1] then
                    local n = 1
                    while n < tResults.n or (n <= nForcePrint) do
                        local value = tResults[ n + 1 ]
                        if type( value ) == "table" then
                            local metatable = getmetatable( value )
                            if type(metatable) == "table" and type(metatable.__tostring) == "function" then
                                print( tostring( value ) )
                            else
                                local ok, serialised = pcall( textutils.serialise, value )
                                if ok then
                                    print( serialised )
                                else
                                    print( tostring( value ) )
                                end
                            end
                        else
                            print( tostring( value ) )
                        end
                        n = n + 1
                    end
                else
                    printError( tResults[2] )
                end
            else
                printError( e )
            end]]
        elseif action[1] == "backtrace" or action[1] == "bt" then print(({debugger.run("return debug.traceback()")})[2])
        elseif action[1] == "help" then
            textutils.pagedPrint([[Available commands:
advance -- Run to a position in a file in the format <file>:<line>
backtrace (bt) -- Show a traceback
break (b) -- Set a breakpoint in the format <file>:<line>
breakpoint set -- Set a breakpoint in the format <file>:<line>
catch -- Set a breakpoint on special calls
catch error -- Break on error
catch load -- Break on loading APIs/require
catch resume -- Break on resuming coroutine
catch run -- Break on running a program
catch yield -- Break on yielding coroutine
clear -- Clear a breakpoint
continue (c) -- Continue execution
edit -- Edit the currently running program
delete -- Clear a breakpoint
delete catch error -- Stop breaking on error
delete catch load -- Stop breaking on loading APIs/require
delete catch run -- Stop breaking on running a program
finish (fin) -- Step to the end of the current function
info -- List info about the running program
info breakpoints -- List all current breakpoints
info frame -- List the status of the program
info locals -- List all available locals
print (p) -- Run an expression and print the result a la lua.lua
step (s) -- Step a number of lines]], 4)
        else printError("Error: Invalid command") end
        lastaction = cmd
    end
    os.queueEvent("debugger_done")
end
end)
if not ok then printError(err) end
while os.pullEvent() do end
