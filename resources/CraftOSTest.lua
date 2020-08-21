-- Tests compliance of CraftOS APIs
if not _HEADLESS then term.clear() end
term.setCursorPos(1, 1)
term.setTextColor(colors.lightBlue)
print("CraftOSTest 1.8")
term.setTextColor(colors.white)
if os.version() ~= "CraftOS 1.8" then error("This test is for CraftOS 1.8.") end

local api_tests = {}
local api = nil
local logfile, err = fs.open("CraftOSTest.log", "w")
if logfile == nil then
	term.setTextColor(colors.red)
	print("!!! Could not open log file: " .. err)
	if _HEADLESS then os.shutdown(1) end
end
local main_thread = coroutine.running()

local function compare(a, b)
	if type(a) ~= type(b) then return false
	elseif type(a) == "number" then return math.abs(a - b) < 0.000001
	elseif type(a) == "table" then
		if #a ~= #b then return false end
		for k,v in pairs(a) do if b[k] == nil or not compare(v, b[k]) then return false end end
		return true
	else return a == b end
end

local function tostr(v)
	if type(v) == "boolean" then return v and "true" or "false"
	elseif v == nil then return "nil"
	elseif type(v) == "string" then return "\"" .. v .. "\""
	elseif type(v) == "number" then return tostring(v)
	elseif type(v) == "table" then 
		local ok, res = pcall(textutils.serialize, v)
		if ok then return res else return "[table]" end
	elseif type(v) == "thread" and v == main_thread then return "[main thread]"
	else return "[" .. tostring(v) .. "]" end
end

local function testStart(tapi)
	term.setTextColor(colors.yellow)
	write("=>  ")
	print("Testing " .. tapi .. " API...")
	logfile.writeLine("=>  Testing " .. tapi .. " API...")
	term.setTextColor(colors.white)
	api = tapi
	if _ENV[api] == nil then
		term.setTextColor(colors.red)
		print("[x] API " .. api .. " does not exist!")
		logfile.writeLine("[x] API " .. api .. " does not exist!")
		term.setTextColor(colors.white)
		return
	end
	api_tests[api] = 0
end

local function testEnd()
	if api_tests[api] == nil then return end
	if api_tests[api] == 0 then
		term.setTextColor(colors.green)
		write("=>  ")
		print("Test of " .. api .. " API succeeded")
		logfile.writeLine("=>  Test of " .. api .. " API succeeded")
		term.setTextColor(colors.white)
	else
		term.setTextColor(colors.red)
		write("=>  ")
		print("Test of " .. api .. " API failed " .. api_tests[api] .. " times")
		logfile.writeLine("=>  Test of " .. api .. " API failed " .. api_tests[api] .. " times")
		term.setTextColor(colors.white)
	end
	logfile.flush()
	api = nil
end

local function test(name, expected, ...)
	if api_tests[api] == nil then return end
	if _ENV[api][name] == nil then
		term.setTextColor(colors.red)
		print("[x] " .. api .. "." .. name .. " does not exist!")
		logfile.writeLine("[x] " .. api .. "." .. name .. " does not exist!")
		term.setTextColor(colors.white)
		api_tests[api] = api_tests[api] + 1
		return false
	end
	local res = {pcall(_ENV[api][name], ...)}
	if not res[1] then
		term.setTextColor(colors.red)
		print("[x] " .. api .. "." .. name .. " threw an error: " .. res[2])
		logfile.writeLine("[x] " .. api .. "." .. name .. " threw an error: " .. res[2])
		term.setTextColor(colors.white)
		api_tests[api] = api_tests[api] + 1
		return false
	end
	table.remove(res, 1)
	local good = true
	if type(expected) == "table" then good = compare(res, expected) else
		res = res[1]
		good = res == expected 
	end
	if not good then
		local line = ({pcall(function() error("", 3) end)})[2]
		term.setTextColor(colors.red)
		print("[x] " .. line .. api .. "." .. name .. " returned " .. tostr(res) .. " (expected " .. tostr(expected) .. ")")
		logfile.writeLine("[x] " .. line .. api .. "." .. name .. " returned " .. tostr(res) .. " (expected " .. tostr(expected) .. ")")
		term.setTextColor(colors.white)
		api_tests[api] = api_tests[api] + 1
	end
	return good
end

local function testValue(name, expected)
	if api_tests[api] == nil then return end
	local res = _ENV[api][name]
	if not compare(res, expected) then
		term.setTextColor(colors.red)
		print("[x] " .. api .. "." .. name .. " returned " .. tostr(res) .. " (expected " .. tostr(expected) .. ")")
		logfile.writeLine("[x] " .. api .. "." .. name .. " returned " .. tostr(res) .. " (expected " .. tostr(expected) .. ")")
		term.setTextColor(colors.white)
		api_tests[api] = api_tests[api] + 1
		return false
	end
	return true
