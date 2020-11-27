// Run this script with Node in the same directory as the ROM
// Generates an fs_standalone.cpp file that can be used for standalone builds
const fs = require("fs");
let out = fs.openSync("fs_standalone.cpp", "w")
fs.writeSync(out, '#include <FileEntry.hpp>\n\nFileEntry standaloneROM(')
function writeDir(path, level) {
    console.log("Reading directory " + path)
    fs.writeSync(out, "{\n")
    for (var f of fs.readdirSync(path)) {
        if (f == "." || f == ".." || f == ".DS_Store" || f == "desktop.ini" || f == "packStandaloneROM.js") continue;
        fs.writeSync(out, `${' '.repeat(level * 4)}{"${f}", `)
        if (fs.lstatSync(path + "/" + f).isDirectory()) writeDir(path + "/" + f, level + 1)
        else {
            console.log("Reading file " + path + "/" + f)
            fs.writeSync(out, JSON.stringify(fs.readFileSync(path + "/" + f, "utf8")).replace(/\\r\\n/g, "\\n").replace(/\\n/g, "\\n\"\n\""))
        }
        fs.writeSync(out, "},\n")
    }
    fs.writeSync(out, " ".repeat((level - 1) * 4) + "}")
}
writeDir("rom", 1)
fs.writeSync(out, ");\n\nFileEntry standaloneDebug(")
writeDir("debug", 1)
fs.writeSync(out, ");\n\nstd::string standaloneBIOS = ")
console.log("Reading BIOS")
fs.writeSync(out, JSON.stringify(fs.readFileSync("bios.lua", "utf8")).replace(/\\r\\n/g, "\\n").replace(/\\n/g, "\\n\"\n\"") + ";")
fs.closeSync(out);
