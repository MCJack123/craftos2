package cc.craftospc.CraftOSPC;

import android.content.res.AssetManager;
import android.graphics.Rect;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;
import android.view.View;
import android.view.ViewTreeObserver;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class MainActivity extends SDLActivity {
    int visibleHeight = 0;
    boolean isKeyboardVisible = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        extractAssets("lua", getCacheDir().getAbsolutePath(), true);
        super.onCreate(savedInstanceState);
        final View view = getWindow().getDecorView();
        final View rootView = view.findViewById(android.R.id.content);
        rootView.getViewTreeObserver().addOnGlobalLayoutListener(new ViewTreeObserver.OnGlobalLayoutListener() {
            @Override
            public void onGlobalLayout() {
                Rect r = new Rect();
                view.getWindowVisibleDisplayFrame(r);
                int oldHeight = visibleHeight;
                boolean oldKeyboardVisible = isKeyboardVisible;
                visibleHeight = r.bottom - r.top;
                isKeyboardVisible = view.getHeight() - visibleHeight > 100; // probably?
                Log.d("Keyboard Height", "Size: " + visibleHeight);
                Log.d("Keyboard Height", "Visible? " + (isKeyboardVisible ? "Yes" : "No"));
                if (Math.abs(oldHeight - visibleHeight) > 10 && (isKeyboardVisible || oldKeyboardVisible)) sendKeyboardUpdate(visibleHeight);
            }
        });
    }

    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        // Check whether rotation lock is enabled, and bail if it is.
        try {
            if (Settings.System.getInt(getContext().getContentResolver(), Settings.System.ACCELEROMETER_ROTATION) == 0) return;
        } catch (Settings.SettingNotFoundException ignored) {}
        super.setOrientationBis(w, h, resizable, hint);
    }

    private static native void sendKeyboardUpdate(int size);

    private void extractAssets(String assetPath, String destPath, boolean isRoot) {
        AssetManager assets = getAssets();
        File dir = new File(destPath);
        if (isRoot || !dir.exists()) {
            dir.mkdirs();
            try {
                for (String p : assets.list(assetPath)) {
                    InputStream stream;
                    try {
                        stream = assets.open(assetPath + "/" + p);
                        System.out.println(assetPath + "/" + p + " => " + destPath + "/" + p);
                        FileOutputStream file = new FileOutputStream(destPath + "/" + p);
                        byte[] buf = new byte[4096];
                        while (stream.available() > 0) file.write(buf, 0, stream.read(buf));
                        stream.close();
                        file.close();
                    } catch (FileNotFoundException e) {
                        extractAssets(assetPath + "/" + p, destPath + "/" + p, false);
                    }
                }
            } catch (IOException | NullPointerException e) {
                System.err.println("Could not copy assets: " + e.getLocalizedMessage());
            }
        }
    }
}