end

local function testLocal(name, res, expected)
	if api_tests[api] == nil then return end
	if not compare(res, expected) then
		term.setTextColor(colors.red)
		print("[x] " .. name .. " returned " .. tostr(res) .. " (expected " .. tostr(expected) .. ")")
		logfile.writeLine("[x] " .. name .. " returned " .. tostr(res) .. " (expected " .. tostr(expected) .. ")")
		term.setTextColor(colors.white)
		api_tests[api] = api_tests[api] + 1
		return false
	end
	return true
end

local function call(name, ...)
	if api_tests[api] == nil then return end
	if _ENV[api][name] == nil then
		term.setTextColor(colors.red)
		print("[x] " .. api .. "." .. name .. " does not exist!")
		logfile.writeLine("[x] " .. api .. "." .. name .. " does not exist!")
		term.setTextColor(colors.white)
		api_tests[api] = api_tests[api] + 1
		return false
	end
	local res = {pcall(_ENV[api][name], ...)}
	if not res[1] then
		term.setTextColor(colors.red)
		print("[x] " .. api .. "." .. name .. " threw an error: " .. res[2])
		logfile.writeLine("[x] " .. api .. "." .. name .. " threw an error: " .. res[2])
		term.setTextColor(colors.white)
		api_tests[api] = api_tests[api] + 1
		return
	end
	table.remove(res, 1)
	return table.unpack(res)
end

local function callLocal(name, f, ...)
	if api_tests[api] == nil then return end
	local res = {pcall(f, ...)}
	if not res[1] then
		term.setTextColor(colors.red)
		print("[x] " .. name .. " threw an error: " .. res[2])
		logfile.writeLine("[x] " .. name .. " threw an error: " .. res[2])
		term.setTextColor(colors.white)
		api_tests[api] = api_tests[api] + 1
		return
	end
	table.remove(res, 1)
	return table.unpack(res)
end

testStart "bit"
	test("blshift", 96, 6, 4)
	test("brshift", -0x1000, 0xFFFF0000, 4)
	test("blogic_rshift", 0x0FFFF000, 0xFFFF0000, 4)
	test("bxor", 2, 7, 5)
	test("bor", 13, 5, 8)
	test("band", 2, 14, 3)
	test("bnot", 0x0F0FF0F0, 0xF0F00F0F)
	testValue("tobits", nil)
	testValue("tonumb", nil)
testEnd()

testStart "colors"
	testValue("white", 0x1)
	testValue("orange", 0x2)
	testValue("magenta", 0x4)
	testValue("lightBlue", 0x8)
	testValue("yellow", 0x10)
	testValue("lime", 0x20)
	testValue("pink", 0x40)
	testValue("gray", 0x80)
	testValue("lightGray", 0x100)
	testValue("cyan", 0x200)
	testValue("purple", 0x400)
	testValue("blue", 0x800)
	testValue("brown", 0x1000)
	testValue("green", 0x2000)
	testValue("red", 0x4000)
	testValue("black", 0x8000)
	test("combine", colors.white, colors.white, colors.white)
	test("subtract", colors.orange, 0x3, colors.white)
	test("test", true, 0x3, colors.white)
testEnd()

testStart "colours"
	testValue("white", 0x1)
	testValue("black", 0x8000)
	testValue("gray", nil)
	testValue("lightGray", nil)
	testValue("grey", 0x80)
	testValue("lightGrey", 0x100)
testEnd()

testStart "coroutine"
	local coro
	local function coro_func(v1, v2)
		testLocal("variable 1", v1, "test")
		testLocal("variable 2", v2, 17)
		test("status", "running", coro)
		test("running", coro)
		test("yield", 50, 40)
		return "done"
	end
	coro = call("create", coro_func)
	testLocal("type(coro)", type(coro), "thread")
	test("status", "suspended", coro)
	test("resume", {true, 40}, coro, "test", 17)
	test("status", "suspended", coro)
	test("resume", {true, "done"}, coro, 50)
	test("status", "dead", coro)
	-- TODO: add wrap tests
testEnd()

