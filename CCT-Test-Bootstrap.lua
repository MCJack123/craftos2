if not debug then
    config.set("debug_enable", true)
    os.reboot()
end
for _,v in ipairs(fs.list("/")) do if not fs.isReadOnly(v) then fs.delete(v) end end
shell.setDir("/test-rom")
shell.run("mcfly")
os.shutdown(_G.failed_tests)
