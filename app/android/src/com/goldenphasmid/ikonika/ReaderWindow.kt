package com.goldenphasmid.ikonika

import android.app.Activity
import android.content.Context
import android.view.WindowManager
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat

object ReaderWindow {
    @JvmStatic
    fun setKeepScreenOn(context: Context, enabled: Boolean) {
        val activity = context as? Activity ?: return

        activity.runOnUiThread {
            if (enabled) {
                activity.window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
            } else {
                activity.window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
            }
        }
    }

    @JvmStatic
    fun setImmersive(context: Context, enabled: Boolean) {
        val activity = context as? Activity ?: return

        activity.runOnUiThread {
            val insetsController = WindowInsetsControllerCompat(activity.window, activity.window.decorView)

            if (enabled) {
                insetsController.systemBarsBehavior =
                    WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
                insetsController.hide(WindowInsetsCompat.Type.systemBars())
            } else {
                insetsController.show(WindowInsetsCompat.Type.systemBars())
            }
        }
    }
}