testStart "fs"
	test("list", {{"apis", "autorun", "help", "modules", "motd.txt", "programs", "startup.lua"}}, "/rom")
	test("exists", true, "/rom/programs/shell.lua")
	test("exists", true, "/rom/programs")
	test("isDir", true, "/rom/programs")
	test("isDir", false, "/rom/programs/shell.lua")
	test("isReadOnly", true, "/rom/programs")
	test("isReadOnly", true, "/rom/programs/shell.lua")
	test("isReadOnly", false, "/")
	test("getName", "shell.lua", "/rom/programs/shell.lua")
	test("getDir", "rom/programs", "/rom/programs/shell.lua")
	test("getDrive", "rom", "/rom/programs/shell.lua")
	local s = call("getSize", "/rom/apis/keys.lua")
	if s ~= 2429 and s ~= 2492 then testLocal("fs.getSize", {2491, 2120}, s) end -- CRLF or LF
	call("makeDir", "test_dir")
	test("isDir", true, "test_dir")
	call("delete", "test_dir")
	local file = call("open", "test_file.txt", "w")
	callLocal("file.writeLine", file.writeLine, "This is a test")
	callLocal("file.flush", file.flush)
	callLocal("file.write", file.write, "Line 2")
	callLocal("file.close", file.close)
	test("exists", true, "test_file.txt")
	call("copy", "test_file.txt", "test_file_new.txt")
	test("exists", true, "test_file_new.txt")
	call("move", "test_file_new.txt", "test_file_new2.txt")
	test("exists", false, "test_file_new.txt")
	test("exists", true, "test_file_new2.txt")
	file = call("open", "test_file_new2.txt", "r")
	testLocal("file.readLine", callLocal("file.readLine", file.readLine), "This is a test")
	testLocal("file.readLine", callLocal("file.readLine", file.readLine), "Line 2")
	testLocal("file.readLine", callLocal("file.readLine", file.readLine), nil)
	callLocal("file.close", file.close)
	call("delete", "test_file_new2.txt")
	test("exists", false, "test_file_new2.txt")
	file = call("open", "test_file.txt", "ab")
	callLocal("file.write", file.write, string.byte("\n"))
	callLocal("file.write", file.write, string.byte("H"))
	callLocal("file.write", file.write, string.byte("i"))
	callLocal("file.close", file.close)
	file = call("open", "test_file.txt", "r")
	testLocal("file.readAll", callLocal("file.readAll", file.readAll), "This is a test\nLine 2\nHi")
	callLocal("file.close", file.close)
	file = call("open", "test_file.txt", "rb")
	testLocal("file.read", callLocal("file.read", file.read), string.byte("T"))
	testLocal("file.read", callLocal("file.read", file.read), string.byte("h"))
	testLocal("file.read", callLocal("file.read", file.read), string.byte("i"))
	testLocal("file.read", callLocal("file.read", file.read), string.byte("s"))
	callLocal("file.close", file.close)
	call("delete", "test_file.txt")
	test("exists", false, "test_file.txt")
	test("combine", "rom/programs/shell.lua", "/rom/programs", "shell.lua")
	test("find", {{"rom/apis/help.lua", "rom/help/help.txt", "rom/programs/help.lua"}}, "/rom/*/help.*")
	test("complete", {{"abel.lua", "ist.lua", "ua.lua"}}, "l", "/rom/programs")
testEnd()

testStart "help"
	callLocal("fs.makeDir", fs.makeDir, "help")
	file = callLocal("fs.open", fs.open, "help/test.txt", "w")
	if file then
		callLocal("file.writeLine", file.writeLine, "This is a test help topic.")
		callLocal("file.close", file.close)
	end
	local oldPath = call("path")
	call("setPath", "/rom/help:/help")
	test("path", "/rom/help:/help")
	test("lookup", "help/test.txt", "test")
	test("lookup", "rom/help/shell.txt", "shell")
	testLocal("type(help.topics)", type(call("topics")), "table")
	test("completeTopic", {{"abel", "ist", "ua"}}, "l")
	call("setPath", oldPath)
	callLocal("fs.delete", fs.delete, "help")
testEnd()

