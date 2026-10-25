-- SPDX-FileCopyrightText: 2019 JackMacWindows
--
-- SPDX-License-Identifier: MPL-2.0

print("The debug adapter is running. Please do not close this window.")
term.setCursorBlink(false)
debugger.continue()

local ok, err = pcall(function()

local nextSequence = 1

local function sendMessage(message, headers)
    message.seq = nextSequence
    nextSequence = nextSequence + 1
    local data = textutils.serializeJSON(message)
    local packet = "Content-Length: " .. #data .. "\r\n"
    if headers then for k, v in pairs(headers) do packet = packet .. k .. ": " .. v .. "\r\n" end end
    packet = packet .. "\r\n" .. data
    print(packet)
    debugger.sendDAPData(packet)
end

local function copy(t)
    if type(t) == "table" then
        local r = {}
        for k, v in pairs(t) do r[k] = copy(v) end
        return r
    else return t end
end

local responseWait = {}
local initConfig = {}
local pause = false
local launchCommand
local variableRefs = {}

local reasonMap = {
    ["debug.debug() called"] = "breakpoint",
    ["Pause"] = "pause",
    ["Breakpoint"] = "breakpoint",
    ["Function breakpoint"] = "function breakpoint",
    ["Error"] = "exception",
    ["Resume"] = "exception",
    ["Yield"] = "exception",
    ["Caught call"] = "exception",
}

local commands, events = {}, {}

function commands.initialize(args)
    initConfig = args
    return {
        supportsConfigurationDoneRequest = true,
        supportsFunctionBreakpoints = true,
        supportsConditionalBreakpoints = false, -- TODO
        supportsHitConditionalBreakpoints = false, -- TODO
        supportsEvaluateForHovers = true,
        exceptionBreakpointFilters = {
            {
                filter = "error",
                label = "Any error",
                description = "Breaks on any thrown error"
            },
            {
                filter = "load",
                label = "Load code",
                description = "Breaks when calling loadfile, loadAPI, require"
            },
            {
                filter = "run",
                label = "Run program",
                description = "Breaks when calling os.run, shell.run, dofile"
            },
            {
                filter = "resume",
                label = "Resume coroutine",
                description = "Breaks when resuming any coroutine"
            },
            {
                filter = "yield",
                label = "Yield coroutine",
                description = "Breaks when any coroutine yields"
            }
        },
        supportsStepBack = false,
        supportsSetVariable = true,
        supportsGotoTargetsRequest = false,
        supportsStepInTargetsRequest = false,
        supportsCompletionsRequest = false,
        supportsModulesRequest = false,
        --supportedChecksumAlgorithms = {"timestamp"},
        supportsRestartRequest = true,
        supportsExceptionOptions = false,
        supportsValueFormattingOptions = true,
        supportsExceptionInfoRequest = true,
        supportTerminateDebuggee = true,
        supportSuspendDebuggee = true,
        supportsDelayedStackTraceLoading = true,
        supportsLoadedSourcesRequest = false,
        supportsLogPoints = false, -- TODO
        supportsTerminateThreadsRequest = false,
        supportsSetExpression = true,
        supportsTerminateRequest = true,
        supportsDataBreakpoints = false,
        supportsReadMemoryRequest = false,
        supportsWriteMemoryRequest = false,
        supportsDisassembleRequest = true,
        supportsCancelRequest = false,
        supportsBreakpointLocationsRequest = false,
        supportsClipboardContext = false,
        supportsSteppingGranularity = false,
        supportsInstructionBreakpoints = false,
        supportsExceptionFilterOptions = false,
        supportsSingleThreadExecutionRequests = false,
    }
end

function commands.launch(args)
    launchCommand = args.program
    pause = true
    if not debugger.status() then
        debugger.step()
        debugger.unblock()
        print("Unblocked")
        debugger.waitForBreak()
        print("Done")
    end
    if launchCommand then debugger.setStartupCode("shell.run('" .. launchCommand .. "')") end
    debugger.run("coroutine.resume(coroutine.create(os.reboot))")
    debugger.continue()
    print("Continuing")
    sleep(0.5)
    debugger.waitForBreakAsync()
end

function commands.attach()
    debugger.waitForBreakAsync()
end

function commands.restart(args)
    pause = true
    if not debugger.status() then
        debugger.step()
        debugger.unblock()
        debugger.waitForBreak()
    end
    if launchCommand then debugger.setStartupCode("shell.run('" .. launchCommand .. "')") end
    debugger.run("coroutine.resume(coroutine.create(os.reboot))")
    debugger.continue()
    sleep(0.5)
    debugger.waitForBreakAsync()
end

function commands.disconnect(args)
    if args.terminateDebuggee then
        pause = true
        if not debugger.status() then
            debugger.step()
            debugger.unblock()
            debugger.waitForBreak()
        end
        debugger.run("coroutine.resume(coroutine.create(os.shutdown))")
        debugger.continue()
        sleep(0.5)
        debugger.waitForBreakAsync()
    elseif args.suspendDebuggee then
        if not debugger.status() then
            debugger.step()
            debugger.unblock()
        end
    else
        if debugger.status() then
            debugger.continue()
            debugger.waitForBreakAsync()
        end
    end
end

function commands.terminate(args)
    pause = true
    if not debugger.status() then
        debugger.step()
        debugger.unblock()
        debugger.waitForBreak()
    end
    debugger.run("coroutine.resume(coroutine.create(os.shutdown))")
    debugger.continue()
    sleep(0.5)
    debugger.waitForBreakAsync()
end

function commands.setBreakpoints(args, message)
    if not args.source.adapterData then
        local path = debugger.getInternalPath(args.source.path)
        args.source.adapterData = {path = path and fs.combine(path)}
    end
    if not args.breakpoints or (args.source.adapterData and not args.source.adapterData.path) then
        local retval = {}
        for i, v in ipairs(args.breakpoints) do
            retval[#retval+1] = {id = i, verified = false, line = v.line}
        end
        if #retval == 0 then retval = textutils.empty_json_array end
        return {breakpoints = retval}
    end
    local bp = debugger.listBreakpoints()
    for i, v in ipairs(bp) do if fs.combine(v.file:sub(2)) == args.source.adapterData.path then debugger.unsetBreakpoint(i) end end
    local retval = {}
    for _, v in ipairs(args.breakpoints) do
        local id = debugger.setBreakpoint(args.source.adapterData.path, v.line)
        retval[#retval+1] = {id = id, verified = true, source = copy(args.source), line = v.line}
    end
    if #retval == 0 then retval = textutils.empty_json_array end
    return {breakpoints = retval}
end

function commands.setFunctionBreakpoints(args)
    local bp = debugger.listBreakpoints()
    for i, v in ipairs(bp) do if v.line == -1 then debugger.unsetBreakpoint(i) end end
    local retval = {}
    for _, v in ipairs(args.breakpoints) do
        local id = debugger.setFunctionBreakpoint(v.name)
        retval[#retval+1] = {id = id, verified = true}
    end
    if #retval == 0 then retval = textutils.empty_json_array end
    return {breakpoints = retval}
end

function commands.setExceptionBreakpoints(args)
    debugger.uncatch("error")
    debugger.uncatch("load")
    debugger.uncatch("run")
    debugger.uncatch("resume")
    debugger.uncatch("yield")
    local retval = {}
    for _, v in ipairs(args.filters) do
        debugger.catch(v)
        retval[#retval+1] = {verified = true}
    end
    if #retval == 0 then retval = textutils.empty_json_array end
    return {breakpoints = retval}
end

function commands.continue(args)
    if debugger.status() then
        debugger.continue()
        debugger.waitForBreakAsync()
    end
    return {allThreadsContinued = true}
end

function commands.next(args)
    if debugger.status() then
        debugger.step()
        debugger.waitForBreakAsync()
    end
end

function commands.stepIn(args)
    if debugger.status() then
        debugger.step()
        debugger.waitForBreakAsync()
    end
end

function commands.stepOut(args)
    if debugger.status() then
        debugger.stepOut()
        debugger.waitForBreakAsync()
    end
end

function commands.pause(args)
    if not debugger.status() then
        debugger.step()
        debugger.unblock()
    end
end

function commands.stackTrace(args)
    local stack = {}
    local total = 0
    while debugger.getInfo(total) do total = total + 1 end
    for i = args.startFrame or 0, (args.startFrame or 1) + (args.levels or math.huge) - 1 do
        local info = debugger.getInfo(i)
        if not info then break end
        local source = {name = info.short_src, origin = info.what}
        if info.source:match("^@") then
            source.path = debugger.getPath(info.source:sub(2))
            source.presentationHint = "normal"
            source.adapterData = {path = info.source:sub(2)}
        elseif info.source:match("^=") then
            source.presentationHint = "deemphasize"
            source.adapterData = {}
        else
            source.adapterData = {data = info.source}
        end
        stack[#stack+1] = {
            id = i,
            name = info.name,
            source = source,
            line = info.currentline ~= -1 and (info.currentline and info.currentline + 1) or ((info.what == "Lua" or info.what == "main") and 1 or -1),
            column = 0,
            instructionPointerReference = info.instruction >= 0 and tostring(i * 0x100000000 + info.instruction) or nil,
        }
    end
    if #stack == 0 then stack = textutils.empty_json_array end
    return {
        stackFrames = stack,
        totalFrames = total
    }
end

function commands.scopes(args)
    if not debugger.status() then return {scopes = textutils.empty_json_array} end
    local info = debugger.getInfo(args.frameId)
    local locals = debugger.getLocals(args.frameId)
    local n = 1
    for _ in pairs(locals) do n = n + 1 end
    local source = {name = info.short_src, origin = info.what}
    if info.source:match("^@") then
        source.path = debugger.getPath(info.source:sub(2))
        source.presentationHint = "normal"
        source.adapterData = {path = info.source:sub(2)}
    elseif info.source:match("^=") then
        source.presentationHint = "deemphasize"
        source.adapterData = {}
    else
        source.adapterData = {data = info.source}
    end
    -- oof
    local source2 = {}
    for k, v in pairs(source) do source2[k] = v end
    source2.adapterData = {path = source.adapterData.path, data = source.adapterData.data}
    return {scopes = {
        {
            name = "Locals",
            presentationHint = "locals",
            variablesReference = 0x7FFFFF00 + args.frameId,
            --namedVariables = n,
            source = source,
            line = info.linedefined,
            endLine = info.lastlinedefined
        },
        {
            name = "Upvalues",
            presentationHint = "locals",
            variablesReference = 0x7FFFFF80 + args.frameId,
            --namedVariables = info.nups,
            source = source2,
            line = info.linedefined,
            endLine = info.lastlinedefined
        }
    }}
end

function commands.variables(args)
    if not debugger.status() then return {variables = textutils.empty_json_array} end
    local retval = {}
    if bit32.band(args.variablesReference, 0x7FFFFF80) == 0x7FFFFF00 then
        local locals = debugger.getLocals(bit32.band(args.variablesReference, 0x7F))
        for k, v in pairs(locals) do
            if args.format and args.format.hex and type(v) == "number" then v = ("%x"):format(v) end
            local id = 0
            if type(v) == "table" then
                id = #variableRefs + 1
                variableRefs[id] = v
            end
            retval[#retval+1] = {
                name = k,
                value = tostring(v),
                type = type(v),
                evaluateName = "locals." .. k,
                variablesReference = id
            }
        end
    elseif bit32.band(args.variablesReference, 0x7FFFFF80) == 0x7FFFFF80 then
        local upvals = debugger.getUpvalues(bit32.band(args.variablesReference, 0x7F))
        for k, v in pairs(upvals) do
            if args.format and args.format.hex and type(v) == "number" then v = ("%x"):format(v) end
            local id = 0
            if type(v) == "table" then
                id = #variableRefs + 1
                variableRefs[id] = v
            end
            retval[#retval+1] = {
                name = k,
                value = tostring(v),
                type = type(v),
                evaluateName = "upvalues." .. k,
                variablesReference = id
            }
        end
    elseif variableRefs[args.variablesReference] then
        for i = (args.start or 0) + 1, (args.start or 0) + (args.count or #variableRefs[args.variablesReference]) do
            local v = variableRefs[args.variablesReference][i]
            if args.format and args.format.hex and type(v) == "number" then v = ("%x"):format(v) end
            local id = 0
            if type(v) == "table" then
                id = #variableRefs + 1
                variableRefs[id] = v
            end
            retval[#retval+1] = {
                name = tostring(i),
                value = tostring(v),
                type = type(v),
                variablesReference = id
            }
        end
    end
    if #retval == 0 then retval = textutils.empty_json_array end
    return {variables = retval}
end

function commands.setVariable(args, message)
    if not debugger.status() then
        sendMessage {type = "response", request_seq = message.seq, success = false, command = message.command, error = "Not paused"}
        return false
    end
    if args.value == "nil" then args.value = nil
    elseif args.value == "true" then args.value = true
    elseif args.value == "false" then args.value = false
    else args.value = tonumber(args.value) or args.value end
    if bit32.band(args.variablesReference, 0x7FFFFF80) == 0x7FFFFF00 then
        debugger.setLocal(bit32.band(args.variablesReference, 0x7F), args.name, args.value)
    elseif bit32.band(args.variablesReference, 0x7FFFFF80) == 0x7FFFFF80 then
        debugger.setUpvalue(bit32.band(args.variablesReference, 0x7F), args.name, args.value)
    else
        variableRefs[args.variablesReference][args.name] = args.value
    end
    return {value = tostring(args.value), type = type(args.value)}
end

function commands.source(args, message)
    if not debugger.status() then
        sendMessage {type = "response", request_seq = message.seq, success = false, command = message.command, error = "Not paused"}
        return false
    end
    if not args.source.adapterData then
        local path = debugger.getInternalPath(args.source.path)
        args.source.adapterData = {path = path and fs.combine(path)}
    end
    if args.source.adapterData.path then
        local file, err = fs.open(args.source.adapterData.path, "r")
        if not file then
            sendMessage {type = "response", request_seq = message.seq, success = false, command = message.command, error = err}
            return false
        end
        local data = file.readAll()
        file.close()
        return {content = data}
    elseif args.source.adapterData.data then
        return {content = args.source.adapterData.data}
    else
        sendMessage {type = "response", request_seq = message.seq, success = false, command = message.command, error = "No source available"}
        return false
    end
end

function commands.threads(args)
    return {threads = {{id = 1, name = "Computer"}}}
end

function commands.evaluate(args, message)
    if not debugger.status() then
        sendMessage {type = "response", request_seq = message.seq, success = false, command = message.command, error = "Not paused"}
        return false
    end
    local sf, func, e = args.expression, load( args.expression, "lua", "t", {} )
    local sf2, func2, e2 = "return _echo("..args.expression..");", load( "return _echo("..args.expression..");", "lua", "t", {} )
    if not func then
        if func2 then
            func = func2
            sf = sf2
            e = nil
        end
    else
        if func2 then
            func = func2
            sf = sf2
        end
    end
    if not func then
        sendMessage {type = "response", request_seq = message.seq, success = false, command = message.command, error = e}
        return false
    end
    local res = table.pack(debugger.run(sf))
    print(table.unpack(res, 1, res.n))
    if not res[1] then
        sendMessage {type = "response", request_seq = message.seq, success = false, command = message.command, error = res[2]}
        return false
    end
    if res.n > 2 then
        local id = #variableRefs + 1
        for i = 1, res.n - 1 do res[i] = res[i+1] end
        res[res.n] = nil
        res.n = res.n - 1
        variableRefs[id] = res
        return {result = tostring(res[1]) .. ", ...", variablesReference = id, indexedVariables = res.n}
    elseif type(res[2]) == "table" then
        local id = #variableRefs + 1
        variableRefs[id] = res[2]
        return {result = tostring(res[2]), variablesReference = id}
    else return {result = tostring(res[2]), variablesReference = 0} end
end

function commands.setExpression(args, message)
    if not debugger.status() then
        sendMessage {type = "response", request_seq = message.seq, success = false, command = message.command, error = "Not paused"}
        return false
    end
    local ok, err = debugger.run(args.expression .. " = " .. args.value)
    if not ok then
        sendMessage {type = "response", request_seq = message.seq, success = false, command = message.command, error = err}
        return false
    end
    local _, val = debugger.run("return _echo(" .. args.expression .. ")")
    if type(val) == "table" then
        local id = #variableRefs + 1
        variableRefs[id] = val
        return {value = tostring(val), type = "table", variablesReference = id}
    else return {value = tostring(val), type = type(val)} end
end

function commands.exceptionInfo(args)
    return {
        exceptionId = debugger.getReason(),
        breakMode = "always",
        details = debugger.getReason(),
    }
end

-- Full Lua bytecode loader! Yay! :D:
-- NOTE: This code MUST be updated when switching to Lua 5.2/5.4!!!

local function LoadChar(S)
    local x
    x, S.pos = ("B"):unpack(S.str, S.pos)
    return x
end

local function LoadInt(S)
    local x
    x, S.pos = ("I4"):unpack(S.str, S.pos)
    return x
end

local function LoadSInt(S)
    local x
    x, S.pos = ("i4"):unpack(S.str, S.pos)
    return x
end

local function LoadNumber(S)
    local x
    x, S.pos = ("d"):unpack(S.str, S.pos)
    return x
end

local function LoadString(S)
    local size = LoadInt(S)
    if size == 0 then return nil
    else
        local s = S.str:sub(S.pos, S.pos + size - 2)
        S.pos = S.pos + size
        return s
    end
end

local function LoadCode(S, f)
    f.code = {}
    local n = LoadInt(S)
    for i = 1, n do f.code[i] = LoadInt(S) end
end

local LoadFunction

local function LoadConstants(S, f)
    local n = LoadInt(S)
    f.k = {}
    for i = 0, n-1 do
        local t = LoadChar(S)
        if t == 0 then f.k[i] = nil
        elseif t == 1 then f.k[i] = LoadChar(S) ~= 0
        elseif t == 3 then f.k[i] = LoadNumber(S)
        elseif t == 4 then f.k[i] = LoadString(S)
        else error("bad constant") end
    end
    n = LoadInt(S)
    f.p = {}
    for i = 0, n-1 do f.p[i] = LoadFunction(S, f.source) end
end

local function LoadDebug(S, f)
    local n = LoadInt(S)
    f.lineinfo = {}
    for i = 1, n do f.lineinfo[i] = LoadSInt(S) end
    n = LoadInt(S)
    f.locvars = {}
    for i = 0, n-1 do
        f.locvars[i] = {}
        f.locvars[i].varname = LoadString(S)
        f.locvars[i].startpc = LoadInt(S)
        f.locvars[i].endpc = LoadInt(S)
    end
    n = LoadInt(S)
    f.upvalues = {}
    for i = 0, n-1 do f.upvalues[i] = LoadString(S) end
end

function LoadFunction(S, p)
    local f = {}
    f.source = LoadString(S) or p
    f.linedefined = LoadInt(S)
    f.lastlinedefined = LoadInt(S)
    f.nups = LoadChar(S)
    f.numparams = LoadChar(S)
    f.is_vararg = LoadChar(S) == 1
    f.maxstacksize = LoadChar(S)
    LoadCode(S, f)
    LoadConstants(S, f)
    LoadDebug(S, f)
    return f
end

local function PrintString(str)
    return ("%q"):format(str)
end

local function PrintConstant(f, i)
    if type(f.k[i]) == "string" then return PrintString(f.k[i])
    else return tostring(f.k[i]) end
end

local opnames = {
    [0] = "MOVE",
    "LOADK",
    "LOADBOOL",
    "LOADNIL",
    "GETUPVAL",
    "GETGLOBAL",
    "GETTABLE",
    "SETGLOBAL",
    "SETUPVAL",
    "SETTABLE",
    "NEWTABLE",
    "SELF",
    "ADD",
    "SUB",
    "MUL",
    "DIV",
    "MOD",
    "POW",
    "UNM",
    "NOT",
    "LEN",
    "CONCAT",
    "JMP",
    "EQ",
    "LT",
    "LE",
    "TEST",
    "TESTSET",
    "CALL",
    "TAILCALL",
    "RETURN",
    "FORLOOP",
    "FORPREP",
    "TFORLOOP",
    "SETLIST",
    "CLOSE",
    "CLOSURE",
    "VARARG"
}

local OpArgN, OpArgU, OpArgR, OpArgK = 0, 1, 2, 3
local iABC, iABx, iAsBx = 0, 1, 2
local function opmode(t, a, b, c, m) return {t = t == 1, a = a == 1, b = b, c = c, mode = m} end

local opmodes = {
    [0] = opmode(0, 1, OpArgR, OpArgN, iABC)
    ,opmode(0, 1, OpArgK, OpArgN, iABx)
    ,opmode(0, 1, OpArgU, OpArgU, iABC)
    ,opmode(0, 1, OpArgR, OpArgN, iABC)
    ,opmode(0, 1, OpArgU, OpArgN, iABC)
    ,opmode(0, 1, OpArgK, OpArgN, iABx)
    ,opmode(0, 1, OpArgR, OpArgK, iABC)
    ,opmode(0, 0, OpArgK, OpArgN, iABx)
    ,opmode(0, 0, OpArgU, OpArgN, iABC)
    ,opmode(0, 0, OpArgK, OpArgK, iABC)
    ,opmode(0, 1, OpArgU, OpArgU, iABC)
    ,opmode(0, 1, OpArgR, OpArgK, iABC)
    ,opmode(0, 1, OpArgK, OpArgK, iABC)
    ,opmode(0, 1, OpArgK, OpArgK, iABC)
    ,opmode(0, 1, OpArgK, OpArgK, iABC)
    ,opmode(0, 1, OpArgK, OpArgK, iABC)
    ,opmode(0, 1, OpArgK, OpArgK, iABC)
    ,opmode(0, 1, OpArgK, OpArgK, iABC)
    ,opmode(0, 1, OpArgR, OpArgN, iABC)
    ,opmode(0, 1, OpArgR, OpArgN, iABC)
    ,opmode(0, 1, OpArgR, OpArgN, iABC)
    ,opmode(0, 1, OpArgR, OpArgR, iABC)
    ,opmode(0, 0, OpArgR, OpArgN, iAsBx)
    ,opmode(1, 0, OpArgK, OpArgK, iABC)
    ,opmode(1, 0, OpArgK, OpArgK, iABC)
    ,opmode(1, 0, OpArgK, OpArgK, iABC)
    ,opmode(1, 1, OpArgR, OpArgU, iABC)
    ,opmode(1, 1, OpArgR, OpArgU, iABC)
    ,opmode(0, 1, OpArgU, OpArgU, iABC)
    ,opmode(0, 1, OpArgU, OpArgU, iABC)
    ,opmode(0, 0, OpArgU, OpArgN, iABC)
    ,opmode(0, 1, OpArgR, OpArgN, iAsBx)
    ,opmode(0, 1, OpArgR, OpArgN, iAsBx)
    ,opmode(1, 0, OpArgN, OpArgU, iABC)
    ,opmode(0, 0, OpArgU, OpArgU, iABC)
    ,opmode(0, 0, OpArgN, OpArgN, iABC)
    ,opmode(0, 1, OpArgU, OpArgN, iABx)
    ,opmode(0, 1, OpArgU, OpArgN, iABC)
}

local function PrintCode(f, src)
    local list = {}
    local pc = 1
    while pc <= #f.code do
        local i = f.code[pc]
        local o = bit32.band(i, 0x3F)
        local a = bit32.band(bit32.rshift(i, 6), 0xFF)
        local b = bit32.band(bit32.rshift(i, 14), 0x1FF)
        local c = bit32.band(bit32.rshift(i, 23), 0x1FF)
        local bx = bit32.band(bit32.rshift(i, 14), 0x3FFFF)
        local sbx = bx - 0x1FFFF
        local inst = {source = copy(src), instructionBytes = ("%08X"):format(i), address = tostring(pc), line = f.lineinfo[pc]}
        local retval = ("%-9s\t"):format(opnames[o])
        local mode = opmodes[o]
        if mode.mode == iABC then
            retval = retval .. a
            if mode.b ~= OpArgN then retval = retval .. " " .. (bit32.btest(b, 0x100) and (-1-bit32.band(b, 0xFF)) or b) end
            if mode.c ~= OpArgN then retval = retval .. " " .. (bit32.btest(c, 0x100) and (-1-bit32.band(c, 0xFF)) or c) end
        elseif mode.mode == iABx then
            if mode.b == OpArgK then retval = retval .. a .. " " .. (-1-bx)
            else retval = retval .. a .. " " .. bx end
        else
            if o == 22 then retval = retval .. sbx
            else retval = retval .. a .. " " .. sbx end
        end
        if o == 1 then retval = retval .. "\t; " .. PrintConstant(f, bx)
        elseif o == 4 or o == 8 then retval = retval .. "\t; " .. (f.upvalues[b] or "-")
        elseif o == 5 or o == 7 then retval = retval .. "\t; " .. f.k[bx]
        elseif o == 6 or o == 11 then if bit32.btest(c, 0x100) then retval = retval .. "\t; " .. PrintConstant(f, bit32.band(c, 0xFF)) end
        elseif o == 9 or o == 12 or o == 13 or o == 14 or o == 15 or o == 17 or o == 23 or o == 24 or o == 25 then
            if bit32.btest(b, 0x100) or bit32.btest(c, 0x100) then
                retval = retval .. "\t; "
                if bit32.btest(b, 0x100) then retval = retval .. PrintConstant(f, bit32.band(b, 0xFF))
                else retval = retval .. "-" end
                retval = retval .. " "
                if bit32.btest(c, 0x100) then retval = retval .. PrintConstant(f, bit32.band(c, 0xFF))
                else retval = retval .. "-" end
            end
        elseif o == 22 or o == 31 or o == 32 then retval = retval .. "\t; to " .. (sbx+pc+2)
        elseif o == 36 then retval = retval .. "\t; " .. tostring(f.p[bx]):gsub("table: ", "")
        elseif o == 34 then
            if c == 0 then
                pc=pc+1
                retval = retval .. "\t; " .. f.code[pc]
            else retval = retval .. "\t; " .. c end
        end
        inst.instruction = retval
        list[pc] = inst
        pc=pc+1
    end
    return list
end

function commands.disassemble(args)
    local pc = bit32.band(tonumber(args.memoryReference), 0xFFFFFFFF)
    local level = math.floor(tonumber(args.memoryReference) / 0x100000000)
    local ok, code, info = debugger.run("local info = debug.getinfo(" .. (level + 2) .. ", 'Sf') local code = string.dump(info.func) info.func = nil return code, info")
    if not ok then return {instructions = textutils.empty_json_array, error = code} end
    local source = {name = info.short_src, origin = info.what}
    if info.source:match("^@") then
        source.path = debugger.getPath(info.source:sub(2))
        source.presentationHint = "normal"
        source.adapterData = {path = info.source:sub(2)}
    elseif info.source:match("^=") then
        source.presentationHint = "deemphasize"
        source.adapterData = {}
    else
        source.adapterData = {data = info.source}
    end
    local f = LoadFunction({str = code, pos = 13}, "?")
    local insts = PrintCode(f, source)
    local retval = {}
    local start = tonumber(pc) + (args.instructionOffset or 0)
    for i = start, start + args.instructionCount - 1 do
        retval[#retval+1] = insts[i] or {address = tonumber(i), instruction = ""}
    end
    if #retval == 0 then retval = textutils.empty_json_array end
    return {instructions = retval}
end

parallel.waitForAny(function()
    local buffer = ""
    while true do
        local _, input = coroutine.yield("dap_input")
        print(input)
        buffer = buffer .. input
        while buffer:match "\r\n\r\n" do
            print("Parsing")
            local headers = {}
            while true do
                local stop = buffer:find("\r\n")
                local line = buffer:sub(1, stop - 1)
                buffer = buffer:sub(stop + 2)
                if line == "" then break end
                headers[line:match("^[^:]+")] = line:match(":%s*(.*)$")
            end
            if headers["Content-Length"] then
                local length = tonumber(headers["Content-Length"])
                while #buffer < length do
                    _, input = coroutine.yield("dap_input")
                    print(input)
                    buffer = buffer .. input
                end
                local data = buffer:sub(1, length)
                buffer = buffer:sub(length + 1)
                local message = textutils.unserializeJSON(data)
                nextSequence = message.seq + 1
                print(message.seq, message.type)
                if message.type == "request" then
                    local body
                    if commands[message.command] then body = commands[message.command](message.arguments, message) end
                    if body ~= false then sendMessage {type = "response", request_seq = message.seq, success = true, command = message.command, body = body} end
                    if message.command == "initialize" then sendMessage {type = "event", event = "initialized"}
                    elseif message.command == "launch" or message.command == "attach" then
                        sendMessage {type = "event", event = "process", body = {name = "CraftOS-PC", isLocalProcess = false, startMethod = message.command}}
                        sendMessage {type = "event", event = "thread", body = {reason = "started", threadId = 1}}
                    end
                elseif message.type == "response" then if responseWait[message.request_seq] then responseWait[message.request_seq](message) end
                elseif message.type == "event" then if events[message.event] then events[message.event](message.body, message) end end
                print("Finished command")
            end
        end
    end
end, function()
    --debugger.waitForBreakAsync()
    sleep(0.25) -- clear queue
    while true do
        coroutine.yield("debugger_break")
        debugger.confirmBreak()
        print("Did break")
        if not pause then
            print("Sending message")
            sendMessage {
                type = "event",
                event = "stopped",
                body = {
                    reason = reasonMap[debugger.getReason()] or "exception",
                    description = debugger.getReason(),
                    text = debugger.getReason(),
                    threadId = 1,
                    allThreadsStopped = true,
                    -- TODO: hitBreakpointIds
                }
            }
        else
            sleep(0.25)
            pause = false
        end
    end
end, function()
    while true do
        local _, text = coroutine.yield("debugger_print")
        sendMessage {
            event = "output",
            body = {
                category = "console",
                output = text
            }
        }
    end
end)

end)
if not ok then printError(err) end
debugger.continue()
while true do coroutine.yield() end
