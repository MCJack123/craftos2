-- SPDX-FileCopyrightText: 2019 JackMacWindows
--
-- SPDX-License-Identifier: MPL-2.0

if term.getGraphicsMode == nil then error("This requires CraftOS-PC v1.2 or later.") end

term.setGraphicsMode(true)
for i = 0, 15 do
    paintutils.drawFilledBox(i*4, 0, i*4+3, 3, bit.blshift(1, i))
end
local c = colors.white
paintutils.drawFilledBox(302, 0, 305, 3, c)
while true do
    local ev, ch, x, y = os.pullEvent()
    if ev == "mouse_click" or ev == "mouse_drag" then
        if y < 4 then
            if x < 64 then c = bit.blshift(1, math.floor(x / 4)) end
            paintutils.drawFilledBox(302, 0, 305, 3, c)
        else
            term.setPixel(x, y, c)
        end
    elseif ev == "char" and ch == "q" then break end
end
term.clear()
term.setGraphicsMode(false)
term.setBackgroundColor(colors.black)
term.setTextColor(colors.white)
term.clear()
term.setCursorPos(1, 1)