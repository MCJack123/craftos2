-- SPDX-FileCopyrightText: 2019 JackMacWindows
--
-- SPDX-License-Identifier: MIT

if shell.environment == nil then error("Requires the cash shell.") end
local args = {...}

local env = setmetatable({}, {__index = _ENV})
local path = shell.path()
local unset = {}
local cmd = {}
local cmdargs = false
local nextarg

for k,v in ipairs(args) do
    if nextarg then
        if nextarg == 1 then path = v
        elseif nextarg == 2 then table.insert(unset, v) end
        nextarg = nil
    elseif cmdargs then 
        table.insert(cmd, v) 
    else
        if v == "-i" then setmetatable(env, {__index = _G})
        elseif v == "-P" then nextarg = 1
        elseif v == "-u" then nextarg = 2
        elseif string.find(v, "=") then env[string.sub(v, 1, string.find(v, "=") - 1)] = string.sub(v, string.find(v, "=") + 1)
        else table.insert(cmd, v); cmdargs = true end
    end
end

if #unset > 0 then
    local oldidx = getmetatable(env).__index
    local u = {}
    for k,v in ipairs(unset) do u[v] = true end
    setmetatable(env, {__index = function(self, name) if u[name] then return nil else return oldidx[name] end end})
end

local oldPath = shell.path()
local oldEnv = shell.environment()
shell.setPath(path)
shell.setEnvironment(env)
shell.run(table.unpack(cmd))
shell.setPath(oldPath)
shell.setEnvironment(oldEnv)