package com.goldenphasmid.ikonika

import java.lang.ref.WeakReference

object SplashGate {
    @Volatile
    var appReady: Boolean = false
        private set

    private var ActivityRef: WeakReference<IkonikaActivity>? = null

    fun Attach(Activity: IkonikaActivity) {
        ActivityRef = WeakReference(Activity)
    }

    @JvmStatic
    fun markReady() {
        appReady = true

        // Qt draws straight to its own native surface and never triggers another decor view draw
        // pass, but AndroidX's compat splash screen (used below API 31) only removes its overlay
        // on the next one. Without this, the overlay covers an already-rendered app forever.
        val Activity = ActivityRef?.get() ?: return

        Activity.runOnUiThread {
            Activity.window?.decorView?.invalidate()
        }
    }
}
