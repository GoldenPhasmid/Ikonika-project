package com.goldenphasmid.ikonika

import android.view.KeyEvent

object ReaderKeys {
    @Volatile
    private var turnPages = false

    @JvmStatic
    external fun nativeOnVolumeKey(forward: Boolean)

    @JvmStatic
    fun setTurnPages(enabled: Boolean) {
        turnPages = enabled
    }

    fun handle(event: KeyEvent): Boolean {
        if (!turnPages) {
            return false
        }

        val forward = when (event.keyCode) {
            KeyEvent.KEYCODE_VOLUME_DOWN -> true
            KeyEvent.KEYCODE_VOLUME_UP -> false
            else -> return false
        }

        if (event.action == KeyEvent.ACTION_DOWN) {
            nativeOnVolumeKey(forward)
        }

        return true
    }
}
