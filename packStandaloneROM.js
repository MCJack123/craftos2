// Run this script with Node in the same directory as the ROM
// Also download the IO api into rom/apis for standalone to work:
// https://raw.githubusercontent.com/MCJack123/craftos2-rom/6133337c9b058ea0cb8c72c26a179ab2787c50b5/rom/apis/io.lua
// Generates an fs_standalone.cpp file that can be used for standalone builds
const fs = require("fs");
let out = fs.openSync("fs_standalone.cpp", "w")
fs.writeSync(out, '#include "fs_standalone.hpp"\n\nFileEntry standaloneROM(')
function writeDir(path, level) {
    console.log("Reading directory " + path)
    fs.writeSync(out, "{\n")
    for (var f of fs.readdirSync(path)) {
        if (f == "." || f == ".." || f == ".DS_Store" || f == "desktop.ini") continue;
        fs.writeSync(out, `${' '.repeat(level * 4)}{"${f}", `)
        if (fs.lstatSync(path + "/" + f).isDirectory()) writeDir(path + "/" + f, level + 1)
        else {
            console.log("Reading file " + path + "/" + f)
            fs.writeSync(out, JSON.stringify(fs.readFileSync(path + "/" + f, "utf8")).replace(/\\r\\n/g, "\\n").replace(/\\n/g, "\\n\\\n"))
        }
        fs.writeSync(out, "},\n")
    }
    if (path == "rom/apis") {
        fs.writeSync(out, `${' '.repeat(level * 4)}{"${f}", "-- Definition for the IO API\\n\\
\\n\\
local g_defaultInput = {\\n\\
    bFileHandle = true,\\n\\
    bClosed = false,\\n\\
    close = function( self )\\n\\
    end,\\n\\
    read = function( self, _sFormat )\\n\\
        if _sFormat and _sFormat ~= "*l" then\\n\\
            error( "Unsupported format" )\\n\\
        end\\n\\
        return _G.read()\\n\\
    end,\\n\\
    lines = function( self )\\n\\
        return function()\\n\\
            return _G.read()\\n\\
        end\\n\\
    end,\\n\\
}\\n\\
\\n\\
local g_defaultOutput = {\\n\\
    bFileHandle = true,\\n\\
    bClosed = false,\\n\\
    close = function( self )\\n\\
    end,\\n\\
    write = function( self, ... )\\n\\
        local nLimit = select("#", ... )\\n\\
        for n = 1, nLimit do\\n\\
            _G.write( select( n, ... ) )\\n\\
        end\\n\\
    end,\\n\\
    flush = function( self )\\n\\
    end,\\n\\
}\\n\\
\\n\\
local g_currentInput = g_defaultInput\\n\\
local g_currentOutput = g_defaultOutput\\n\\
\\n\\
function close( _file )\\n\\
    (_file or g_currentOutput):close()\\n\\
end\\n\\
\\n\\
function flush()\\n\\
    g_currentOutput:flush()\\n\\
end\\n\\
\\n\\
function input( _arg )\\n\\
    if _G.type( _arg ) == "string" then\\n\\
        g_currentInput = open( _arg, "r" )\\n\\
    elseif _G.type( _arg ) == "table" then\\n\\
        g_currentInput = _arg\\n\\
    elseif _G.type( _arg ) == "nil" then\\n\\
        return g_currentInput\\n\\
    else\\n\\
        error( "bad argument #1 (expected string/table/nil, got " .. _G.type( _arg ) .. ")", 2 )\\n\\
    end\\n\\
end\\n\\
\\n\\
function lines( _sFileName )\\n\\
    if _G.type( _sFileName ) ~= "string" then\\n\\
        error( "bad argument #1 (expected string, got " .. _G.type( _sFileName ) .. ")", 2 )\\n\\
    end\\n\\
    if _sFileName then\\n\\
        return open( _sFileName, "r" ):lines()\\n\\
    else\\n\\
        return g_currentInput:lines()\\n\\
    end\\n\\
end\\n\\
\\n\\
function open( _sPath, _sMode )\\n\\
    if _G.type( _sPath ) ~= "string" then\\n\\
        error( "bad argument #1 (expected string, got " .. _G.type( _sPath ) .. ")", 2 )\\n\\
    end\\n\\
    if _sMode ~= nil and _G.type( _sMode ) ~= "string" then\\n\\
        error( "bad argument #2 (expected string, got " .. _G.type( _sMode ) .. ")", 2 )\\n\\
    end\\n\\
    local sMode = _sMode or "r"\\n\\
    local file, err = fs.open( _sPath, sMode )\\n\\
    if not file then\\n\\
        return nil, err\\n\\
    end\\n\\
    \\n\\
    if sMode == "r"then\\n\\
        return {\\n\\
            bFileHandle = true,\\n\\
            bClosed = false,				\\n\\
            close = function( self )\\n\\
                file.close()\\n\\
                self.bClosed = true\\n\\
            end,\\n\\
            read = function( self, _sFormat )\\n\\
                local sFormat = _sFormat or "*l"\\n\\
                if sFormat == "*l" then\\n\\
                    return file.readLine()\\n\\
                elseif sFormat == "*a" then\\n\\
                    return file.readAll()\\n\\
                elseif _G.type( sFormat ) == "number" then\\n\\
                    return file.read( sFormat )\\n\\
                else\\n\\
                    error( "Unsupported format", 2 )\\n\\
                end\\n\\
                return nil\\n\\
            end,\\n\\
            lines = function( self )\\n\\
                return function()\\n\\
                    local sLine = file.readLine()\\n\\
                    if sLine == nil then\\n\\
                        file.close()\\n\\
                        self.bClosed = true\\n\\
                    end\\n\\
                    return sLine\\n\\
                end\\n\\
            end,\\n\\
        }\\n\\
    elseif sMode == "w" or sMode == "a" then\\n\\
        return {\\n\\
            bFileHandle = true,\\n\\
            bClosed = false,				\\n\\
            close = function( self )\\n\\
                file.close()\\n\\
                self.bClosed = true\\n\\
            end,\\n\\
            write = function( self, ... )\\n\\
                local nLimit = select("#", ... )\\n\\
                for n = 1, nLimit do\\n\\
                    file.write( select( n, ... ) )\\n\\
                end\\n\\
            end,\\n\\
            flush = function( self )\\n\\
                file.flush()\\n\\
            end,\\n\\
        }\\n\\
    \\n\\
    elseif sMode == "rb" then\\n\\
        return {\\n\\
            bFileHandle = true,\\n\\
            bClosed = false,				\\n\\
            close = function( self )\\n\\
                file.close()\\n\\
                self.bClosed = true\\n\\
            end,\\n\\
            read = function( self )\\n\\
                return file.read()\\n\\
            end,\\n\\
        }\\n\\
        \\n\\
    elseif sMode == "wb" or sMode == "ab" then\\n\\
        return {\\n\\
            bFileHandle = true,\\n\\
            bClosed = false,				\\n\\
            close = function( self )\\n\\
                file.close()\\n\\
                self.bClosed = true\\n\\
            end,\\n\\
            write = function( self, ... )\\n\\
                local nLimit = select("#", ... )\\n\\
                for n = 1, nLimit do\\n\\
                    file.write( select( n, ... ) )\\n\\
                end\\n\\
            end,\\n\\
            flush = function( self )\\n\\
                file.flush()\\n\\
            end,\\n\\
        }\\n\\
    \\n\\
    else\\n\\
        file.close()\\n\\
        error( "Unsupported mode", 2 )\\n\\
        \\n\\
    end\\n\\
end\\n\\
\\n\\
function output( _arg )\\n\\
    if _G.type( _arg ) == "string" then\\n\\
        g_currentOutput = open( _arg, "w" )\\n\\
    elseif _G.type( _arg ) == "table" then\\n\\
        g_currentOutput = _arg\\n\\
    elseif _G.type( _arg ) == "nil" then\\n\\
        return g_currentOutput\\n\\
    else\\n\\
        error( "bad argument #1 (expected string/table/nil, got " .. _G.type( _arg ) .. ")", 2 )\\n\\
    end\\n\\
end\\n\\
\\n\\
function read( ... )\\n\\
    return input():read( ... )\\n\\
end\\n\\
\\n\\
function type( _handle )\\n\\
    if _G.type( _handle ) == "table" and _handle.bFileHandle == true then\\n\\
        if _handle.bClosed then\\n\\
            return "closed file"\\n\\
        else\\n\\
            return "file"\\n\\
        end\\n\\
    end\\n\\
    return nil\\n\\
end\\n\\
\\n\\
function write( ... )\\n\\
    return output():write( ... )\\n\\
end\\n\\
"},`)
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