testStart "http"
	local handle = call("get", "https://pastebin.com/raw/yY9StxvU")
	if testLocal("handle", type(handle), "table") then
		testLocal("handle.getResponseCode", callLocal("handle.getResponseCode", handle.getResponseCode), 200)
		testLocal("handle.readLine", callLocal("handle.readLine", handle.readLine), 'print("Downloading latest installer...")')
		testLocal("handle.readLine", callLocal("handle.readLine", handle.readLine), 'local file = http.get("https://raw.githubusercontent.com/MCJack123/CCKernel2/master/CCKernel2Installer.lua")')
		testLocal("handle.readLine", callLocal("handle.readLine", handle.readLine), 'local out = fs.open("install.lua", "w")')
		testLocal("handle.readAll", callLocal("handle.readAll", handle.readAll), [[out.write(file.readAll())
file.close()
out.close()
shell.run("/install.lua")
fs.delete("/install.lua")]])
		callLocal("handle.close", handle.close)
	end
	test("checkURL", {true, nil}, "https://pastebin.com/raw/yY9StxvU")
	test("checkURL", {false, "Must specify http or https"}, "qwertyuiop")
	test("checkURL", {false, "URL malformed"}, "http:qwertyuiop")
	test("checkURL", {false, "Invalid protocol 'ftp'"}, "ftp://example.com")
	test("checkURL", {false, "Domain not permitted"}, "http://192.168.1.1")
testEnd()

testStart "io"
	-- TODO: test IO API
testEnd()

testStart "keys"
	testValue("a", 30)
	testValue("z", 44)
	testValue("zero", 11)
	testValue("rightCtrl", 157)
	test("getName", "a", 30)
	test("getName", "z", 44)
	test("getName", "zero", 11)
	test("getName", "rightCtrl", 157)
testEnd()

testStart "math"
	local function testF(name, expected, ...) return testLocal(api.."."..name, math.floor(call(name, ...) * 1000000) / 1000000, expected) end
	test("abs", 7, -7)
	test("acos", math.pi / 2, 0)
	test("asin", math.pi / 2, 1)
	testF("atan", 0.785398, 1)
	testF("atan2", 0.927295, 4, 3)
	test("ceil", 6, 5.2654535)
	testF("cos", 0, math.pi / 2)
	test("cosh", 1, 0)
	test("deg", 90, math.pi / 2)
	testF("exp", 2.718281, 1)
	test("floor", 5, 5.2654535)
	test("fmod", 1, 7, 3)
	test("frexp", {0.625, 3}, 5)
	testLocal("math.huge", math.huge > 9E99, true)
	test("ldexp", 5, 0.625, 3)
	test("log", 2, math.exp(2))
	test("log10", 2, 100)
	test("max", 10, 2, 4, 6, 8, 10)
	test("min", 2, 2, 4, 6, 8, 10)
	test("modf", {5, 0.26545}, 5.26545)
	testLocal("math.pi", math.floor(math.pi * 1000000) / 1000000, 3.141592)
	test("pow", 8, 2, 3)
	test("rad", math.pi / 2, 90)
	--[[call("randomseed", 15)
	testF("random", 0.729982)
	call("randomseed", 15)
	test("random", 7, 15)
	call("randomseed", 15)
	test("random", 26, 15, 30)]]
	test("sin", 1, math.pi / 2)
	testF("sinh", 1.175201, 1)
	test("sqrt", 3, 9)
	testF("tan", 1.557407, 1)
	test("tanh", 1, 1000000)
testEnd()

if multishell ~= nil then testStart "multishell"
	test("getCurrent", 1)
	test("getCount", 1)
	call("setTitle", 1, "Title")
	test("getTitle", "Title", 1)
	file = callLocal("fs.open", fs.open, "multishell_test.lua", "w")
	callLocal("file.write", file.write, [[args = {...}
print("This is a test of the multishell API.")
sleep(3)
print("done")]])
	callLocal("file.close", file.close)
	local env = {args = {}}
	test("launch", 2, env, "multishell_test.lua", "test")
	test("getFocus", 1)
	test("getCount", 2)
	call("setTitle", 2, "test")
	test("getTitle", "test", 2)
	test("setFocus", true, 2)
	test("getFocus", 2)
	test("getCurrent", 1)
	testLocal("env.args[1]", env.args[1], "test")
	print("If stuck, press any key.")
	while call("getCount") ~= 1 do os.pullEvent() end
	test("getFocus", 1)
	callLocal("fs.delete", fs.delete, "multishell_test.lua")
testEnd() end

