-- SPDX-FileCopyrightText: 2019 JackMacWindows
--
-- SPDX-License-Identifier: MPL-2.0

multishell.setTitle(multishell.getCurrent(), "Console  ")
local w, h = term.getSize()
local win = window.create(term.current(), 1, 1, w, 9000)
local top = 1
local bottom = 1
local scrolling = false
local old = term.redirect(win)
while true do
    local ev, p1 = os.pullEventRaw()
    if ev == "debugger_print" then
        local lines = print(p1)
        bottom = math.min(bottom + lines, 9000)
        if not scrolling and bottom > h + 1 and top < 9000 - h then
            top = bottom - h
            win.reposition(1, 2-top)
        end
    elseif ev == "mouse_scroll" then
        if (p1 == -1 and top > 1) or (p1 == 1 and top < 9000 - h) then
            top = math.min(top + p1, 9000)
            scrolling = top + h - 1 ~= bottom
            multishell.setTitle(multishell.getCurrent(), scrolling and "Console \7" or "Console  ")
            win.reposition(1, 2-top)
        end
    elseif ev == "term_resize" then
        w, h = old.getSize()
        win.reposition(1, 2-top, old.getSize(), 9000)
    end
end