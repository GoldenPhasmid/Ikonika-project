package com.goldenphasmid.ikonika

import android.content.Intent
import android.util.Log

object TextIntake {
    private var pendingText: String? = null

    @JvmStatic
    external fun nativeOnTextArrived()

    @JvmStatic
    @Synchronized
    fun takePending(): String? {
        val text = pendingText
        pendingText = null
        return text
    }

    fun accept(intent: Intent?) {
        if (intent?.action != Intent.ACTION_PROCESS_TEXT) {
            return
        }

        val text = intent.getCharSequenceExtra(Intent.EXTRA_PROCESS_TEXT)?.toString()?.trim()
        intent.action = null
        intent.removeExtra(Intent.EXTRA_PROCESS_TEXT)

        if (text.isNullOrEmpty()) {
            return
        }

        synchronized(this) {
            pendingText = text
        }

        try {
            nativeOnTextArrived()
        } catch (exception: UnsatisfiedLinkError) {
            Log.i("TextIntake", "Selection kept until the app is ready")
        }
    }
}
