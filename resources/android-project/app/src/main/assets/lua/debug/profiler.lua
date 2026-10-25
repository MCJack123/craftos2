-- SPDX-FileCopyrightText: 2019 JackMacWindows
--
-- SPDX-License-Identifier: MPL-2.0

multishell.setTitle(multishell.getCurrent(), "Profiler")
local ok, err = pcall(function()
local w, h = term.getSize()
local header = window.create(term.current(), 1, 1, w, 2)
local viewport = window.create(term.current(), 1, 3, w, h - 2)
local body
local widths = {{"#", 5, "count"}, {"Source", math.ceil((w - 11) / 2), "source"}, {"Function", math.floor((w - 11) / 2), "func"}, {"Time", 6, "time"}}
local profilingTime, tm
local scrollPos, scrollSize = 1, 1

local function formatTime(n) return string.format("%i:%02i:%02i", math.floor(n / 3600), math.floor(n / 60) % 60, n % 60) end

local function updateHeader()
    header.setBackgroundColor(colors.gray)
    header.clear()
    header.setCursorPos(2, 1)
    header.blit(" " .. string.char(7) .. " ", profilingTime == nil and "eee" or "000", profilingTime == nil and "000" or "eee")
    local timestr = "0:00:00"
    if profilingTime ~= nil then timestr = formatTime((os.epoch() - profilingTime) / 1000) end
    header.setBackgroundColor(colors.gray)
    header.setTextColor(colors.white)
    header.setCursorPos(w - #timestr, 1)
    header.write(timestr)
    local i = 1
    for k,v in ipairs(widths) do
        header.setCursorPos(i, 2)
        header.write(v[1])
        i = i + v[2]
    end
end

local sortFunctions = {
    [0] = function(a, b) return a.count > b.count end,
    function(a, b) return a.source > b.source end,
    function(a, b) return a.func > b.func end,
    function(a, b) return a.time > b.time end,
    function(a, b) return a.count < b.count end,
    function(a, b) return a.source < b.source end,
    function(a, b) return a.func < b.func end,
    function(a, b) return a.time < b.time end,
}

local sorter = 1

local function parseProfile()
    local lines = {}
    local profile = debugger.profile()
    local cw = 2
    local tw = 4
    for k,v in pairs(profile) do for l,w in pairs(v) do 
        table.insert(lines, {source = k, func = l, count = w.count, time = w.time}) 
        cw = math.max(math.floor(math.log10(w.count)) + 2, cw)
        tw = math.max(math.floor(math.log10(w.time)) + 2, tw)
    end end
    widths[1][2] = cw
    widths[2][2] = math.floor((w - (cw + tw)) / 2)
    widths[3][2] = math.ceil((w - (cw + tw)) / 2)
    widths[4][2] = tw
    body = window.create(viewport, 1, 1, w, #lines)
    scrollPos = 1
    scrollSize = #lines
    table.sort(lines, sortFunctions[sorter])
    for k,v in ipairs(lines) do
        local i = 1
        for l,w in ipairs(widths) do
            body.setCursorPos(i, k)
            if w[3] == "source" and #v.source > w[2]-1 then body.write(string.sub(fs.getName(v.source), 1, w[2]-1))
            else body.write(string.sub(tostring(v[w[3]]), 1, w[2]-1)) end
            i = i + w[2]
        end
    end
end

updateHeader()

while true do
    local ev = {os.pullEvent()}
    if ev[1] == "mouse_click" and ev[2] == 1 then
        if ev[4] == 1 and ev[3] > 1 and ev[3] < 5 then
            if profilingTime then
                os.cancelTimer(tm)
                profilingTime = nil
                tm = nil
                debugger.startProfiling(false)
                parseProfile()
            else
                profilingTime = os.epoch()
                tm = os.startTimer(1)
                debugger.startProfiling(true)
                if body then body.setVisible(false) end
                viewport.clear()
            end
            updateHeader()
        elseif ev[4] == 2 then
            if ev[3] <= widths[1][2] then sorter = 0 + (bit.band(sorter, 3) == 0 and bit.bxor(bit.band(sorter, 4), 4) or 0)
            elseif ev[3] > widths[1][2] and ev[3] <= widths[1][2] + widths[2][2] then sorter = 1 + (bit.band(sorter, 3) == 1 and bit.bxor(bit.band(sorter, 4), 4) or 0)
            elseif ev[3] > widths[1][2] + widths[2][2] and ev[3] <= widths[1][2] + widths[2][2] + widths[3][2] then sorter = 2 + (bit.band(sorter, 3) == 2 and bit.bxor(bit.band(sorter, 4), 4) or 0)
            else sorter = 3 + (bit.band(sorter, 3) == 3 and bit.bxor(bit.band(sorter, 4), 4) or 0) end
            parseProfile()
        end
    elseif ev[1] == "mouse_scroll" and ev[4] > 2 then
        if ev[2] == -1 and scrollPos < 1 then scrollPos = scrollPos + 1
        elseif ev[2] == 1 and scrollPos > h - 1 - scrollSize then scrollPos = scrollPos - 1 end
        if body then body.reposition(1, scrollPos) end
    elseif ev[1] == "timer" and ev[2] == tm then
        updateHeader()
        parseProfile()
        tm = os.startTimer(1)
    elseif ev[1] == "term_resize" then
        w, h = term.getSize()
        header = window.create(term.current(), 1, 1, w, 2)
        viewport = window.create(term.current(), 1, 3, w, h - 2)
        widths = {{"#", 5, "count"}, {"Source", math.ceil((w - 11) / 2), "source"}, {"Function", math.floor((w - 11) / 2), "func"}, {"Time", 6, "time"}}
        updateHeader()
    elseif ev[1] == "key" then
        if ev[2] == keys.enter then
            if profilingTime then
                os.cancelTimer(tm)
                profilingTime = nil
                tm = nil
                debugger.startProfiling(false)
                parseProfile()
            else
                profilingTime = os.epoch()
                tm = os.startTimer(1)
                debugger.startProfiling(true)
                if body then body.setVisible(false) end
                viewport.clear()
            end
            updateHeader()
        elseif ev[2] == keys.up and scrollPos < 1 then 
            scrollPos = scrollPos + 1
            if body then body.reposition(1, scrollPos) end
        elseif ev[2] == keys.down and scrollPos > h - 1 - scrollSize then 
            scrollPos = scrollPos - 1 
            if body then body.reposition(1, scrollPos) end
        end
    end
end
end)

if not ok then printError(err) end
while os.pullEvent() do end