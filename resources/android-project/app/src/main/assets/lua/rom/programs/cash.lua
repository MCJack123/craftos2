--[[
MIT License

Copyright (c) 2019 JackMacWindows

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
]]--

-- To silence IDE warnings
kernel = kernel
users = users
signal = signal

local topshell = _ENV.shell
local shell = {}
local multishell = {}
local pack = {}
_ENV.package = nil
_G.package = nil
package = nil
_ENV.require = nil
_G.require = nil
local start_time = os.epoch()
local args = {...}
local running = true
local shell_retval = 0
local shell_title = nil
local execCommand
local shell_env = _ENV
local pausedJob
local CCKernel2 = kernel and users and kernel.getPID
local OpusOS = kernel and kernel.hook

if table.maxn == nil then table.maxn = function(t) local i = 1 while t[i] ~= nil do i = i + 1 end return i - 1 end end

local function trim(s) return string.match(s, '^()%s*$') and '' or string.match(s, '^%s*(.*%S)') end

HOME = "/"
SHELL = topshell and topshell.getRunningProgram() or "/usr/bin/cash"
PATH = topshell and string.gsub(topshell.path(), "%.:", "") or "/rom/programs:/rom/programs/fun:/rom/programs/rednet"
USER = CCKernel2 and users.getShortName(users.getuid()) or "root"
EDITOR = "edit"
OLDPWD = topshell and topshell.dir() or "/"
PWD = topshell and topshell.dir() or "/"
SHLVL = SHLVL and SHLVL + 1 or 1
TERM = "craftos"
COLORTERM = "16color"

local vars = {
    PS1 = "\\s-\\v\\$ ",
    PS2 = "> ",
    IFS = "\n",
    CASH = topshell and topshell.getRunningProgram() or "cash.lua",
    CASH_VERSION = "0.3",
    RANDOM = function() return math.random(0, 32767) end,
    SECONDS = function() return math.floor((os.epoch() - start_time) / 1000) end,
    HOSTNAME = os.getComputerLabel(),
    TERMINATE_QUIT = "no",
    ["*"] = table.concat(args, " "),
    ["@"] = function() return table.concat(args, " ") end,
    ["#"] = #args,
    ["?"] = 0,
    ["0"] = topshell and topshell.getRunningProgram() or "cash.lua",
    _ = topshell and topshell.getRunningProgram() or "cash.lua",
    ["$"] = CCKernel2 and kernel.getPID() or (OpusOS and kernel.getCurrent() or 0),
}

local aliases = topshell and topshell.aliases() or {}
local completion = topshell and topshell.getCompletionInfo() or {}
local if_table, if_statement = {}, 0
local while_table, while_statement = {}, 0
local case_table, case_statement = {}, 0
local function_name = nil
local functions = {}
local history = {}
local historyfile
local run_tokens
local function_running = false
local should_break = false
local no_funcs = false
local dirstack = {}
local jobs = {}
local completed_jobs = {}

