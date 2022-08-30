package cc.craftospc.CraftOSPC;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.res.AssetManager;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.provider.Settings;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.inputmethod.InputMethodManager;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.constraintlayout.widget.ConstraintLayout;

import com.mapbox.android.gestures.AndroidGesturesManager;
import com.mapbox.android.gestures.MultiFingerTapGestureDetector;
import com.mapbox.android.gestures.ShoveGestureDetector;
import com.mapbox.android.gestures.SidewaysShoveGestureDetector;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.util.Scanner;

public class MainActivity extends SDLActivity implements MultiFingerTapGestureDetector.OnMultiFingerTapGestureListener, ShoveGestureDetector.OnShoveGestureListener, SidewaysShoveGestureDetector.OnSidewaysShoveGestureListener, View.OnTouchListener {
    public static MainActivity instance = null;
    int visibleHeight = 0;
    public AndroidGesturesManager gesturesManager;
    boolean showingBar = false;
    boolean isCtrlDown = false;
    boolean isAltDown = false;
    boolean isKeyboardVisible = false;
    Handler holdHandler = new Handler();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        instance = this;
        String filePath = getCacheDir().getAbsolutePath();
        int version = 0;
        try {
            PackageInfo pInfo = getPackageManager().getPackageInfo(getPackageName(), 0);
            version = pInfo.versionCode;
        } catch (Exception ignored) {}
        File versionFile = new File(filePath + "/.last_rom_update");
        boolean extract = true;
        try {
            Scanner scanner = new Scanner(versionFile);
            int currentROM = scanner.nextInt();
            scanner.close();
            if (version == currentROM) extract = false;
        } catch (Exception ignored) {}
        if (extract) {
            extractAssets("lua", filePath, true);
            try {
                FileWriter writer = new FileWriter(versionFile);
                writer.write(Integer.toString(version));
                writer.close();
            } catch (Exception ignored) {}
        } else System.out.println("Already extracted ROM for this build; skipping.");
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        ((ConstraintLayout)findViewById(R.id.rootview)).addView(mLayout, 0);
        gesturesManager = new AndroidGesturesManager(getBaseContext());
        gesturesManager.setMultiFingerTapGestureListener(this);
        gesturesManager.setShoveGestureListener(this);
        gesturesManager.setSidewaysShoveGestureListener(this);
        findViewById(R.id.terminateButton).setOnTouchListener(this);
        findViewById(R.id.shutdownButton).setOnTouchListener(this);
        findViewById(R.id.rebootButton).setOnTouchListener(this);
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
                isKeyboardVisible = (view.getHeight() - visibleHeight) / (double)view.getHeight() > 0.2; // probably?
                final View keyboardToolbar = findViewById(R.id.keyboardToolbar);
                if (Math.abs(oldHeight - visibleHeight) > 10 && (isKeyboardVisible || oldKeyboardVisible)) sendKeyboardUpdate(visibleHeight - keyboardToolbar.getHeight());
                if (isKeyboardVisible) {
                    final float keyboardHeight = rootView.getHeight() - visibleHeight;
                    keyboardToolbar.animate().setDuration(oldKeyboardVisible ? 0 : 100).translationY(-keyboardHeight);
                } else {
                    keyboardToolbar.animate().setDuration(250).translationY(rootView.getHeight() + keyboardToolbar.getHeight());
                }
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

    @Override
    public void setTitle(CharSequence title) {
        super.setTitle(title);
        ((TextView)findViewById(R.id.title)).setText(title);
    }

    private static native void sendKeyboardUpdate(int size);
    private static native void sendCloseEvent();

    public void resetModifiers() {
        if (isCtrlDown) {
            onNativeKeyUp(KeyEvent.KEYCODE_CTRL_LEFT);
            isCtrlDown = false;
            findViewById(R.id.controlButton).setBackgroundColor(0);
        }
        if (isAltDown) {
            onNativeKeyUp(KeyEvent.KEYCODE_ALT_LEFT);
            isAltDown = false;
            findViewById(R.id.altButton).setBackgroundColor(0);
        }
    }

