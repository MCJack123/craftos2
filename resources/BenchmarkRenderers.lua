local function runTests()
    -- Test characters
    local w, h = term.getSize()
    local canBenchmark = term.benchmark ~= nil
    local count = 0
    local start = os.epoch "utc"
    if canBenchmark then term.benchmark() end
    pcall(function()
        while true do
            term.setCursorPos(math.random(1, w), math.random(1, h))
            term.setBackgroundColor(2^math.random(0, 15))
            term.setTextColor(2^math.random(0, 15))
            term.write(string.char(math.random(0, 255)))
            count = count + 1
        end
    end)
    local frames = canBenchmark and term.benchmark() or 0
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
    if canBenchmark then term.benchmark() end
    pcall(function()
        while true do
            term.setPixel(math.random(0, w), math.random(0, h), math.random(0, 255))
            count = count + 1
        end
    end)
    frames = canBenchmark and term.benchmark() or 0
    time = os.epoch "utc" - start
    term.clear()
    for i = 0, 15 do term.setPaletteColor(i, term.nativePaletteColor(2^i)) end
    term.setGraphicsMode(0)
    if canBenchmark then print("Rendered " .. frames .. " frames in " .. time .. " ms (" .. (frames / (time / 1000)) .. " fps)") end
    print("Drew " .. count .. " pixels (" .. (count / (time / 1000)) .. " pps)")
    return score + (frames / (time / 1000))
end

local mode = ...
if mode == "software" then
    local score = runTests()
    local file = fs.open(".benchmark_results", "w")
    file.write(score)
    file.close()
    os.shutdown()
elseif mode == "hardware" then
    local score = runTests()
    local file = fs.open(".benchmark_results", "r")
    if file == nil then
        printError("Could not open results for software test, did you run them before this?")
        return
    end
    local swscore = tonumber(file.readLine())
    file.close()
    fs.delete(".benchmark_results")
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
    print("Press return to exit.")
    read()
    os.shutdown()
else printError("This script must be used with the BenchmarkRenderers shell/batch script.") end