local builtins
builtins = {
    [":"] = function() return 0 end,
    ["."] = function(path)
        path = fs.exists(path) and path or shell.resolve(path)
        local file = io.open(path, "r")
        if not file then return 1 end
        vars.LINENUM = 1
        for line in file:lines() do 
            shell.run(line) 
            vars.LINENUM = vars.LINENUM + 1
        end
        vars.LINENUM = nil
        file:close()
    end,
    echo = function(...) print(...); return 0 end,
    builtin = function(name, ...) return builtins[name](...) end,
    cd = function(dir)
        if not fs.isDir(shell.resolve(dir or "/")) then 
            printError("cash: cd: " .. dir .. ": No such file or directory")
            return 1
        end
        OLDPWD = PWD
        PWD = shell.resolve(dir or "/") 
    end,
    command = function(...) no_funcs = true; shell.run(...); no_funcs = false; return vars["?"] end,
    complete = function() end, -- TODO
    eval = function(...) shell.run(...); return vars["?"] end,
    exec = function(...) execCommand = table.concat({...}, ' '); shell.exit() end,
    exit = shell.exit,
    export = function(...)
        local vars = {...}
        if #vars == 0 or vars[1] == "-p" then for k,v in pairs(_ENV) do if type(v) == "string" or type(v) == "number" then print("export " .. k .. "=" .. v) end end else
            for k,v in ipairs(vars) do
                local kk, vv = string.match(v, "(.+)=(.+)")
                if not (kk == nil or vv == nil) and (_ENV[kk] == nil or type(_ENV[kk]) == "string" or type(_ENV[kk]) == "number") then _ENV[kk] = vv end
            end
        end
    end,
    history = function(...)
        if ({...})[1] == "-c" then
            historyfile.close()
            historyfile = fs.open(CCKernel2 and "/~/.cash_history" or ".cash_history", "w")
            history = {}
            return
        end
        local lines = {}
        for k,v in ipairs(history) do print(" " .. k .. string.rep(" ", math.floor(math.log10(#history)) - math.floor(math.log10(k)) + 2) .. v) end
        --textutils.tabulate(table.unpack(lines))
    end,
    jobs = function(...)
        local filter = {...}
        for k,v in pairs(jobs) do if v.cmd ~= "jobs" then
            if #filter == 0 then print("[" .. k .. "]+  " .. (v.paused and "Paused" or "Running") .. "  " .. v.cmd) 
            else for l,w in ipairs(filter) do 
                if k == w then print("[" .. k .. "]+  " .. (v.paused and "Paused" or "Running") .. "  " .. v.cmd) end 
            end end 
        end end
    end,
    pushd = function(newdir)
        table.insert(dirstack, PWD)
        if newdir then PWD = shell.resolve(newdir) end
        write((PWD == "" and "/" or PWD) .. " ")
        for i = #dirstack, 1, -1 do write((dirstack[i] == "" and "/" or dirstack[i]) .. " ") end
        print()
    end,
    popd = function()
        if #dirstack == 0 then
            printError("cash: popd: directory stack empty")
            return -1
        end
        PWD = table.remove(dirstack, #dirstack)
        write((PWD == "" and "/" or PWD) .. " ")
        for i = #dirstack, 1, -1 do write((dirstack[i] == "" and "/" or dirstack[i]) .. " ") end
        print()
    end,
    dirs = function()
        write((PWD == "" and "/" or PWD) .. " ")
        for i = #dirstack, 1, -1 do write((dirstack[i] == "" and "/" or dirstack[i]) .. " ") end
        print()
    end,
    pwd = function() print(PWD) end,
    read = function(var) -- TODO: expand
        vars[var] = read()
    end,
    set = function(...)
        local lvars = {...}
        if #lvars == 0 then for k,v in pairs(vars) do print(k .. "=" .. v) end else
            for k,v in ipairs(lvars) do
                if string.find(v, "=") then
                    local kk, vv = string.match(v, "(.+)=(.+)")
                    vars[kk] = vv
                end
            end
        end
    end,
    alias = function(...)
        local vars = {...}
        if #vars == 0 or vars[1] == "-p" then for k,v in pairs(aliases) do print("alias " .. k .. "=" .. v) end else
            for k,v in ipairs(vars) do
                local kk, vv = string.match(v, "(.+)=(.+)")
                aliases[kk] = vv
            end
        end
    end,
    sleep = function(time) sleep(tonumber(time)) end,
    test = function(...) -- TODO: add and/or
        local args = {...}
        if #args < 1 then
            printError("cash: test: unary operator expected")
            return -1
        end
        local function n(v) return v end
        if args[1] == "!" then
            table.remove(args, 1)
            n = function(v) return not v end
        end
        if string.sub(args[1], 1, 1) == "-" then
            if args[2] == nil then return n(true)
            elseif args[1] == "-d" then return n(fs.exists(shell.resolve(args[2])) and fs.isDir(shell.resolve(args[2])))
            elseif args[1] == "-e" then return n(fs.exists(shell.resolve(args[2])))
            elseif args[1] == "-f" then return n(fs.exists(shell.resolve(args[2])) and not fs.isDir(shell.resolve(args[2])))
            elseif args[1] == "-n" then return n(#args[2] > 0)
            elseif args[1] == "-s" then return n(fs.getSize(shell.resolve(args[2])) > 0)
            elseif args[1] == "-u" and type(CCKernel2) == "table" then return n(fs.hasPermissions(shell.resolve(args[2]), fs.permissions.setuid))
            elseif args[1] == "-w" then return n(not fs.isReadOnly(shell.resolve(args[2])))
            elseif args[1] == "-x" then return n(true)
            elseif args[1] == "-z" then return n(#args[2] == 0)
            else return n(false) end
        elseif args[3] and string.sub(args[2], 1, 1) == "-" then
            if args[2] == "-eq" then return n(tonumber(args[1]) == tonumber(args[3]))
            elseif args[2] == "-ne" then return n(tonumber(args[1]) ~= tonumber(args[3]))
            elseif args[2] == "-lt" then return n(tonumber(args[1]) < tonumber(args[3]))
            elseif args[2] == "-gt" then return n(tonumber(args[1]) > tonumber(args[3]))
            elseif args[2] == "-le" then return n(tonumber(args[1]) <= tonumber(args[3]))
            elseif args[2] == "-ge" then return n(tonumber(args[1]) >= tonumber(args[3]))
            else return n(false) end
        elseif args[2] == "=" then return n(args[1] == args[3])
        elseif args[2] == "!=" then return n(args[1] ~= args[3])
        else
            printError("cash: test: unary operator expected")
            return 2
        end
    end,
    ["true"] = function() return 0 end,
    ["false"] = function() return 1 end,
    unalias = function(...) for k,v in ipairs({...}) do aliases[v] = nil end end,
    unset = function(...) for k,v in ipairs({...}) do vars[v] = nil end end,
    wait = function(job)
        if job then while jobs[tonumber(job)] ~= nil do sleep(0.1) end
        else while table.maxn(jobs) ~= 0 do sleep(0.1) end end
    end,
    lua = function(...)
        if #({...}) > 0 then
            if fs.exists(shell.resolve(...)) then
                local args = {...}
                table.remove(args, 1)
                shell.run(shell.resolve(...), table.unpack(args)) 
            else
                local s = table.concat({...}, " ")
                local tEnv = setmetatable({shell = shell, multishell = multishell, package = pack, require = require, _echo = function(...) return ... end}, {__index = _ENV})
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
                end
            end
        else shell.run("/rom/programs/lua.lua") end
    end,
    cat = function(...)
        for k,v in ipairs({...}) do
            local file = fs.open(v, "r")
            if file ~= nil then
                print(file.readAll())
                file.close()
            end
        end
    end,
    which = function(name) local name, v = shell.resolveProgram(name); if not v and name then print(name) end end,
    ["if"] = function(...)
        shell.run(...)
        table.insert(if_table, {cond = vars["?"] == 0, inv = false})
    end,
    ["then"] = function(...) 
        if if_statement >= table.maxn(if_table) then
            printError("cash: syntax error near unexpected token `then'")
            return -1
        end
        if_statement = if_statement + 1
        shell.run(...) 
        return vars["?"]
    end,
    ["else"] = function(...)
        if if_statement < 1 or if_table[if_statement].inv then
            printError("cash: syntax error near unexpected token `else'")
            return -1
        end
        if_table[if_statement].inv = true
        if_table[if_statement].cond = not if_table[if_statement].cond
        shell.run(...)
        return vars["?"]
    end,
    fi = function()
        if if_statement < 1 then
            printError("cash: syntax error near unexpected token `fi'")
            return -1
        end
        table.remove(if_table, if_statement)
        if_statement = if_statement - 1
    end,
    ["while"] = function(...)
        table.insert(while_table, {cond = {...}, lines = {}})
    end,
    ["do"] = function(...)
        if table.maxn(while_table) == 0 then
            printError("cash: syntax error near unexpected token `do'")
            return -1
        end
        while_statement = while_statement + 1
        shell.run(...)
    end,
    done = function()
        if while_statement < 1 then
            printError("cash: syntax error near unexpected token `done'")
            return -1
        end
        while_statement = while_statement - 1
        if while_statement == 0 then
            local last = table.remove(while_table, while_statement + 1)
            if type(last.cond) == "function" then last.cond()
            else shell.run(table.unpack(last.cond)) end
            local cond = vars["?"]
            should_break = false
            while cond == 0 and not should_break do
                for k,v in ipairs(last.lines) do 
                    if type(v) == "function" then v()
                    else shell.run(v) end
                end
                if type(last.cond) == "function" then last.cond()
                else shell.run(table.unpack(last.cond)) end
                cond = vars["?"]
            end
        end
    end,
    ["break"] = function() should_break = true end,
    ["for"] = function(...)
        local args = {...}
        if args[2] ~= "in" then
            printError("cash: missing `in' in for loop")
            return -1
        end
        local i = 2
        table.insert(while_table, {cond = function() i = i + 1; vars["?"] = args[i] ~= nil and 0 or 1 end, lines = {function() vars[args[1]] = args[i] end}})
    end,
    ["function"] = function(name, p)
        if function_name ~= nil then
            printError("cash: syntax error near unexpected token `function'")
            return -1
        end
        if p ~= "{" then
            printError("cash: syntax error near token `" .. name .. "'")
            return -1
        end
        function_name = name
        functions[function_name] = {}
    end,
    ["}"] = function() 
        if function_name == nil then
            printError("cash: syntax error near unexpected token `}'")
            return -1
        end
        function_name = nil 
    end,
    ["return"] = function(var)
        if function_running == false then
            printError("cash: syntax error near unexpected token `return'")
            return -1
        end
        function_running = false
        return var
    end,
    bg = function(t)
        if pausedJob then 
            jobs[pausedJob].isfg = false
            jobs[pausedJob].paused = false
            if CCKernel2 then kernel.signal(signal.SIGCONT, jobs[pausedJob].pid) end
            pausedJob = nil
            return 0
        elseif tonumber(t) and jobs[tonumber(t)] then
            local task = tonumber(t)
            jobs[task].isfg = false
            jobs[task].paused = false
            if CCKernel2 then kernel.signal(signal.SIGCONT, jobs[task].pid) end
            return 0
        else
            printError("cash: bg: current: no such job")
            return 1
        end
    end,
    fg = function(t)
        if pausedJob then 
            jobs[pausedJob].isfg = true
            jobs[pausedJob].paused = false
            if CCKernel2 then kernel.signal(signal.SIGCONT, jobs[pausedJob].pid) end
            pausedJob = nil
            return 0
        elseif tonumber(t) and jobs[tonumber(t)] then
            local task = tonumber(t)
            jobs[task].isfg = true
            jobs[task].paused = false
            if CCKernel2 then kernel.signal(signal.SIGCONT, jobs[task].pid) end
            return 0
        else
            printError("cash: fg: current: no such job")
            return 1
        end
    end,
}
builtins["["] = builtins.test

pack.loaded = {
    _G = _G,
    bit32 = bit32,
    coroutine = coroutine,
    math = math,
    package = pack,
    string = string,
    table = table,
}
pack.loaders = {
    function( name )
        if pack.preload[name] then
            return pack.preload[name]
        else
            return nil, "no field package.preload['" .. name .. "']"
        end
    end,
    function( name )
        local fname = string.gsub(name, "%.", "/")
        local sError = ""
        for pattern in string.gmatch(pack.path, "[^;]+") do
            local sPath = string.gsub(pattern, "%?", fname)
            if sPath:sub(1,1) ~= "/" then
                sPath = fs.combine(fs.getDir(vars._), sPath)
            end
            if fs.exists(sPath) and not fs.isDir(sPath) then
                local fnFile, sError = loadfile( sPath, setmetatable({shell = shell, multishell = multishell, package = pack, require = require}, {__index = _ENV}) )
                if fnFile then
                    return fnFile, sPath
                else
                    return nil, sError
                end
            else
                if #sError > 0 then
                    sError = sError .. "\n"
                end
                sError = sError .. "no file '" .. sPath .. "'!"
            end
        end
        return nil, sError
    end
}
pack.preload = {}
pack.config = "/\n;\n?\n!\n-"
pack.path = "?;?.lua;?/init.lua;/rom/modules/main/?;/rom/modules/main/?.lua;/rom/modules/main/?/init.lua"
if turtle then
    pack.path = pack.path..";/rom/modules/turtle/?;/rom/modules/turtle/?.lua;/rom/modules/turtle/?/init.lua"
elseif command then
    pack.path = pack.path..";/rom/modules/command/?;/rom/modules/command/?.lua;/rom/modules/command/?/init.lua"
end
pack.custom = true

local sentinel = {}
function require( name )
    if type( name ) ~= "string" then
        error( "bad argument #1 (expected string, got " .. type( name ) .. ")", 2 )
    end
    if pack.loaded[name] == sentinel then
        error("Loop detected requiring '" .. name .. "'", 0)
    end
    if pack.loaded[name] then
        return pack.loaded[name]
    end

    local sError = "Error loading module '" .. name .. "':"
    for n,searcher in ipairs(pack.loaders) do
        local loader, err = searcher(name)
        if loader then
            pack.loaded[name] = sentinel
            local result = loader( err )
            if result ~= nil then
                pack.loaded[name] = result
                return result
            else
                pack.loaded[name] = true
                return true
            end
        else
            sError = sError .. "\n" .. err
        end
    end
    error(sError, 2)
end

function shell.exit(retval)
    running = false
    shell_retval = retval or 0
end

function shell.dir()
    return PWD
end

function shell.setDir(path)
    OLDPWD = PWD
    PWD = path
end

function shell.path()
    return PATH
end

function shell.setPath(path)
    PATH = path
end

function shell.resolve(localPath)
    if string.sub(localPath, 1, 1) == "/" then return fs.combine(localPath, "")
    else return fs.combine(PWD, localPath) end
end

function shell.resolveProgram(name)
    if builtins[name] ~= nil then return name end
    if aliases[name] ~= nil then name = aliases[name] end
    for path in string.gmatch(PATH, "[^:]+") do
        if fs.exists(fs.combine(shell.resolve(path), name)) and not fs.isDir(fs.combine(shell.resolve(path), name)) then return fs.combine(shell.resolve(path), name)
        elseif fs.exists(fs.combine(shell.resolve(path), name .. ".lua")) and not fs.isDir(fs.combine(shell.resolve(path), name .. ".lua")) then return fs.combine(shell.resolve(path), name .. ".lua") end
    end
    if fs.exists(shell.resolve(name)) and not fs.isDir(shell.resolve(name)) then return shell.resolve(name), string.find(name, "/") == nil end
    if fs.exists(shell.resolve(name .. ".lua")) and not fs.isDir(shell.resolve(name .. ".lua")) then return shell.resolve(name .. ".lua"), string.find(name, "/") == nil end
    return nil
end

function shell.aliases()
    return aliases
end

function shell.setAlias(alias, program)
    aliases[alias] = program
end

function shell.clearAlias(alias)
    aliases[alias] = nil
end

local function combineArray(dst, src, prefix)
    for k,v in ipairs(src) do table.insert(dst, (prefix or "") .. v) end
    return dst
end

function shell.programs(showHidden)
    local retval = {}
    for path in string.gmatch(PATH, "[^:]+") do combineArray(retval, fs.find(fs.combine(shell.resolve(path), "*"))) end
    combineArray(retval, fs.find(fs.combine(PWD, "*")), "./")
    return retval
end

function shell.getRunningProgram()
    return vars._
end

function shell.complete(prefix)
    return fs.complete(prefix, PWD)
end

function shell.completeProgram(prefix)
    if string.find(prefix, "/") then
        return fs.complete(prefix, PWD, true, false)
    else
        local retval = {}
        for path in string.gmatch(PATH, "[^:]+") do combineArray(retval, fs.complete(prefix, path, true, false)) end
        return retval
    end
end

function shell.setCompletionFunction(path, completionFunction)
    completion[path] = {fnComplete = completionFunction}
end

function shell.getCompletionInfo()
    return completion
end

function shell.switchTab() end

function multishell.getCurrent()
    return 1
end

function multishell.getCount()
    return 1
end

function multishell.setFocus(id)
    return id == 1
end

function multishell.setTitle(title) 
    shell_title = title   
end

function multishell.getTitle()
    return shell_title
end

function multishell.getFocus()
    return 1
end

function shell.environment()
    return shell_env
end

function shell.setEnvironment(e)
    shell_env = e
end

local function expandVar(var)
    if string.sub(var, 1, 1) ~= "$" then return nil end
    if string.sub(var, 2, 2) == "{" then
        local varname = string.sub(string.match(var, "%b{}"), 2, -2)
        local retval = _ENV[varname] or vars[varname]
        if type(retval) == "function" then return retval(), #varname + 2 else return retval or "", #varname + 2 end
    elseif string.sub(var, 2, 3) == "((" then
        local expr = string.gsub(string.sub(string.match(string.sub(var, 3), "%b()"), 2, -2), "%$", "")
        local fn = loadstring("return " .. expr)
        local varenv = setmetatable({}, {__index = _ENV})
        for k,v in pairs(vars) do varenv[k] = v end
        setfenv(fn, varenv)
        return tostring(fn()), #expr + 4
    elseif tonumber(string.sub(var, 2, 2)) then
        local varname = tonumber(string.match(string.sub(var, 2, 2), "[0-9]+"))
        if varname == 0 then return vars["0"], 1 else return args[varname] or "", math.floor(math.log10(varname)) + 1 end
    else
        local varname = ""
        for c in string.gmatch(string.sub(var, 2), ".") do
            if c == " " then return "", #varname end
            varname = varname .. c
            if _ENV[varname] or vars[varname] then
                local retval = _ENV[varname] or vars[varname]
                if type(retval) == "function" then return retval(), #varname else return retval or "", #varname end
            end
        end
        return "", #var - 1
    end
end

local function splitSemicolons(cmdline)
    local escape = false
    local quoted = false
    local j = 1
    local retval = {""}
    local lastc, lastc2
    for c in string.gmatch(cmdline, ".") do
        if lastc == '&' and c ~= '&' and lastc2 ~= '&' and not quoted and not escape then
            j=j+1
            retval[j] = ""
        end
        local setescape = false
        if c == '"' or c == '\'' and not escape then quoted = not quoted
        elseif c == '\\' and not quoted and not escape then 
            setescape = true
            escape = true
        end
        if c == ';' and not quoted and not escape then
            j=j+1
            retval[j] = ""
        elseif not (c == ' ' and retval[j] == "") then retval[j] = retval[j] .. c end
        if not setescape then escape = false end
        lastc2 = lastc
        lastc = c
    end
    return retval
end

local function tokenize(cmdline, noexpand)
    -- Expand vars
    local singleQuote = false
    local escape = false
    local expstr = ""
    local i = 1
    local function tostr(v)
        if type(v) == "boolean" then return v and "true" or "false"
        elseif v == nil then return "nil"
        elseif type(v) == "table" then return textutils.serialize(v)
        elseif type(v) == "string" then return v
        else return tostring(v) end
    end
    if noexpand then expstr = cmdline else
        while i <= #cmdline do
            local c = string.sub(cmdline, i, i)
            if c == '$' and not escape and not singleQuote then
                local s, n = expandVar(string.sub(cmdline, i))
                s = tostr(s)
                expstr = expstr .. s
                i = i + n
            else
                if c == '\'' and not escape then singleQuote = not singleQuote end
                escape = c == '\\' and not escape
                expstr = expstr .. c
            end
            i=i+1
        end
    end
    -- Tokenize
    local retval = {{[0] = ""}}
    i = 0
    local j = 1
    local quoted = false
    escape = false
    local lastc
    for c in string.gmatch(expstr, ".") do
        if (c == '"' or c == '\'') and not escape then quoted = not quoted
        elseif c == ' ' and not quoted and not escape then
            if #retval[j][i] > 0 then
                i=i+1
                retval[j][i] = ""
            end
        elseif c == ';' and not quoted and not escape then
            j=j+1
            i=0
            retval[j] = {[0] = ""}
        elseif lastc == '&' and c == '&' and not quoted and not escape then
            retval[j][i] = string.sub(retval[j][i], 1, -2)
            j=j+1
            i=0
            retval[j] = {[0] = "", last = 0}
        elseif lastc == '|' and c == '|' and not quoted and not escape then
            retval[j][i] = string.sub(retval[j][i], 1, -2)
            j=j+1
            i=0
            retval[j] = {[0] = "", last = 1}
        elseif not (c == '\\' and not quoted and not escape) then
            retval[j][i] = retval[j][i] .. c 
        end
        escape = c == '\\' and not quoted and not escape
        lastc = c
    end
    if lastc == '&' then retval.async = true end
    for k,v in ipairs(retval) do if v[0] ~= "" then
        local path, islocal = shell.resolveProgram(v[0])
        path = path or v[0]
        if not (islocal and string.find(v[0], "/") == nil) then v[0] = path end
        v.vars = {}
        while v[0] and string.find(v[0], "=") do
            local l = string.sub(v[0], 1, string.find(v[0], "=") - 1)
            v.vars[l] = string.sub(v[0], string.find(v[0], "=") + 1)
            v.vars[l] = tonumber(v.vars[l]) or v.vars[l]
            v[0] = nil
            for i = 1, table.maxn(v) do v[i-1] = v[i]; v[i] = nil end
        end
    end end
    return retval
end

local junOff = 31 + 28 + 31 + 30 + 31 + 30
local function dayToString(day)
    if day <= 31 then return "Jan " .. day
    elseif day > 31 and day <= 31 + 28 then return "Feb " .. day - 31
    elseif day > 31 + 28 and day <= 31 + 28 + 31 then return "Mar " .. day - 31 - 28
    elseif day > 31 + 28 + 31 and day <= 31 + 28 + 31 + 30 then return "Apr " .. day - 31 - 28 - 31
    elseif day > 31 + 28 + 31 + 30 and day <= 31 + 28 + 31 + 30 + 31 then return "May " .. day - 31 - 28 - 31 - 30
    elseif day > 31 + 28 + 31 + 30 + 31 and day <= junOff then return "Jun " .. day - 31 - 28 - 31 - 30 - 31
    elseif day > junOff and day <= junOff + 31 then return "Jul " .. day - junOff
    elseif day > junOff + 31 and day <= junOff + 31 + 31 then return "Aug " .. day - junOff - 31
    elseif day > junOff + 31 + 31 and day <= junOff + 31 + 31 + 30 then return "Sep " .. day - junOff - 31 - 31
    elseif day > junOff + 31 + 31 + 30 and day <= junOff + 31 + 31 + 30 + 31 then return "Oct " .. day - junOff - 31 - 31 - 30
    elseif day > junOff + 31 + 31 + 30 + 31 and day <= junOff + 31 + 31 + 30 + 31 + 30 then return "Nov " .. day - junOff - 31 - 31 - 30 - 31
    else return "Dec " .. day - junOff - 31 - 31 - 30 - 31 - 30 end
end

local function getPrompt()
    local retval = (if_statement > 0 or while_statement > 0 or case_statement > 0) and vars.PS2 or vars.PS1 or "\\$ "
    for k,v in pairs({
        ["\\d"] = dayToString(os.day()),
        ["\\e"] = string.char(0x1b),
        ["\\h"] = string.sub(os.getComputerLabel() or "localhost", 1, string.find(os.getComputerLabel() or "localhost", "%.")),
        ["\\H"] = os.getComputerLabel() or "localhost",
        ["\\n"] = "\n",
        ["\\s"] = string.gsub(fs.getName(vars["0"]), ".lua", ""),
        ["\\t"] = textutils.formatTime(os.time(), true),
        ["\\T"] = textutils.formatTime(os.time(), false),
        ["\\u"] = USER,
        ["\\v"] = vars.CASH_VERSION,
        ["\\V"] = vars.CASH_VERSION,
        ["\\w"] = PWD,
        ["\\W"] = fs.getName(PWD) == "." and "/" or fs.getName(PWD),
        ["\\%#"] = vars.LINENUM,
        ["\\%$"] = USER == "root" and "#" or "$",
        ["\\([0-7][0-7][0-7])"] = function(n) return string.char(tonumber(n, 8)) end,
        ["\\\\"] = "\\",
        ["\\%[.+\\%]"] = ""
    }) do retval = string.gsub(retval, k, v) end
    return retval
end

local function run( _tEnv, _sPath, ... )
    if type( _tEnv ) ~= "table" then
        error( "bad argument #1 (expected table, got " .. type( _tEnv ) .. ")", 2 ) 
    end
    if type( _sPath ) ~= "string" then
        error( "bad argument #2 (expected string, got " .. type( _sPath ) .. ")", 2 ) 
    end
    local tArgs = table.pack( ... )
    local tEnv = _tEnv
    local fnFile, err = loadfile( _sPath, tEnv )
    if fnFile then
        local ok, err = pcall( function()
            vars["?"] = fnFile( table.unpack( tArgs, 1, tArgs.n ) )
            if vars["?"] == nil or vars["?"] == true then vars["?"] = 0 
            elseif vars["?"] == false then vars["?"] = 1 end
        end )
        if not ok then
            if err and err ~= "" then
                printError( err )
            end
            vars["?"] = 1
            return false
        end
        return true
    end
    if err and err ~= "" then
        printError( err )
    end
    vars["?"] = 1
    return false
end

local function execv(tokens)
    local path = tokens[0]
    if path == nil then return end
    if #tokens == 0 and string.find(path, "=") ~= nil then
        local k = string.sub(path, 1, string.find(path, "=") - 1)
        vars[k] = string.sub(path, string.find(path, "=") + 1)
        vars[k] = tonumber(vars[k]) or vars[k]
        return
    end
    local oldenv = {}
    for k,v in pairs(tokens.vars) do 
        oldenv[k] = _ENV[k]
        _ENV[k] = v 
    end
    if if_statement > 0 and not if_table[if_statement].cond and path ~= "else" and path ~= "elif" and path ~= "fi" then return end
    if builtins[path] ~= nil then 
        vars["?"] = builtins[path](table.unpack(tokens))
        if vars["?"] == nil or vars["?"] == true then vars["?"] = 0 
        elseif vars["?"] == false then vars["?"] = 1 end
    elseif functions[path] ~= nil and not no_funcs then
        local oldargs = args
        args = tokens
        function_running = true
        for k,v in ipairs(functions[path]) do 
            shell.run(v) 
            if not function_running then break end
        end
        args = oldargs
    else
        if not fs.exists(path) then
            printError("cash: " .. path .. ": No such file or directory")
            vars["?"] = -1
            return
        end
        if not CCKernel2 then
            local file = fs.open(path, "r")
            local firstLine = file.readLine()
            file.close()
            if firstLine ~= nil and string.sub(firstLine, 1, 2) == "#!" then
                table.insert(tokens, 1, path)
                path = string.sub(firstLine, 3)
                if not fs.exists(path) and fs.exists(path .. ".lua") then path = path .. ".lua" end
            end
        end
        local _old = vars._
        vars._ = path
        run(setmetatable({shell = shell, multishell = multishell, package = pack, require = require, arg = tokens}, {__index = shell_env}), path, table.unpack(tokens)) 
        vars._ = _old
    end
    for k,v in pairs(tokens.vars) do _ENV[k] = oldenv[k] end
end

run_tokens = function(tokens, isAsync)
    if tokens.async and not isAsync then
        local coro, pid
        if CCKernel2 then pid = kernel.fork("cash", function() run_tokens(tokens, true) end)
        else coro = coroutine.create(function() run_tokens(tokens, true) end) end
        local id = #jobs + 1
        jobs[id] = {cmd = tokens[1][0] .. " " .. table.concat(tokens[1], " "), coro = coro, pid = pid, isfg = false, start = true}
        print("[" .. (id) .. "] " .. (pid or ""))
    else
        for k,tok in ipairs(tokens) do 
            if tok[0] then
                if trim(tok[0]) ~= "" and (tok.last == 0 and vars["?"] == 0) or (tok.last == 1 and vars["?"] ~= 0) or tok.last == nil then
                    execv(tok) 
                end
            else
                for k,v in pairs(tok.vars) do vars[k] = tonumber(v) or v end
            end 
        end
    end
    return vars["?"] == 0
end

local run_tokens_async = function(tokens)
    local coro, pid
    if CCKernel2 then pid = kernel.fork("cash", function() run_tokens(tokens, true) end)
    else coro = coroutine.create(function() run_tokens(tokens, true) end) end
    local id = #jobs + 1
    jobs[id] = {cmd = tokens[1][0] and (tokens[1][0] .. " " .. table.concat(tokens[1], " ")) or "cash", coro = coro, pid = pid, isfg = not tokens.async, start = true}
    if tokens.async then print("[" .. (id) .. "] " .. (pid or "")) end
end

function shell.run(...)
    local cmd = table.concat({...}, " ")
    if cmd == "" or string.sub(cmd, 1, 1) == "#" then return end
    if function_name ~= nil then
        if string.find(cmd, "}") then function_name = nil
        else table.insert(functions[function_name], cmd) end
        return true
    elseif while_statement > 0 then
        local tokens = splitSemicolons(cmd)
        for k,line in ipairs(tokens) do 
            line = string.sub(line, #string.match(line, "^ *") + 1)
            if line == "do" or line == "done" or string.find(line, "^do ") or string.find(line, "^done ") then run_tokens(tokenize(line)) end
            if while_statement > 0 then table.insert(while_table[1].lines, line) end
        end
        return true
    end
    local lines = splitSemicolons(cmd)
    for k,v in ipairs(lines) do run_tokens(tokenize(v, string.sub(v, 1, 6) == "while ")) end
    return vars["?"] == 0
end

function shell.runAsync(...)
    local cmd = table.concat({...}, " ")
    if cmd == "" or string.sub(cmd, 1, 1) == "#" then return end
    if function_name ~= nil then
        if string.find(cmd, "}") then function_name = nil
        else table.insert(functions[function_name], cmd) end
        return true
    elseif while_statement > 0 then
        local tokens = splitSemicolons(cmd)
        for k,line in ipairs(tokens) do 
            line = string.sub(line, #string.match(line, "^ *") + 1)
            if line == "do" or line == "done" or string.find(line, "^do ") or string.find(line, "^done ") then run_tokens(tokenize(line)) end
            if while_statement > 0 then table.insert(while_table[1].lines, line) end
        end
        return true
    end
    local lines = splitSemicolons(cmd)
    for k,v in ipairs(lines) do run_tokens_async(tokenize(v, string.sub(v, 1, 6) == "while ")) end
    return vars["?"] == 0
end

function multishell.launch(environment, path, ...)
    local coro, pid
    local tok = {[0] = path, ...}
    if CCKernel2 then pid = kernel.fork(path, function() execv(tok) end)
    else coro = coroutine.create(function() execv(tok) end) end
    local id = #jobs + 1
    jobs[id] = {cmd = path .. " " .. table.concat({...}, " "), coro = coro, pid = pid, isfg = false}
    return id
end

function shell.openTab(...)
    local cmd = table.concat({...}, " ")
    if cmd == "" or string.sub(cmd, 1, 1) == "#" then return end
    if function_name ~= nil then
        if string.find(cmd, "}") then function_name = nil
        else table.insert(functions[function_name], cmd) end
        return true
    elseif while_statement > 0 then
        local tokens = splitSemicolons(cmd)
        for k,line in ipairs(tokens) do 
            line = string.sub(line, #string.match(line, "^ *") + 1)
            if line == "do" or line == "done" or string.find(line, "^do ") or string.find(line, "^done ") then run_tokens(tokenize(line)) end
            if while_statement > 0 then table.insert(while_table[1].lines, line) end
        end
        return true
    end
    local lines = splitSemicolons(cmd)
    for k,v in ipairs(lines) do 
        local tokens = tokenize(v, string.sub(v, 1, 6) == "while ")
        for k,tok in ipairs(tokens) do if tok[0] ~= "" then 
            local coro, pid
            if CCKernel2 then pid = kernel.fork(tok[0], function() execv(tok) end)
            else coro = coroutine.create(function() execv(tok) end) end
            local id = #jobs + 1
            jobs[id] = {cmd = tok[0] .. " " .. table.concat(tok, " "), coro = coro, pid = pid, isfg = false}
            print("[" .. (id) .. "] " .. (pid or ""))
        end end
    end
    return vars["?"] == 0
end

if args[1] ~= nil then
    local path = table.remove(args, 1)
    path = fs.exists(path) and path or shell.resolve(path)
    local file = io.open(path, "r")
    if not file then return 1 end
    vars.LINENUM = 1
    for line in file:lines() do 
        shell.run(line) 
        vars.LINENUM = vars.LINENUM + 1
        if not running then break end
    end
    file:close()
    vars.LINENUM = nil
    return shell_retval
end

if topshell == nil then shell.run("rom/startup.lua") end

if CCKernel2 then
    if fs.exists("/etc/cashrc") then
        local file = io.open("/etc/cashrc", "r")
        for line in file:lines() do shell.run(line) end
        file:close()
    end
    local function lines(file) return function() return file.readLine() end end
    if fs.exists("/~/.cashrc") then
        local file = fs.open("/~/.cashrc", "r")
        for line in lines(file) do shell.run(line) end
        file.close()
    end
    if fs.exists("/~/.cash_history") then
        local file = fs.open("/~/.cash_history", "r")
        for line in lines(file) do table.insert(history, line) end
        file.close()
        historyfile = fs.open("/~/.cash_history", "a")
    else historyfile = fs.open("/~/.cash_history", "w") end
else
    if fs.exists(".cashrc") then
        local file = io.open(".cashrc", "r")
        for line in file:lines() do shell.run(line) end
        file:close()
    end
    if fs.exists(".cash_history") then
        local file = io.open(".cash_history", "r")
        for line in file:lines() do table.insert(history, line) end
        file:close()
        historyfile = fs.open(".cash_history", "a")
    else historyfile = fs.open(".cash_history", "w") end
end

local function ansiWrite(str)
    local seq = nil
    local bold = false
    local function getnum(d) 
        if seq == "[" then return d or 1
        elseif string.find(seq, ";") then return 
            tonumber(string.sub(seq, 2, string.find(seq, ";") - 1)), 
            tonumber(string.sub(seq, string.find(seq, ";") + 1)) 
        else return tonumber(string.sub(seq, 2)) end 
    end
    for c in string.gmatch(str, ".") do
        if seq == "\27" then
            if c == "c" then
                term.setBackgroundColor(colors.black)
                term.setTextColor(colors.white)
                term.setCursorBlink(true)
            elseif c == "[" then seq = "["
            else seq = nil end
        elseif seq ~= nil and string.sub(seq, 1, 1) == "[" then
            if tonumber(c) ~= nil or c == ';' then seq = seq .. c else
                if c == "A" then term.setCursorPos(term.getCursorPos(), select(2, term.getCursorPos()) - getnum())
                elseif c == "B" then term.setCursorPos(term.getCursorPos(), select(2, term.getCursorPos()) + getnum())
                elseif c == "C" then term.setCursorPos(term.getCursorPos() + getnum(), select(2, term.getCursorPos()))
                elseif c == "D" then term.setCursorPos(term.getCursorPos() - getnum(), select(2, term.getCursorPos()))
                elseif c == "E" then term.setCursorPos(1, select(2, term.getCursorPos()) + getnum())
                elseif c == "F" then term.setCursorPos(1, select(2, term.getCursorPos()) - getnum())
                elseif c == "G" then term.setCursorPos(getnum(), select(2, term.getCursorPos()))
                elseif c == "H" then term.setCursorPos(getnum())
                elseif c == "J" then term.clear() -- ?
                elseif c == "K" then term.clearLine() -- ?
                elseif c == "T" then term.scroll(getnum())
                elseif c == "f" then term.setCursorPos(getnum())
                elseif c == "m" then
                    local n, m = getnum(0)
                    if n == 0 then
                        term.setBackgroundColor(colors.black)
                        term.setTextColor(colors.white)
                    elseif n == 1 then bold = true
                    elseif n == 7 or n == 27 then
                        local bg = term.getBackgroundColor()
                        term.setBackgroundColor(term.getTextColor())
                        term.setTextColor(bg)
                    elseif n == 22 then bold = false
                    elseif n >= 30 and n <= 37 then term.setTextColor(2^(15 - (n - 30) - (bold and 8 or 0)))
                    elseif n == 39 then term.setTextColor(colors.white)
                    elseif n >= 40 and n <= 47 then term.setBackgroundColor(2^(15 - (n - 40) - (bold and 8 or 0)))
                    elseif n == 49 then term.setBackgroundColor(colors.black) end
                    if m ~= nil then
                        if m == 0 then
                            term.setBackgroundColor(colors.black)
                            term.setTextColor(colors.white)
                        elseif m == 1 then bold = true
                        elseif m == 7 or m == 27 then
                            local bg = term.getBackgroundColor()
                            term.setBackgroundColor(term.getTextColor())
                            term.setTextColor(bg)
                        elseif m == 22 then bold = false
                        elseif m >= 30 and m <= 37 then term.setTextColor(2^(15 - (m - 30) - (bold and 8 or 0)))
                        elseif m == 39 then term.setTextColor(colors.white)
                        elseif m >= 40 and m <= 47 then term.setBackgroundColor(2^(15 - (m - 40) - (bold and 8 or 0)))
                        elseif m == 49 then term.setBackgroundColor(colors.black) end
                    end
                end
                seq = nil
            end
        elseif c == string.char(0x1b) then seq = "\27"
        else write(c) end
    end
end

local function readCommand()
    if term.getGraphicsMode and term.getGraphicsMode() then term.setGraphicsMode(false) end
    local prompt = getPrompt()
    ansiWrite(prompt)
    local str = ""
    local ox, oy = term.getCursorPos()
    local coff = 0
    local histpos = table.maxn(history) + 1
    local lastlen = 0
    local waitTab = false
    local ly = ({term.getCursorPos()})[2]
    local function redrawStr()
        term.setCursorPos(ox, oy)
        local x, y
        local i = 0
        for c in string.gmatch(str, ".") do
            if term.getCursorPos() == term.getSize() then
                if select(2, term.getCursorPos()) == select(2, term.getSize()) then
                    term.scroll(1)
                    oy = oy - 1
                    term.setCursorPos(1, select(2, term.getCursorPos()))
                else term.setCursorPos(1, select(2, term.getCursorPos()) + 1) end
            end
            if i == coff then x, y = term.getCursorPos() end
            term.write(c)
            i=i+1
        end
        if x == nil then x, y = term.getCursorPos() end
        for i = 0, lastlen - #str - 1 do write(" ") end
        if term.getCursorPos() == 1 and lastlen > #str then
            term.write(" ")
        end
        lastlen = #str
        ly = ({term.getCursorPos()})[2]
        term.setCursorPos(x, y)
    end
    term.setCursorBlink(true)
    while true do
        local ev = {os.pullEventRaw()}
        if ev[1] == "key" then
            if ev[2] == keys.enter then break
            elseif ev[2] == keys.up and history[histpos-1] ~= nil then 
                histpos = histpos - 1
                str = history[histpos]
                coff = #str
                waitTab = false
            elseif ev[2] == keys.down and history[histpos+1] ~= nil then 
                histpos = histpos + 1
                str = history[histpos]
                coff = #str
                waitTab = false
            elseif ev[2] == keys.down and histpos == table.maxn(history) then
                histpos = histpos + 1
                str = ""
                coff = 0
                waitTab = false
            elseif ev[2] == keys.left and coff > 0 then 
                coff = coff - 1
                waitTab = false
            elseif ev[2] == keys.right and coff < #str then 
                coff = coff + 1
                waitTab = false
            elseif ev[2] == keys.backspace and coff > 0 then
                str = string.sub(str, 1, coff - 1) .. string.sub(str, coff + 1)
                coff = coff - 1
                waitTab = false
            elseif ev[2] == keys.tab and coff == #str then
                local tokens = tokenize(str)[1]
                -- TODO: FIX THIS
                if completion[tokens[0]] ~= nil then
                    local t = {}
                    for i = 1, table.maxn(tokens) - 1 do t[i] = tokens[i] end
                    local res = completion[tokens[0]].fnComplete(shell, table.maxn(tokens), tokens[table.maxn(tokens)], t)
                    if res and #res > 0 then
                        local longest = res[1]
                        local function getLongest(a, b)
                            for i = 1, math.min(#a, #b) do if string.sub(a, i, i) ~= string.sub(b, i, i) then return string.sub(a, 1, i-1) end end
                            return a 
                        end
                        for k,v in ipairs(res) do longest = getLongest(longest, v) end
                        if longest == "" then
                            if not waitTab then waitTab = true else
                                for k,v in ipairs(res) do res[k] = tokens[table.maxn(tokens)] .. v end
                                print("")
                                textutils.pagedTabulate(res)
                                ansiWrite(getPrompt())
                                ox, oy = term.getCursorPos()
                            end
                        else
                            str = str .. longest
                            coff = #str
                            waitTab = false
                        end
                    end
                elseif tokens[1] == nil then
                    local res = shell.completeProgram(tokens[0])
                    if res and #res > 0 then
                        local longest = res[1]
                        local function getLongest(a, b)
                            for i = 1, math.min(#a, #b) do if string.sub(a, i, i) ~= string.sub(b, i, i) then return string.sub(a, 1, i-1) end end
                            return a 
                        end
                        for k,v in ipairs(res) do longest = getLongest(longest, v) end
                        if longest == "" then
                            if not waitTab then waitTab = true else
                                for k,v in ipairs(res) do res[k] = string.gsub(fs.getName(tokens[0]), "%.lua", "") .. v end
                                print("")
                                textutils.pagedTabulate(res)
                                ansiWrite(getPrompt())
                                ox, oy = term.getCursorPos()
                            end
                        else
                            str = str .. string.gsub(longest, "%.lua", "")
                            coff = #str
                            waitTab = false
                        end
                    end
                else
                    local res = fs.complete(tokens[table.maxn(tokens)], PWD, true, true)
                    if res and #res > 0 then
                        local longest = res[1]
                        local function getLongest(a, b)
                            for i = 1, math.min(#a, #b) do if string.sub(a, i, i) ~= string.sub(b, i, i) then return string.sub(a, 1, i-1) end end
                            return a 
                        end
                        for k,v in ipairs(res) do longest = getLongest(longest, v) end
                        if longest == "" then
                            if not waitTab then waitTab = true else
                                for k,v in ipairs(res) do res[k] = fs.getName(tokens[table.maxn(tokens)]) .. v end
                                print("")
                                textutils.pagedTabulate(res)
                                ansiWrite(getPrompt())
                                ox, oy = term.getCursorPos()
                            end
                        else
                            str = str .. longest
                            coff = #str
                            waitTab = false
                        end
                    end
                end
            end
        elseif ev[1] == "char" then
            str = string.sub(str, 1, coff) .. ev[2] .. string.sub(str, coff + 1)
            coff = coff + 1
            waitTab = false
        elseif ev[1] == "paste" then
            str = string.sub(str, 1, coff) .. ev[2] .. string.sub(str, coff + 1)
            coff = coff + #ev[2]
            waitTab = false
        elseif ev[1] == "terminate" then
            if vars.TERMINATE_QUIT == "yes" or vars.terminate_quit == 1 then running = false end
            str = ""
            break
        end
        redrawStr()
    end
    if ly >= ({term.getSize()})[2] then
        term.scroll(1)
        ly = ly - 1
    end
    term.setCursorPos(1, ly + 1)
    term.setCursorBlink(false)
    if str ~= "" and str ~= history[#history] then
        table.insert(history, str)
        historyfile.writeLine(str)
        historyfile.flush()
    end
    return str
end

local function jobManager()
    while running do
        if CCKernel2 then
            local e = {os.pullEventRaw()}
            if e[1] == "process_complete" then
                for k,v in pairs(jobs) do if v.pid == e[2] then 
                    jobs[k] = nil
                    completed_jobs[k] = {err = "Done", cmd = v.cmd}
                    break
                end end
            end
        else
            local delete = {}
            local e = {os.pullEventRaw()}
            for k,v in pairs(jobs) do
                if not v.paused and (v.filter == nil or v.filter == e[1]) and (v.isfg or v.start or not (
                    e[1] == "key" or e[1] == "char" or e[1] == "key_up" or e[1] == "paste" or
                    e[1] == "mouse_click" or e[1] == "mouse_up" or e[1] == "mouse_drag" or 
                    e[1] == "mouse_scroll" or e[1] == "monitor_touch")) then
                    local oldterm = term.current()
                    if v.term then term.redirect(v.term) end
                    local ok, filter = coroutine.resume(v.coro, table.unpack(e))
                    v.term = term.redirect(oldterm)
                    if coroutine.status(v.coro) == "dead" then
                        table.insert(delete, k)
                        completed_jobs[k] = {err = "Done", cmd = v.cmd, isfg = v.isfg}
                        os.queueEvent("job_complete", k)
                    elseif not ok then
                        table.insert(delete, k)
                        completed_jobs[k] = {err = filter, cmd = v.cmd, isfg = v.isfg}
                        os.queueEvent("job_complete", k)
                    end
                    v.filter = filter
                    v.start = false
                end
            end
            for k,v in ipairs(delete) do jobs[v] = nil end
        end
    end
end

parallel.waitForAny(function()
    while running do 
        for k,v in pairs(completed_jobs) do if not v.isfg then print("[" .. k .. "]+  " .. v.err .. "  " .. v.cmd) end end
        completed_jobs = {}
        shell.runAsync(readCommand())
        while #jobs > 0 do
            local b = true
            for k,v in pairs(jobs) do if v.isfg and not v.paused then b = false; break end end
            if b then break end
            if os.pullEventRaw() == "terminate" then
                for k,v in pairs(jobs) do if v.isfg and not v.paused then
                    jobs[k] = nil
                    print("^T")
                    b = true
                    break
                end end
            end
            if b then break end
        end
    end
end, function()
    local ctrlHeld = false
    while running do
        local ev = {os.pullEventRaw()}
        if ev[1] == "key" and (ev[2] == keys.leftCtrl or ev[2] == keys.rightCtrl) then ctrlHeld = true
        elseif ev[1] == "key_up" and (ev[2] == keys.leftCtrl or ev[2] == keys.rightCtrl) then ctrlHeld = false
        elseif ctrlHeld and ev[1] == "key" and ev[2] == keys.z then
            print("^Z")
            for k,v in pairs(jobs) do if v.isfg and not v.paused then 
                v.paused = true
                if CCKernel2 then kernel.signal(signal.SIGSTOP, v.pid) end
                pausedJob = k
                print("[" .. k .. "]+  Paused  " .. v.cmd)
                os.queueEvent("job_paused")
                break
            end end
        end
    end
end, jobManager)

if execCommand then shell.run(execCommand); return vars["?"] end

historyfile.close()
return shell_retval