    private void extractAssets(String assetPath, String destPath, boolean isRoot) {
        AssetManager assets = getAssets();
        File dir = new File(destPath);
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

    // Gestures

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return gesturesManager.onTouchEvent(event);
    }

    @Override
    public boolean onMultiFingerTap(@NonNull MultiFingerTapGestureDetector detector, int pointersCount) {
        if (pointersCount == 2) {
            ConstraintLayout v = findViewById(R.id.toolbarGroup);
            v.animate().translationY(showingBar ? -v.getHeight() : 0);
            showingBar = !showingBar;
            return true;
        }
        return false;
    }

    void handleShove(float velocityX, float velocityY) {
        if (velocityY > 500) {
            onNativeKeyDown(KeyEvent.KEYCODE_DPAD_DOWN);
            onNativeKeyUp(KeyEvent.KEYCODE_DPAD_DOWN);
        } else if (velocityY < -500) {
            onNativeKeyDown(KeyEvent.KEYCODE_DPAD_UP);
            onNativeKeyUp(KeyEvent.KEYCODE_DPAD_UP);
        }
        if (velocityX > 500) {
            onNativeKeyDown(KeyEvent.KEYCODE_DPAD_RIGHT);
            onNativeKeyUp(KeyEvent.KEYCODE_DPAD_RIGHT);
        } else if (velocityX < -500) {
            onNativeKeyDown(KeyEvent.KEYCODE_DPAD_LEFT);
            onNativeKeyUp(KeyEvent.KEYCODE_DPAD_LEFT);
        }
    }

    @Override
    public boolean onShoveBegin(@NonNull ShoveGestureDetector detector) {
        return true;
    }

    @Override
    public boolean onShove(@NonNull ShoveGestureDetector detector, float deltaPixelsSinceLast, float deltaPixelsSinceStart) {
        return true;
    }

    @Override
    public void onShoveEnd(@NonNull ShoveGestureDetector detector, float velocityX, float velocityY) {
        handleShove(velocityX, velocityY);
    }

    @Override
    public boolean onSidewaysShoveBegin(@NonNull SidewaysShoveGestureDetector detector) {
        return true;
    }

    @Override
    public boolean onSidewaysShove(@NonNull SidewaysShoveGestureDetector detector, float deltaPixelsSinceLast, float deltaPixelsSinceStart) {
        return true;
    }

    @Override
    public void onSidewaysShoveEnd(@NonNull SidewaysShoveGestureDetector detector, float velocityX, float velocityY) {
        handleShove(velocityX, velocityY);
    }

    // Button actions

    public void onPreviousButton(View view) {
        onNativeKeyDown(KeyEvent.KEYCODE_CTRL_LEFT);
        onNativeKeyDown(KeyEvent.KEYCODE_ALT_LEFT);
        onNativeKeyDown(KeyEvent.KEYCODE_DPAD_LEFT);
        onNativeKeyUp(KeyEvent.KEYCODE_DPAD_LEFT);
        onNativeKeyUp(KeyEvent.KEYCODE_ALT_LEFT);
        onNativeKeyUp(KeyEvent.KEYCODE_CTRL_LEFT);
        if (isCtrlDown) onCtrlButton(view);
        if (isAltDown) onAltButton(view);
    }

    public void onNextButton(View view) {
        onNativeKeyDown(KeyEvent.KEYCODE_CTRL_LEFT);
        onNativeKeyDown(KeyEvent.KEYCODE_ALT_LEFT);
        onNativeKeyDown(KeyEvent.KEYCODE_DPAD_RIGHT);
        onNativeKeyUp(KeyEvent.KEYCODE_DPAD_RIGHT);
        onNativeKeyUp(KeyEvent.KEYCODE_ALT_LEFT);
        onNativeKeyUp(KeyEvent.KEYCODE_CTRL_LEFT);
        if (isCtrlDown) onCtrlButton(view);
        if (isAltDown) onAltButton(view);
    }