testStart "os"
	testLocal("os.getComputerID", type(os.getComputerID()), "number")
	testLocal("os.computerID", type(os.getComputerID()), "number")
	call("setComputerLabel", "Computer")
	test("getComputerLabel", "Computer")
	test("computerLabel", "Computer")
	call("queueEvent", "test", "test1", 2, true)
	test("pullEvent", {"test", "test1", 2, true}, "test")
	call("queueEvent", "terminate")
	test("pullEventRaw", "terminate", "terminate")
	testLocal("os.clock", type(os.clock()), "number")
	testLocal("os.time", type(os.time()), "number")
	testLocal("os.day", type(os.day()), "number")
	local id = call("startTimer", 3)
	local ev = {call("pullEvent")}
	while ev[1] ~= "timer" or ev[2] ~= id do ev = {call("pullEvent")} end
	id = call("startTimer", 3)
	call("cancelTimer", id)
	--local id = call("setAlarm", os.time() + .01)
	--print("Waiting for alarm, this may take a few minutes.")
	--local ev = {call("pullEvent")}
	--while ev[1] ~= "alarm" or ev[2] ~= id do ev = {call("pullEvent")} end
	local id = call("setAlarm", 3)
	call("cancelAlarm", id)
	call("sleep", 3)
	file = callLocal("fs.open", fs.open, "run_test.lua", "w")
	callLocal("file.write", file.write, [[print("This is a second program.")
function func(a) return a*3 end
if test then test("getComputerLabel", ({...})[1]) end]])
	callLocal("file.close", file.close)
	test("run", true, {test = test}, "run_test.lua", "Computer")
	test("loadAPI", true, "run_test.lua")
	testLocal("type(run_test)", type(run_test), "table")
	testLocal("run_test.func", callLocal("run_test.func", run_test.func, 2), 6)
	call("unloadAPI", "run_test")
	callLocal("fs.delete", fs.delete, "run_test.lua")
testEnd()

testStart "parallel"
	test("waitForAny", 2, function() callLocal("sleep", sleep, 3) end, function() callLocal("sleep", sleep, 1) end, function() callLocal("sleep", sleep, 5) end)
	local i = 0
	local function getParallelFunc(n) return function()
		callLocal("sleep", sleep, n)
		i=i+n
	end end
	call("waitForAll", getParallelFunc(3), getParallelFunc(1), getParallelFunc(5))
	testLocal("i", i, 9)
testEnd()

testStart "settings"
	test("save", true, "old_settings.ltn")
	call("clear")
	call("set", "test", 3)
	test("get", 3, "test")
	call("set", "test2", "hello")
	test("getNames", {{
		"bios.use_cash",
		"bios.use_multishell",
		"edit.autocomplete",
		"edit.default_extension",
		"list.show_hidden",
		"lua.autocomplete",
		"lua.function_args",
		"lua.function_source",
		"lua.warn_against_use_of_local",
		"motd.enable",
		"motd.path",
		"paint.default_extension",
		"shell.allow_disk_startup",
		"shell.allow_startup",
		"shell.autocomplete",
		"test",
		"test2"
	}})
	call("unset", "test")
	test("get", "none", "test", "none")
	test("save", true, "new_settings.ltn")
	test("load", true, "new_settings.ltn")
	test("get", "hello", "test2")
	call("unset", "test2")
	test("load", true, "new_settings.ltn")
	test("get", "hello", "test2")
	test("load", true, "old_settings.ltn")
	callLocal("fs.delete", fs.delete, "old_settings.ltn")
	callLocal("fs.delete", fs.delete, "new_settings.ltn")
testEnd()

testStart "shell"
	local oldDir = call("dir")
	call("setDir", "rom")
	test("dir", "rom")
	local oldPath = call("path")
	call("setPath", "/rom/programs:/rom/programs/fun")
	test("path", "/rom/programs:/rom/programs/fun")
	test("resolve", "rom/apis/colors.lua", "apis/colors.lua")
	test("resolveProgram", "rom/programs/shell.lua", "shell")
	testLocal("shell.aliases", type(call("aliases")), "table")
	call("setAlias", "del", "delete")
	test("resolveProgram", "rom/programs/delete.lua", "del")
	call("clearAlias", "del")
	testLocal("shell.programs", type(call("programs")), "table")
	local runningProgram = callLocal("fs.getName", fs.getName, call("getRunningProgram"))
	if runningProgram ~= "CraftOSTest.lua" and runningProgram ~= "startup.lua" then testLocal("shell.getRunningProgram", runningProgram, "CraftOSTest.lua or startup.lua") end
	test("completeProgram", {{"abel", "ist", "s", "ua"}}, "l")
	call("setDir", oldDir)
	call("setPath", oldPath)
	test("complete", {{"pis/", "pis", "utorun/", "utorun"}}, "cd rom/a")
	file = callLocal("fs.open", fs.open, "shell_test.lua", "w")
	callLocal("file.write", file.write, [[args = {...}
print("This is a test of the shell API: " .. args[1] .. ".")
sleep(3)]])
	callLocal("file.close", file.close)
	test("run", true, "shell_test.lua current_tab")
	if callLocal("term.isColor", term.isColor) then
		test("openTab", 2, "shell_test.lua new_tab")
		call("switchTab", 2)
		testLocal("shell.switchTab", callLocal("multishell.getCount", multishell.getCount), 2)
		callLocal("print", print, "If stuck, press any key.")
		while callLocal("multishell.getCount", multishell.getCount) == 2 do callLocal("os.pullEvent", os.pullEvent) end
	end
	-- TODO: add setCompletionFunction test
	callLocal("fs.delete", fs.delete, "shell_test.lua")
