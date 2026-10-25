
package cc.craftospc.CraftOSPC;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.widget.Toast;

public class LaunchActivity extends Activity {
    private static final int STORAGE_REQ_CODE = 1001;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // Optional: Set a splash screen layout here if you create one
        // setContentView(R.layout.activity_launch);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                showPermissionRationale();
            } else {
                startCraftOS();
            }
        } else {
            startCraftOS();
        }
    }

    private void showPermissionRationale() {
        new AlertDialog.Builder(this)
            .setTitle("Storage Permission Required")
            .setMessage("CraftOS-PC requires 'All Files Access' to emulate the ComputerCraft filesystem, save your scripts, and access world data on your device.")
            .setPositiveButton("Grant Access", (dialog, which) -> requestManageStorage())
            .setNegativeButton("Exit", (dialog, which) -> finish())
            .setCancelable(false)
            .show();
    }

    private void requestManageStorage() {
        try {
            Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
            intent.addCategory("android.intent.category.DEFAULT");
            intent.setData(Uri.parse(String.format("package:%s", getApplicationContext().getPackageName())));
            startActivityForResult(intent, STORAGE_REQ_CODE);
        } catch (Exception e) {
            Intent intent = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
            startActivityForResult(intent, STORAGE_REQ_CODE);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == STORAGE_REQ_CODE) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                if (Environment.isExternalStorageManager()) {
                    startCraftOS();
                } else {
                    Toast.makeText(this, "CraftOS-PC cannot run without storage access.", Toast.LENGTH_LONG).show();
                    finish(); 
                }
            }
        }
    }

    private void startCraftOS() {
        Intent intent = new Intent(this, MainActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION); // Prevents a flashing screen transition
        startActivity(intent);
        finish(); 
    }
}
