-- SPDX-FileCopyrightText: 2019-2024 JackMacWindows
--
-- SPDX-License-Identifier: MIT

local function runTests()
    local oldClockSpeed = config.get("clockSpeed")
    config.set("clockSpeed", 1000)
    -- Test characters
    term.redirect(term.native())
    local benchmark = debug.getregistry().benchmark
    local w, h = term.getSize()
    local canBenchmark = benchmark ~= nil
    local count = 0
    local start = os.epoch "utc"
    if canBenchmark then benchmark() end
    pcall(function()
        while true do
            term.setCursorPos(math.random(1, w), math.random(1, h))
            term.setBackgroundColor(2^math.random(0, 15))
            term.setTextColor(2^math.random(0, 15))
            term.write(string.char(math.random(0, 255)))
            count = count + 1
        end
    end)
    local frames = canBenchmark and benchmark() or 0
    local time = os.epoch "utc" - start
    term.setCursorPos(1, 1)
    term.setBackgroundColor(colors.black)
    term.setTextColor(colors.white)
    term.clear()
    if canBenchmark then print("Rendered " .. frames .. " frames in " .. time .. " ms (" .. (frames / (time / 1000)) .. " fps)") end
    print("Drew " .. count .. " characters (" .. (count / (time / 1000)) .. " cps)")
    local score = (frames / (time / 1000)) * 2

    os.queueEvent("nosleep")
    os.pullEvent()

    -- Test pixels
    term.setGraphicsMode(2)
    w, h = w*6-1, h*9-1
    for i = 0, 255 do term.setPaletteColor(i, 7 / bit32.rshift(bit32.band(i, 0xE0), 5), 7 / bit32.rshift(bit32.band(i, 0x1C), 2), 3 / bit32.band(i, 3)) end
    count = 0
    start = os.epoch "utc"
    if canBenchmark then benchmark() end
    pcall(function()
        while true do
            term.setPixel(math.random(0, w), math.random(0, h), math.random(0, 255))
            count = count + 1
        end
    end)
    frames = canBenchmark and benchmark() or 0
    time = os.epoch "utc" - start
    term.clear()
    for i = 0, 15 do term.setPaletteColor(i, term.nativePaletteColor(2^i)) end
    term.setGraphicsMode(0)
    if canBenchmark then print("Rendered " .. frames .. " frames in " .. time .. " ms (" .. (frames / (time / 1000)) .. " fps)") end
    print("Drew " .. count .. " pixels (" .. (count / (time / 1000)) .. " pps)")
    config.set("clockSpeed", oldClockSpeed)
    return score + (frames / (time / 1000))
end

if shell == nil then error("This program must be run from the shell.") end

local mode = ...
if mode ~= "hardware" then
    if config.get("useHardwareRenderer") == true or config.get("debug_enable") == false then
        config.set("useHardwareRenderer", false)
        config.set("debug_enable", true)
        print("Please quit and relaunch CraftOS-PC and try again.")
        return
    end
    term.setTextColor(colors.yellow)
    term.clear()
    term.setCursorPos(1, 1)
    print("This program will test which renderer is best for your system. It will take about a minute to complete, and you will need to quit and relaunch CraftOS-PC mid-way through. It is recommended you close all other applications before starting to accurately gauge the performance of your system.\n\nPress enter to continue.")
    read()
    local score = runTests()
    local file = fs.open(".benchmark_results", "w")
    file.write(score)
    file.close()
    if fs.exists("/startup.lua") then fs.move("/startup.lua", "/startup.f8CyMWNJ.lua") end
    file = fs.open("/startup.lua", "w")
    file.write("shell.run(\"" .. shell.getRunningProgram() .. " hardware\")")
    file.close()
    config.set("useHardwareRenderer", true)
    term.setBackgroundColor(colors.black)
    term.setTextColor(colors.lightBlue)
    term.clear()
    term.setCursorPos(1, 1)
    print("The software rendering portion of the test is complete. CraftOS-PC will now quit. Re-open CraftOS-PC to complete the test.\n\nPress enter to continie.")
    read()
    os.shutdown()
else
    local score = runTests()
    local file = fs.open(".benchmark_results", "r")
    if file == nil then
        printError("Could not open results for software test, did you run them before this?")
        return
    end
    local swscore = tonumber(file.readLine())
    file.close()
    fs.delete(".benchmark_results")
    fs.delete("/startup.lua")
    if fs.exists("/startup.f8CyMWNJ.lua") then fs.move("/startup.f8CyMWNJ.lua", "/startup.lua") end
    term.clear()
    term.setCursorPos(1, 1)
    term.setTextColor(colors.yellow)
    print("Results:")
    term.setTextColor(swscore > score and colors.green or colors.red)
    print("Software renderer: " .. swscore)
    term.setTextColor(swscore < score and colors.green or colors.red)
    print("Hardware renderer: " .. score)
    term.setTextColor(colors.lightBlue)
    print("It is recommended that you use the " .. (swscore > score and "software" or "hardware") .. " renderer.")
    term.setTextColor(colors.yellow)
    write("Would you like to set CraftOS-PC to use this renderer by default? (y/N) ")
    term.setTextColor(colors.white)
    local answer = read()
    if answer:sub(1, 1):upper() == "Y" then config.set("useHardwareRenderer", swscore < score) 
    else config.set("useHardwareRenderer", false) end
end