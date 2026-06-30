package com.cleverraven.cataclysmdda

import android.content.Context
import android.content.pm.ActivityInfo
import android.content.res.Configuration
import android.os.Environment
import android.os.Vibrator
import android.view.View
import android.widget.Toast
import androidx.preference.PreferenceManager
import java.io.File
import org.libsdl.app.SDLActivity

class CataclysmDDA : SDLActivity() {
    override fun setOrientationBis(w: Int, h: Int, resizeable: Boolean, hint: String?) {
        mSingleton.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_USER_LANDSCAPE
    }

    fun getDocumentsDirectory(): String {
        val file = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOCUMENTS)
        return file.absolutePath
    }

    fun vibrate(duration: Int) {
        try {
            val vibrator = getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
            @Suppress("DEPRECATION")
            vibrator.vibrate(duration.toLong())
        } catch (error: Exception) {
            System.err.println(error.message)
        }
    }

    fun toast(message: String) {
        try {
            runOnUiThread {
                Toast.makeText(applicationContext, message, Toast.LENGTH_SHORT).show()
            }
        } catch (error: Exception) {
            System.err.println(error.message)
        }
    }

    @Suppress("unused")
    private fun isHardwareKeyboardAvailable(): Boolean =
        resources.configuration.keyboard == Configuration.KEYBOARD_QWERTY

    @Suppress("unused")
    private fun getDisplayDensity(): Float = resources.displayMetrics.density

    fun show_sdl_surface() {
        try {
            runOnUiThread { mLayout.visibility = View.VISIBLE }
        } catch (error: Exception) {
            System.err.println(error.message)
        }
    }

    fun getDefaultSetting(settingsName: String, defaultValue: Boolean): Boolean =
        PreferenceManager.getDefaultSharedPreferences(applicationContext)
            .getBoolean(settingsName, defaultValue)
}