testEnd()

-- might add string tests later, assuming it'll work since it's Lua standard

testStart "table"
	test("concat", "is:a", {"this", "is", "a", "test"}, ":", 2, 3)
	test("concat", "", {"this", "is", "a", "test"}, ":", 4, 1)
	local t = {"this", "is", "a", "test"}
	call("insert", t, "hello")
	testLocal("table.insert", t, {"this", "is", "a", "test", "hello"})
	call("insert", t, 4, "real")
	testLocal("table.insert", t, {"this", "is", "a", "real", "test", "hello"})
	test("maxn", 4, {"this", "is", "a", "test"})
	test("pack", {{"this", "is", "a", "test", n = 4}}, "this", "is", "a", "test")
	test("remove", "this", t, 1)
	testLocal("table.remove", t, {"is", "a", "real", "test", "hello"})
	test("remove", "hello", t, call("maxn", t))
	testLocal("table.remove", t, {"is", "a", "real", "test"})
	t = {9, 3, 5, 1, 7}
	call("sort", t)
	testLocal("table.sort", t, {1, 3, 5, 7, 9})
	t = {"dragonfruit", "carrot", "apple", "eggplant", "banana"}
	call("sort", t, function(a, b) return string.byte(a, 1) < string.byte(b, 1) end)
	testLocal("table.sort", t, {"apple", "banana", "carrot", "dragonfruit", "eggplant"})
	test("unpack", {"this", "is", "a", "test"}, {"this", "is", "a", "test"})
testEnd()

if not _HEADLESS then testStart "term"
	test("getSize", {51, 19})
	call("clear")
	call("setCursorPos", 1, 1)
	test("getCursorPos", {1, 1})
	call("write", "This terminal " .. (call("isColor") and "supports" or "does not support") .. " color.")
	call("setCursorPos", 1, 2)
	call("write", "This text should overflow the boundaries of the terminal as long as the terminal is 51x19.")
	call("setCursorPos", 1, 3)
	call("write", "The following text should be the color it says:")
	call("setCursorPos", 1, 4)
	call("blit", "red orange yellow green blue purple white black", "eeee11111114444444ddddddbbbbbaaaaaaa000000fffff", "ffffffffffffffffffffffffffffffffffffffffff00000")
	call("setCursorPos", 1, 5)
	call("write", "The following text BG should be its color:")
	call("setCursorPos", 1, 6)
	call("blit", "red orange yellow green blue purple white black", "ffffffffffffffffffffffffffffffffffffffffff00000", "eeee11111114444444ddddddbbbbbaaaaaaa000000fffff")
	call("setCursorBlink", false)
	if term.getCursorBlink == nil then
		call("setCursorPos", 1, 7)
		call("write", "The cursor should not be blinking.")
	else test("getCursorBlink", false) end
	callLocal("sleep", sleep, 3)
	call("setCursorPos", 1, 8)
	call("write", "Type exactly 'y' if all statements are true.")
	call("setCursorPos", 1, 9)
	testLocal("term_sectionA", callLocal("read", read), "y")
	call("write", "The screen should scroll 2 lines.")
	call("scroll", 2)
	call("write", "The top line should be erased.")
	call("setCursorPos", 1, 1)
	call("clearLine")
	call("setCursorPos", 1, 10)
	call("write", "The entire screen should clear to white in 3 secs.")
	call("setBackgroundColor", colors.white)
	call("setTextColor", colors.black)
	test("getBackgroundColor", colors.white)
	test("getTextColor", colors.black)
	callLocal("sleep", sleep, 3)
	call("clear")
	local r, g, b
	if call("isColor") then
		call("setCursorPos", 1, 1)
		call("write", "The screen should turn bright purple in 3 seconds.")
		callLocal("sleep", sleep, 3)
		r, g, b = call("getPaletteColor", colors.white)
		call("setPaletteColor", colors.white, .5, .5, 1)
		test("getPaletteColor", {.5, .5, 1}, colors.white)
	end
	call("setCursorPos", 1, 2)
	call("write", "Type exactly 'y' if all statements were true.")
	call("setCursorPos", 1, 3)
	testLocal("term_sectionB", callLocal("read", read), "y")
	if call("isColor") then
		call("setPaletteColor", colors.white, r, g, b)
		test("getPaletteColor", {r, g, b}, colors.white)
	end
	call("setBackgroundColor", colors.black)
	call("setTextColor", colors.white)
	call("setCursorPos", 1, 1)
	call("setCursorBlink", true)
	call("clear")
	test("getBackgroundColor", colors.black)
	test("getTextColor", colors.white)
	test("getCursorPos", {1, 1})
	if term.getCursorBlink then test("getCursorBlink", true) end
	testLocal("term.current", type(call("current")), "table")
	testLocal("term.native", type(call("native")), "table")
	local oldwin = call("redirect", call("native"))
	testLocal("term.redirect", type(oldwin), "table")
	test("current", {call("native")})
	call("redirect", oldwin)
	testValue("restore", nil)
