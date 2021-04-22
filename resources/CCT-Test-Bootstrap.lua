if not debug then
    config.set("debug_enable", true)
    config.add("http_blacklist", "$private")
    os.reboot()
end
if ... == "debugger" then
    periphemu.create("left", "debugger")
    peripheral.call("left", "break")
end
for _,v in ipairs(fs.list("/")) do if not fs.isReadOnly(v) then fs.delete(v) end end
_G._CCPC_FIRST_RUN = nil
_G._CCPC_UPDATED_VERSION = nil
local logfile = io.open("test-log.txt", "w")
io.output(logfile)
shell.run("/test-rom/mcfly /test-rom/spec")
logfile:close()
os.shutdown(_G.failed_tests)