    public void onKeyboardButton(View view) {
        InputMethodManager imm = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
        View rootView = getWindow().getDecorView();
        if (isKeyboardVisible) {
            sendMessage(SDLActivity.COMMAND_TEXTEDIT_HIDE, 0);
        } else {
            SDLActivity.showTextInput(0, 0, rootView.getWidth(), rootView.getHeight());
            imm.restartInput(mTextEdit);
        }
    }

    public void onCloseButton(View view) {
        sendCloseEvent();
    }

    public void onCtrlButton(View view) {
        if (isCtrlDown) onNativeKeyUp(KeyEvent.KEYCODE_CTRL_LEFT);
        else onNativeKeyDown(KeyEvent.KEYCODE_CTRL_LEFT);
        isCtrlDown = !isCtrlDown;
        findViewById(R.id.controlButton).setBackgroundColor(isCtrlDown ? 0x40000000 : 0);
    }

    public void onAltButton(View view) {
        if (isAltDown) onNativeKeyUp(KeyEvent.KEYCODE_ALT_LEFT);
        else onNativeKeyDown(KeyEvent.KEYCODE_ALT_LEFT);
        isAltDown = !isAltDown;
        findViewById(R.id.altButton).setBackgroundColor(isAltDown ? 0x40000000 : 0);
    }

    public void onTabButton(View view) {
        onNativeKeyDown(KeyEvent.KEYCODE_TAB);
        onNativeKeyUp(KeyEvent.KEYCODE_TAB);
    }

    public void onPasteButton(View view) {
        onNativeKeyDown(KeyEvent.KEYCODE_CTRL_LEFT);
        onNativeKeyDown(KeyEvent.KEYCODE_V);
        onNativeKeyUp(KeyEvent.KEYCODE_V);
        onNativeKeyUp(KeyEvent.KEYCODE_CTRL_LEFT);
        if (isCtrlDown) onCtrlButton(view);
    }

    public Runnable onTerminateButton = () -> {
        onNativeKeyDown(KeyEvent.KEYCODE_CTRL_LEFT);
        onNativeKeyDown(KeyEvent.KEYCODE_T);
        onNativeKeyDown(KeyEvent.KEYCODE_T);
        onNativeKeyUp(KeyEvent.KEYCODE_T);
        onNativeKeyUp(KeyEvent.KEYCODE_CTRL_LEFT);
        if (isCtrlDown) onCtrlButton(null);
    };

    public Runnable onShutdownButton = () -> {
        onNativeKeyDown(KeyEvent.KEYCODE_CTRL_LEFT);
        onNativeKeyDown(KeyEvent.KEYCODE_S);
        onNativeKeyDown(KeyEvent.KEYCODE_S);
        onNativeKeyUp(KeyEvent.KEYCODE_S);
        onNativeKeyUp(KeyEvent.KEYCODE_CTRL_LEFT);
        if (isCtrlDown) onCtrlButton(null);
    };

    public Runnable onRebootButton = () -> {
        onNativeKeyDown(KeyEvent.KEYCODE_CTRL_LEFT);
        onNativeKeyDown(KeyEvent.KEYCODE_R);
        onNativeKeyDown(KeyEvent.KEYCODE_R);
        onNativeKeyUp(KeyEvent.KEYCODE_R);
        onNativeKeyUp(KeyEvent.KEYCODE_CTRL_LEFT);
        if (isCtrlDown) onCtrlButton(null);
    };

    // TSR handler
    @Override
    public boolean onTouch(View v, MotionEvent event) {
        Runnable r;
        switch (v.getId()) {
            case R.id.terminateButton: r = onTerminateButton; break;
            case R.id.shutdownButton: r = onShutdownButton; break;
            case R.id.rebootButton: r = onRebootButton; break;
            default: return false;
        }
        if (event.getAction() == MotionEvent.ACTION_DOWN) {
            holdHandler.removeCallbacks(r);
            holdHandler.postDelayed(r, 1000);
            v.performClick();
            return true;
        } else if (event.getAction() == MotionEvent.ACTION_UP) {
            holdHandler.removeCallbacks(r);
            return true;
        }
        return false;
    }
}
