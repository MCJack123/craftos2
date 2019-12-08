import java.lang.System;
import java.io.FileReader;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import javax.swing.JOptionPane;
import javax.swing.UIManager;
import javax.swing.UnsupportedLookAndFeelException;

import org.json.simple.*;
import org.json.simple.parser.*;

class CCEmuXConverter {
    static final String[][] optionPairs = {
        {"maximumFilesOpen", "maximumFilesOpen"},
        {"maxComputerCapacity", "computerSpaceLimit"},
        {"httpEnable", "http_enable"},
        {"disableLua51Features", "disable_lua51_features"},
        {"defaultComputerSettings", "default_computer_settings"},
        {"debugEnable", "debug_enable"}
    };

    public static void main(String[] args) throws FileNotFoundException, IOException, ParseException, ClassNotFoundException, UnsupportedLookAndFeelException, InstantiationException, IllegalAccessException {
        UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
        String name = System.getProperty("os.name"), oldpath, newpath;
        if (name.startsWith("Windows")) {
            oldpath = System.getenv("appdata") + "\\ccemux";
            newpath = System.getenv("appdata") + "\\CraftOS-PC";
        } else if (name.startsWith("Linux")) {
            String home = System.getenv("XDG_DATA_HOME");
            if (home == null) home = System.getenv("HOME") + "/.local/share";
            oldpath = home + "/ccemux";
            newpath = home + "/craftos-pc";
        } else if (name.startsWith("Mac OS X")) {
            oldpath = System.getenv("HOME") + "/Library/Application Support/ccemux";
            newpath = System.getenv("HOME") + "/Library/Application Support/CraftOS-PC";
        } else {
            System.err.println("Invalid OS");
            System.exit(1);
            return;
        }
        File dir = new File(newpath);
        dir.mkdir();
        dir = new File(newpath + "/config");
        dir.mkdir();
        JSONObject config = new JSONObject();
        JSONObject options = (JSONObject)(new JSONParser().parse(new FileReader(oldpath + "/ccemux.json")));
        for (String[] option : optionPairs)
            if (options.containsKey(option[0])) 
                config.put(option[1], options.get(option[0]));
        if (options.containsKey("plugins") && 
            ((JSONObject)options.get("plugins")).containsKey("net.clgd.ccemux.plugins.builtin.HDFontPlugin") && 
            ((JSONObject)((JSONObject)options.get("plugins")).get("net.clgd.ccemux.plugins.builtin.HDFontPlugin")).containsKey("enabled") &&
            (Boolean)(((JSONObject)((JSONObject)options.get("plugins")).get("net.clgd.ccemux.plugins.builtin.HDFontPlugin")).get("enabled")) == false)
            config.put("customFontPath", "");
        else config.put("customFontPath", "hdfont");
        PrintWriter writer = new PrintWriter(newpath + "/config/global.json");
        writer.write(config.toJSONString());
        writer.close();
        copyFolder(new File(oldpath + "/computer"), new File(newpath + "/computer"));
        JOptionPane.showMessageDialog(null, "Finished copying files, you may now open CraftOS-PC.", "Finished", JOptionPane.INFORMATION_MESSAGE);
    }

    private static void copyFolder(File sourceFolder, File destinationFolder) throws IOException {
        if (sourceFolder.isDirectory()) {
            if (!destinationFolder.exists()) destinationFolder.mkdir();
            for (String file : sourceFolder.list()) copyFolder(new File(sourceFolder, file), new File(destinationFolder, file));
        } else Files.copy(sourceFolder.toPath(), destinationFolder.toPath(), StandardCopyOption.REPLACE_EXISTING);
    }
}