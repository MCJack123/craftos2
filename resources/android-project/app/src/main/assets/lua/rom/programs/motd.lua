-- SPDX-FileCopyrightText: 2020 The CC: Tweaked Developers
--
-- SPDX-License-Identifier: MPL-2.0

if _CCPC_FIRST_RUN then
    print([[Welcome to CraftOS-PC! This is the ComputerCraft shell prompt, where you can run programs.
* Type "programs" to see the programs you can run.
* Type "help <program>" to see help for a specific program.
* Read the documentation at https://www.craftos-pc.cc/docs/.
* Report bugs to https://www.craftos-pc.cc/bugreport.
]])
elseif _CCPC_UPDATED_VERSION then
    print("CraftOS-PC has been updated to " .. _HOST:match("CraftOS%-PC [%a]* ?(v[%d%.]+)") .. ". To see the new changes, type \"help whatsnew\".")
else
    local date = os.date("*t")
    if date.year == 2021 then
        print("Thank you SquidDev!")
    elseif date.month == 1 and date.day == 1 then
        print("Happy new year!")
    elseif date.month == 12 and date.day == 24 then
        print("Merry X-mas!")
    elseif date.month == 10 and date.day == 31 then
        print("OOoooOOOoooo! Spooky!")
    elseif date.month == 4 and date.day == 28 then
        print("Ed Balls")
    elseif date.month == 9 and date.day == 8 then
        print("Happy birthday, CraftOS-PC!")
    else
        local tMotd = {}

        for sPath in string.gmatch(settings.get("motd.path"), "[^:]+") do
            if fs.exists(sPath) then
                for sLine in io.lines(sPath) do
                    table.insert(tMotd, sLine)
                end
            end
        end

        if mobile and mobile.purchasePlugin then
            table.insert(tMotd, "Want to add more features to CraftOS-PC? Buy additional plugins using the plugins command.")
        end

        if #tMotd == 0 then
            print("missingno")
        else
            print(tMotd[math.random(1, #tMotd)])
        end
    end
end
