package com.goldenphasmid.ikonika

import android.app.Activity
import android.content.Context
import androidx.core.view.WindowInsetsControllerCompat

object StatusBarController {
    @JvmStatic
    fun applyAppearance(context: Context, lightBars: Boolean) {
        val activity = context as? Activity ?: return

        activity.runOnUiThread {
            val insetsController = WindowInsetsControllerCompat(activity.window, activity.window.decorView)
            insetsController.isAppearanceLightStatusBars = lightBars
            insetsController.isAppearanceLightNavigationBars = lightBars
        }
    }
}
