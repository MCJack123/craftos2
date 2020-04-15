// Run this script with Node in the same directory as the ROM
// Also download the IO api into rom/apis for standalone to work:
// https://raw.githubusercontent.com/MCJack123/craftos2-rom/6133337c9b058ea0cb8c72c26a179ab2787c50b5/rom/apis/io.lua
// Generates an fs_standalone.cpp file that can be used for standalone builds
const fs = require("fs");
let out = fs.openSync("fs_standalone.cpp", "w")
fs.writeSync(out, '#include "fs_standalone.hpp"\n\nFileEntry standaloneROM(')
let ioapi = `-- Definition for the IO API

local g_defaultInput = {
    bFileHandle = true,
    bClosed = false,
    close = function( self )
    end,
    read = function( self, _sFormat )
        if _sFormat and _sFormat ~= "*l" then
            error( "Unsupported format" )
        end
        return _G.read()
    end,
    lines = function( self )
        return function()
            return _G.read()
        end
    end,
}

local g_defaultOutput = {
    bFileHandle = true,
    bClosed = false,
    close = function( self )
    end,
    write = function( self, ... )
        local nLimit = select("#", ... )
        for n = 1, nLimit do
            _G.write( select( n, ... ) )
        end
    end,
    flush = function( self )
    end,
}

local g_currentInput = g_defaultInput
local g_currentOutput = g_defaultOutput

function close( _file )
    (_file or g_currentOutput):close()
end

function flush()
    g_currentOutput:flush()
end

function input( _arg )
    if _G.type( _arg ) == "string" then
        g_currentInput = open( _arg, "r" )
    elseif _G.type( _arg ) == "table" then
        g_currentInput = _arg
    elseif _G.type( _arg ) == "nil" then
        return g_currentInput
    else
        error( "bad argument #1 (expected string/table/nil, got " .. _G.type( _arg ) .. ")", 2 )
    end
end

function lines( _sFileName )
    if _G.type( _sFileName ) ~= "string" then
        error( "bad argument #1 (expected string, got " .. _G.type( _sFileName ) .. ")", 2 )
    end
    if _sFileName then
        return open( _sFileName, "r" ):lines()
    else
        return g_currentInput:lines()
    end
end

function open( _sPath, _sMode )
    if _G.type( _sPath ) ~= "string" then
        error( "bad argument #1 (expected string, got " .. _G.type( _sPath ) .. ")", 2 )
    end
    if _sMode ~= nil and _G.type( _sMode ) ~= "string" then
        error( "bad argument #2 (expected string, got " .. _G.type( _sMode ) .. ")", 2 )
    end
    local sMode = _sMode or "r"
    local file, err = fs.open( _sPath, sMode )
    if not file then
        return nil, err
    end
    
    if sMode == "r"then
        return {
            bFileHandle = true,
            bClosed = false,				
            close = function( self )
                file.close()
                self.bClosed = true
            end,
            read = function( self, _sFormat )
                local sFormat = _sFormat or "*l"
                if sFormat == "*l" then
                    return file.readLine()
                elseif sFormat == "*a" then
                    return file.readAll()
                elseif _G.type( sFormat ) == "number" then
                    return file.read( sFormat )
                else
                    error( "Unsupported format", 2 )
                end
                return nil
            end,
            lines = function( self )
                return function()
                    local sLine = file.readLine()
                    if sLine == nil then
                        file.close()
                        self.bClosed = true
                    end
                    return sLine
                end
            end,
        }
    elseif sMode == "w" or sMode == "a" then
        return {
            bFileHandle = true,
            bClosed = false,				
            close = function( self )
                file.close()
                self.bClosed = true
            end,
            write = function( self, ... )
                local nLimit = select("#", ... )
                for n = 1, nLimit do
                    file.write( select( n, ... ) )
                end
            end,
            flush = function( self )
                file.flush()
            end,
        }
    
    elseif sMode == "rb" then
        return {
            bFileHandle = true,
            bClosed = false,				
            close = function( self )
                file.close()
                self.bClosed = true
            end,
            read = function( self )
                return file.read()
            end,
        }
        
    elseif sMode == "wb" or sMode == "ab" then
        return {
            bFileHandle = true,
            bClosed = false,				
            close = function( self )
                file.close()
                self.bClosed = true
            end,
            write = function( self, ... )
                local nLimit = select("#", ... )
                for n = 1, nLimit do
                    file.write( select( n, ... ) )
                end
            end,
            flush = function( self )
                file.flush()
            end,
        }
    
    else
        file.close()
        error( "Unsupported mode", 2 )
        
    end
end

function output( _arg )
    if _G.type( _arg ) == "string" then
        g_currentOutput = open( _arg, "w" )
    elseif _G.type( _arg ) == "table" then
        g_currentOutput = _arg
    elseif _G.type( _arg ) == "nil" then
        return g_currentOutput
    else
        error( "bad argument #1 (expected string/table/nil, got " .. _G.type( _arg ) .. ")", 2 )
    end
end

function read( ... )
    return input():read( ... )
end

function type( _handle )
    if _G.type( _handle ) == "table" and _handle.bFileHandle == true then
        if _handle.bClosed then
            return "closed file"
        else
            return "file"
        end
    end
    return nil
end

function write( ... )
    return output():write( ... )
end
`
function writeDir(path, level) {
    console.log("Reading directory " + path)
    fs.writeSync(out, "{\n")
    for (var f of fs.readdirSync(path)) {
        if (f == "." || f == ".." || f == ".DS_Store" || f == "desktop.ini" || f == "packStandaloneROM.js") continue;
        fs.writeSync(out, `${' '.repeat(level * 4)}{"${f}", `)
        if (fs.lstatSync(path + "/" + f).isDirectory()) writeDir(path + "/" + f, level + 1)
        else {
            console.log("Reading file " + path + "/" + f)
            fs.writeSync(out, JSON.stringify(fs.readFileSync(path + "/" + f, "utf8")).replace(/\\r\\n/g, "\\n").replace(/\\n/g, "\\n\\\n"))
        }
        fs.writeSync(out, "},\n")
    }
    if (path == "rom/apis") {
        fs.writeSync(out, `${' '.repeat(level * 4)}{"${f}", ${JSON.stringify(ioapi).replace(/\\r\\n/g, "\\n").replace(/\\n/g, "\\n\\\n")}},`)
    }
    fs.writeSync(out, " ".repeat((level - 1) * 4) + "}")
}
writeDir("rom", 1)
fs.writeSync(out, ");\n\nFileEntry standaloneDebug(")
writeDir("debug", 1)
fs.writeSync(out, ");\n\nstd::string standaloneBIOS = ")
console.log("Reading BIOS")
fs.writeSync(out, JSON.stringify(fs.readFileSync("bios.lua", "utf8")).replace(/\\r\\n/g, "\\n").replace(/\\n/g, "\\n\\\n") + ";")
fs.closeSync(out);