testEnd() end

testStart "textutils"
	call("slowWrite", "This should write slowly: ")
	call("slowPrint", "And even slower this time...", 10)
	call("slowPrint", "Very slow...", 2)
	call("tabulate", {"First", "Last", "Balance"}, colors.white, {"John", "Doe", "$123.45"}, {"Jane", "Smith", "$13.37"}, colors.gray, {"Bob", "Johnson", "-$2.13"})
	callLocal("print", print, "The following text should be paged:")
	call("pagedTabulate", 
		{"First", "Last", "Balance"}, colors.white, {"John", "Doe", "$123.45"}, {"Jane", "Smith", "$13.37"}, colors.gray, {"Bob", "Johnson", "-$2.13"},
		colors.white, {"John", "Doe", "$123.45"}, {"Jane", "Smith", "$13.37"}, colors.gray, {"Bob", "Johnson", "-$2.13"},
		colors.white, {"John", "Doe", "$123.45"}, {"Jane", "Smith", "$13.37"}, colors.gray, {"Bob", "Johnson", "-$2.13"},
		colors.white, {"John", "Doe", "$123.45"}, {"Jane", "Smith", "$13.37"}, colors.gray, {"Bob", "Johnson", "-$2.13"},
		colors.white, {"John", "Doe", "$123.45"}, {"Jane", "Smith", "$13.37"}, colors.gray, {"Bob", "Johnson", "-$2.13"},
		colors.white, {"John", "Doe", "$123.45"}, {"Jane", "Smith", "$13.37"}, colors.gray, {"Bob", "Johnson", "-$2.13"},
		colors.white, {"John", "Doe", "$123.45"}, {"Jane", "Smith", "$13.37"}, colors.gray, {"Bob", "Johnson", "-$2.13"},
		colors.white, {"John", "Doe", "$123.45"}, {"Jane", "Smith", "$13.37"}, colors.gray, {"Bob", "Johnson", "-$2.13"}
	)
	callLocal("term.setTextColor", term.setTextColor, colors.white)
	if not _HEADLESS then call("pagedPrint", [[Lorem ipsum dolor sit amet, consectetur adipiscing elit. Ut vitae urna viverra justo viverra placerat.
Quisque sollicitudin sem mi, ultrices ullamcorper ante aliquet eu. Vivamus condimentum sem libero, vitae porta dolor dictum vitae. 
Proin eleifend ipsum elit. Ut finibus magna quis quam dapibus, at bibendum ligula gravida. 
Nam neque nibh, pretium eget libero ac, iaculis finibus elit. Vivamus ipsum massa, pharetra semper velit eu, condimentum dictum metus. 
Vestibulum a nibh vitae magna euismod faucibus eu vel diam. In cursus laoreet vehicula. In feugiat, sem eu tristique malesuada, 
mi elit consequat nibh, finibus fringilla elit elit eget tellus. Morbi non ante ornare, hendrerit enim et, imperdiet enim. 
Nunc non pulvinar magna, id tempus erat. Nunc eget magna non quam dapibus gravida in at lacus. Donec dignissim pellentesque enim, eu placerat ligula accumsan in.]], 3) end
	local ser = call("serialize", {"this", "is", "a", "test", results = {first = "this", last = "test"}})
	test("unserialize", {{"this", "is", "a", "test", results = {last = "test", first = "this"}}}, ser)
	testLocal("textutils.serializeJSON", type(call("serializeJSON", {"this", "is", "a", "test", results = {first = "this", last = "test"}})), "string")
	test("urlEncode", "this+is+a+%2Ftest+%24hello+%25hi", "this is a /test $hello %hi")
	test("complete", {{"able.", "erm.", "extutils.", "onumber(", "ostring(", "ype("}}, "t", _ENV)
testEnd()

