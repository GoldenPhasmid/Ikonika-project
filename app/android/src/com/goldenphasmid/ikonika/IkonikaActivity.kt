package com.goldenphasmid.ikonika

import android.content.Intent
import android.os.Bundle
import android.os.SystemClock
import android.view.KeyEvent
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import org.qtproject.qt.android.bindings.QtActivity

private const val MinimumSplashDurationMs = 500L

class IkonikaActivity : QtActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        val splashScreen = installSplashScreen()

        SplashGate.Attach(this)

        super.onCreate(savedInstanceState)

        val startElapsed = SystemClock.elapsedRealtime()

        splashScreen.setKeepOnScreenCondition {
            SystemClock.elapsedRealtime() - startElapsed < MinimumSplashDurationMs
                || !SplashGate.appReady
        }

        ReaderIntake.accept(this, intent)
        HomeWidget.acceptLaunch(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)

        ReaderIntake.accept(this, intent)
        HomeWidget.acceptLaunch(intent)
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (ReaderKeys.handle(event)) {
            return true
        }

        return super.dispatchKeyEvent(event)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == GoogleAuthorization.AUTHORIZATION_REQUEST_CODE) {
            GoogleAuthorization.handleAuthorizationResult(this, data)
            return
        }

        if (requestCode == ReaderFilePicker.PICK_REQUEST_CODE) {
            ReaderFilePicker.handleResult(this, resultCode, data)
            return
        }

        super.onActivityResult(requestCode, resultCode, data)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        if (requestCode == PlatformNotification.PERMISSION_REQUEST_CODE) {
            PlatformNotification.handlePermissionResult(grantResults)
            return
        }

        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
    }
}
