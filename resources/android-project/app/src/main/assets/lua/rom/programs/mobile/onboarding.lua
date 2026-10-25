-- PrimeUI by JackMacWindows
-- Public domain/CC0

local expect = require "cc.expect".expect

-- Initialization code
local PrimeUI = {}
do
    local coros = {}
    local restoreCursor

    --- Adds a task to run in the main loop.
    ---@param func function The function to run, usually an `os.pullEvent` loop
    function PrimeUI.addTask(func)
        expect(1, func, "function")
        local t = {coro = coroutine.create(func)}
        coros[#coros+1] = t
        _, t.filter = coroutine.resume(t.coro)
    end

    --- Sends the provided arguments to the run loop, where they will be returned.
    ---@param ... any The parameters to send
    function PrimeUI.resolve(...)
        coroutine.yield(coros, ...)
    end

    --- Clears the screen and resets all components. Do not use any previously
    --- created components after calling this function.
    function PrimeUI.clear()
        -- Reset the screen.
        term.setCursorPos(1, 1)
        term.setCursorBlink(false)
        term.setBackgroundColor(colors.black)
        term.setTextColor(colors.white)
        term.clear()
        -- Reset the task list and cursor restore function.
        coros = {}
        restoreCursor = nil
    end

    --- Sets or clears the window that holds where the cursor should be.
    ---@param win window|nil The window to set as the active window
    function PrimeUI.setCursorWindow(win)
        expect(1, win, "table", "nil")
        restoreCursor = win and win.restoreCursor
    end

    --- Gets the absolute position of a coordinate relative to a window.
    ---@param win window The window to check
    ---@param x number The relative X position of the point
    ---@param y number The relative Y position of the point
    ---@return number x The absolute X position of the window
    ---@return number y The absolute Y position of the window
    function PrimeUI.getWindowPos(win, x, y)
        if win == term then return x, y end
        while win ~= term.native() and win ~= term.current() do
            if not win.getPosition then return x, y end
            local wx, wy = win.getPosition()
            x, y = x + wx - 1, y + wy - 1
            _, win = debug.getupvalue(select(2, debug.getupvalue(win.isColor, 1)), 1) -- gets the parent window through an upvalue
        end
        return x, y
    end

    --- Runs the main loop, returning information on an action.
    ---@return any ... The result of the coroutine that exited
    function PrimeUI.run()
        while true do
            -- Restore the cursor and wait for the next event.
            if restoreCursor then restoreCursor() end
            local ev = table.pack(os.pullEvent())
            -- Run all coroutines.
            for _, v in ipairs(coros) do
                if v.filter == nil or v.filter == ev[1] then
                    -- Resume the coroutine, passing the current event.
                    local res = table.pack(coroutine.resume(v.coro, table.unpack(ev, 1, ev.n)))
                    -- If the call failed, bail out. Coroutines should never exit.
                    if not res[1] then error(res[2], 2) end
                    -- If the coroutine resolved, return its values.
                    if res[2] == coros then return table.unpack(res, 3, res.n) end
                    -- Set the next event filter.
                    v.filter = res[2]
                end
            end
        end
    end
end

--- Creates a clickable button on screen with text.
---@param win window The window to draw on
---@param x number The X position of the button
---@param y number The Y position of the button
---@param text string The text to draw on the button
---@param action function|string A function to call when clicked, or a string to send with a `run` event
---@param fgColor color|nil The color of the button text (defaults to white)
---@param bgColor color|nil The color of the button (defaults to light gray)
---@param clickedColor color|nil The color of the button when clicked (defaults to gray)
function PrimeUI.button(win, x, y, text, action, fgColor, bgColor, clickedColor)
    expect(1, win, "table")
    expect(2, x, "number")
    expect(3, y, "number")
    expect(4, text, "string")
    expect(5, action, "function", "string")
    fgColor = expect(6, fgColor, "number", "nil") or colors.white
    bgColor = expect(7, bgColor, "number", "nil") or colors.gray
    clickedColor = expect(8, clickedColor, "number", "nil") or colors.lightGray
    -- Draw the initial button.
    win.setCursorPos(x, y)
    win.setBackgroundColor(bgColor)
    win.setTextColor(fgColor)
    win.write(" " .. text .. " ")
    -- Get the screen position and add a click handler.
    PrimeUI.addTask(function()
        local buttonDown = false
        while true do
            local event, button, clickX, clickY = os.pullEvent()
            local screenX, screenY = PrimeUI.getWindowPos(win, x, y)
            if event == "mouse_click" and button == 1 and clickX >= screenX and clickX < screenX + #text + 2 and clickY == screenY then
                -- Initiate a click action (but don't trigger until mouse up).
                buttonDown = true
                -- Redraw the button with the clicked background color.
                win.setCursorPos(x, y)
                win.setBackgroundColor(clickedColor)
                win.setTextColor(fgColor)
                win.write(" " .. text .. " ")
            elseif event == "mouse_up" and button == 1 and buttonDown then
                -- Finish a click event.
                if clickX >= screenX and clickX < screenX + #text + 2 and clickY == screenY then
                    -- Trigger the action.
                    if type(action) == "string" then PrimeUI.resolve("button", action)
                    else action() end
                end
                -- Redraw the original button state.
                win.setCursorPos(x, y)
                win.setBackgroundColor(bgColor)
                win.setTextColor(fgColor)
                win.write(" " .. text .. " ")
            end
        end
    end)
end

--- Draws a line of text, centering it inside a box horizontally.
---@param win window The window to draw on
---@param x number The X position of the left side of the box
---@param y number The Y position of the box
---@param width number The width of the box to draw in
---@param text string The text to draw
---@param fgColor color|nil The color of the text (defaults to white)
---@param bgColor color|nil The color of the background (defaults to black)
function PrimeUI.centerLabel(win, x, y, width, text, fgColor, bgColor)
    expect(1, win, "table")
    expect(2, x, "number")
    expect(3, y, "number")
    expect(4, width, "number")
    expect(5, text, "string")
    fgColor = expect(6, fgColor, "number", "nil") or colors.white
    bgColor = expect(7, bgColor, "number", "nil") or colors.black
    assert(#text <= width, "string is too long")
    win.setCursorPos(x + math.floor((width - #text) / 2), y)
    win.setTextColor(fgColor)
    win.setBackgroundColor(bgColor)
    win.write(text)
end

--- Creates a text box that wraps text and can have its text modified later.
---@param win window The parent window of the text box
---@param x number The X position of the box
---@param y number The Y position of the box
---@param width number The width of the box
---@param height number The height of the box
---@param text string The initial text to draw
---@param fgColor color|nil The color of the text (defaults to white)
---@param bgColor color|nil The color of the background (defaults to black)
---@return function redraw A function to redraw the window with new contents
function PrimeUI.textBox(win, x, y, width, height, text, fgColor, bgColor)
    expect(1, win, "table")
    expect(2, x, "number")
    expect(3, y, "number")
    expect(4, width, "number")
    expect(5, height, "number")
    expect(6, text, "string")
    fgColor = expect(7, fgColor, "number", "nil") or colors.white
    bgColor = expect(8, bgColor, "number", "nil") or colors.black
    -- Create the box window.
    local box = window.create(win, x, y, width, height)
    -- Override box.getSize to make print not scroll.
    function box.getSize()
        return width, math.huge
    end
    -- Define a function to redraw with.
    local function redraw(_text)
        expect(1, _text, "string")
        -- Set window parameters.
        box.setBackgroundColor(bgColor)
        box.setTextColor(fgColor)
        box.clear()
        box.setCursorPos(1, 1)
        -- Redirect and draw with `print`.
        local old = term.redirect(box)
        print(_text)
        term.redirect(old)
    end
    redraw(text)
    return redraw
end

if mobile then mobile.openKeyboard(false) end
local screens = {}

screens[#screens+1] = function()
    local w, h = term.getSize()
    PrimeUI.clear()
    local lt, tt = math.floor(h / 3), math.floor(h / 2)
    if h < 25 then lt, tt = math.floor(h / 4), math.floor(h / 3) + 2 end
    if w < 25 then
        PrimeUI.centerLabel(term.current(), 1, lt - 1, w, "Welcome to", colors.yellow)
        PrimeUI.centerLabel(term.current(), 1, lt, w, "CraftOS-PC", colors.yellow)
    else PrimeUI.centerLabel(term.current(), 1, lt, w, "Welcome to CraftOS-PC", colors.yellow) end
    PrimeUI.textBox(term.current(), 5, tt, w - 8, (h - tt) - 4, "Before using CraftOS-PC, read these basic tips to understand how to navigate it efficiently.")
    PrimeUI.button(term.current(), 4, h - 2, "Skip", "skip")
    PrimeUI.button(term.current(), w - 8, h - 2, "Next", "next")
end

screens[#screens+1] = function()
    local w, h = term.getSize()
    PrimeUI.clear()
    local lt, tt = math.floor(h / 3), math.floor(h / 2)
    if h < 25 then lt, tt = math.floor(h / 4), math.floor(h / 3) + 2 end
    PrimeUI.centerLabel(term.current(), 1, lt, w, "Navigation Bar", colors.yellow)
    PrimeUI.textBox(term.current(), 5, tt, w - 8, (h - tt) - 4, "Tap the screen with two fingers to open the navigation bar.\
\
< >   Change screen\
 \xD7    Close screen\
[#]   Toggle keyboard")
    PrimeUI.button(term.current(), 4, h - 2, "Skip", "skip")
    PrimeUI.button(term.current(), w - 8, h - 2, "Next", "next")
end

screens[#screens+1] = function()
    local w, h = term.getSize()
    PrimeUI.clear()
    local lt, tt = math.floor(h / 3), math.floor(h / 2)
    if h < 25 then lt, tt = math.floor(h / 4), math.floor(h / 3) + 2 end
    PrimeUI.centerLabel(term.current(), 1, lt, w, "Gestures", colors.yellow)
    PrimeUI.textBox(term.current(), 5, tt, w - 8, (h - tt) - 4, [[
Swipe with two fingers in any direction to emulate an arrow key.

Hold two fingers on any edge to hold an arrow key in that direction.]])
    PrimeUI.button(term.current(), 4, h - 2, "Skip", "skip")
    PrimeUI.button(term.current(), w - 8, h - 2, "Next", "next")
end

screens[#screens+1] = function()
    local w, h = term.getSize()
    PrimeUI.clear()
    if mobile then mobile.openKeyboard(true) end
    local lt, tt = 2, 4
    PrimeUI.centerLabel(term.current(), 1, lt, w, "Keyboard", colors.yellow)
    PrimeUI.textBox(term.current(), 5, tt, w - 8, (h - tt) - 3, "Use the keyboard to type commands. Some extra buttons are placed above the keyboard.\
\
Ctrl   Toggles Ctrl\
Alt    Toggles Alt\
 \x1A|    Tab key")
    PrimeUI.button(term.current(), 4, h - 2, "Skip", "skip")
    PrimeUI.button(term.current(), w - 8, h - 2, "Next", "next")
end

screens[#screens+1] = function()
    local w, h = term.getSize()
    PrimeUI.clear()
    if mobile then mobile.openKeyboard(true) end
    local lt, tt = 2, 4
    PrimeUI.centerLabel(term.current(), 1, lt, w, "Keyboard", colors.yellow)
    PrimeUI.textBox(term.current(), 5, tt, w - 8, (h - tt) - 3, "Red keys must be held to activate. Release after two seconds to send an action.\
\
(-)    Terminate\
(')    Shut down\
 \x11     Reboot", colors.red)
    PrimeUI.button(term.current(), 4, h - 2, "Skip", "skip")
    PrimeUI.button(term.current(), w - 8, h - 2, "Next", "next")
end

if not mobile or mobile.listPlugins then
    screens[#screens+1] = function()
        local w, h = term.getSize()
        PrimeUI.clear()
        if mobile then mobile.openKeyboard(true) end
        local lt, tt = 2, 4
        PrimeUI.centerLabel(term.current(), 1, lt, w, "Keyboard", colors.yellow)
        PrimeUI.textBox(term.current(), 5, tt, w - 8, (h - tt) - 3, "Navigation keys are available by pressing the three dots button.\
\
\x1B\x18\x19\x1A   Arrow keys\
\x18  \x19   Page Up/Down\
  n\x8F   Paste\
  \xAF")
        PrimeUI.button(term.current(), 4, h - 2, "Skip", "skip")
        PrimeUI.button(term.current(), w - 8, h - 2, "Next", "next")
    end

    screens[#screens+1] = function()
        local w, h = term.getSize()
        PrimeUI.clear()
        if mobile then mobile.openKeyboard(false) end
        local lt, tt = math.floor(h / 3), math.floor(h / 2)
        if h < 25 then lt, tt = math.floor(h / 4), math.floor(h / 3) + 2 end
        PrimeUI.centerLabel(term.current(), 1, lt, w, "Plugins", colors.yellow)
        PrimeUI.textBox(term.current(), 5, tt, w - 8, (h - tt) - 4, "Plugins can extend the functionality of CraftOS-PC using in-app purchases. Run the 'plugins' command to view the available plugins.")
        PrimeUI.button(term.current(), 4, h - 2, "Skip", "skip")
        PrimeUI.button(term.current(), w - 8, h - 2, "Next", "next")
    end
end

screens[#screens+1] = function()
    local w, h = term.getSize()
    PrimeUI.clear()
    if mobile then mobile.openKeyboard(false) end
    local lt, tt = math.floor(h / 4), math.floor(h / 3)
    PrimeUI.centerLabel(term.current(), 1, lt, w, "Get Started", colors.yellow)
    PrimeUI.textBox(term.current(), 5, tt, w - 8, (h - tt) - 4, "For more help using CraftOS-PC, use the 'help' program at the shell prompt, or visit https://www.craftos-pc.cc.\n\nTo learn more about the Lua programming language, visit https://www.lua.org.\n\nFor info on the APIs available, visit https://tweaked.cc.")
    PrimeUI.button(term.current(), w - 10, h - 2, "Finish", "next")
end

for i = 1, #screens do
    while true do
        screens[i]()
        PrimeUI.addTask(function()
            os.pullEvent("term_resize")
            PrimeUI.resolve("resize")
        end)
        local event, action = PrimeUI.run()
        if event == "button" then
            if action == "skip" then
                PrimeUI.clear()
                return
            else break end
        end
    end
end
PrimeUI.clear()
if mobile then mobile.openKeyboard(true) end
term.setTextColor(colors.yellow)
print(os.version())
term.setTextColor(colors.white)