testStart "vector"
	local va = call("new", 1, 2, 3)
	local vb = call("new", 10, 5, 3)
	local vc = callLocal("vector:add", va.add, va, vb)
	testLocal("vector:add", {vc.x, vc.y, vc.z}, {11, 7, 6})
	vc = callLocal("vector:sub", vb.sub, vb, va)
	testLocal("vector:sub", {vc.x, vc.y, vc.z}, {9, 3, 0})
	vc = callLocal("vector:mul", vb.mul, vb, 3)
	testLocal("vector:mul", {vc.x, vc.y, vc.z}, {30, 15, 9})
	testLocal("vector:dot", callLocal("vector:dot", va.dot, va, vb), 29)
	vc = callLocal("vector:cross", va.cross, va, vb)
	testLocal("vector:cross", {vc.x, vc.y, vc.z}, {-9, 27, -15})
	testLocal("vector:length", callLocal("vector:length", vb.length, vb), 11.575837)
	vc = callLocal("vector:normalize", vb.normalize, vb)
	testLocal("vector:normalize", {vc.x, vc.y, vc.z}, {0.8638684, 0.4319342, 0.25916052})
	vc = callLocal("vector:round", vc.round, vc)
	testLocal("vector:round", {vc.x, vc.y, vc.z}, {1, 0, 0})
	testLocal("vector:tostring", callLocal("vector:tostring", vb.tostring, vb), "10,5,3")
testEnd()

testStart "window"
	local x, y = callLocal("term.getCursorPos", term.getCursorPos)
	local win = call("create", term.current(), 5, 5, 12, 5)
	testLocal("window.getPosition", {callLocal("window.getPosition", win.getPosition)}, {5, 5})
	testLocal("window.getSize", {callLocal("window.getSize", win.getSize)}, {12, 5})
	callLocal("window.setBackgroundColor", win.setBackgroundColor, colors.red)
	callLocal("window.setTextColor", win.setTextColor, colors.cyan)
	testLocal("window.getBackgroundColor", callLocal("window.getBackgroundColor", win.getBackgroundColor), colors.red)
	testLocal("window.getTextColor", callLocal("window.getTextColor", win.getTextColor), colors.cyan)
	callLocal("window.clear", win.clear)
	callLocal("window.write", win.write, "Window Test ")
	callLocal("window.setCursorPos", win.setCursorPos, 1, 2)
	testLocal("window.getCursorPos", {callLocal("window.getCursorPos", win.getCursorPos)}, {1, 2})
	local oldwin = callLocal("term.redirect", term.redirect, win)
	callLocal("print", print, "This is a long string of text")
	callLocal("sleep", sleep, 1)
	callLocal("window.reposition", win.reposition, 15, 10)
	testLocal("window.getPosition", {callLocal("window.getPosition", win.getPosition)}, {15, 10})
	testLocal("window.getSize", {callLocal("window.getSize", win.getSize)}, {12, 5})
	callLocal("window.setCursorPos", win.setCursorPos, 1, 1)
	callLocal("window.redraw", win.redraw)
	callLocal("term.redirect", term.redirect, oldwin)
	callLocal("term.setCursorPos", term.setCursorPos, 1, 1)
	callLocal("window.restoreCursor", win.restoreCursor)
	testLocal("term.getCursorPos", {callLocal("term.getCursorPos", term.getCursorPos)}, {15, 10})
	callLocal("window.setVisible", win.setVisible, false)
	callLocal("window.redraw", win.redraw)
	callLocal("term.setCursorPos", term.setCursorPos, x, y)
testEnd()

term.setBackgroundColor(colors.black)
term.setTextColor(colors.white)
term.setCursorPos(1, 1)
term.clear()

local failed = {}
for k,v in pairs(api_tests) do if v > 0 then table.insert(failed, k) end end
if #failed > 0 then
	term.setTextColor(colors.red)
	print("!!! " .. #failed .. " tests failed")
	logfile.writeLine("!!! " .. #failed .. " tests failed")
	term.setTextColor(colors.white)
	print("=>  Tests that failed:")
	logfile.writeLine("=>  Tests that failed:")
	for k,v in pairs(failed) do 
		print("    " .. v .. " (" .. api_tests[v] .. " failed tests)")
		logfile.writeLine("    " .. v .. " (" .. api_tests[v] .. " failed tests)")
	end
else
	term.setTextColor(colors.lime)
	print("==> All tests passed.")
	logfile.writeLine("==> All tests passed.")
	term.setTextColor(colors.white)
end
logfile.close()
if _HEADLESS then os.shutdown(#failed) end