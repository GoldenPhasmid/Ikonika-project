package com.goldenphasmid.ikonika

import android.app.NotificationManager
import android.content.Context
import android.content.Intent
import android.util.Log

private const val CLIENT_TAG = "PackageDownloadClient"

object PackageDownloadClient {
    const val ACTION_START = "com.goldenphasmid.ikonika.PACKAGE_DOWNLOAD_START"
    const val ACTION_CANCEL = "com.goldenphasmid.ikonika.PACKAGE_DOWNLOAD_CANCEL"
    const val EXTRA_PAYLOAD = "payload"
    const val EXTRA_DESTINATION = "destination"
    const val FINISHED_NOTIFICATION_ID = 4

    @JvmStatic
    fun start(context: Context, payload: String): Boolean {
        val applicationContext = context.applicationContext
        val intent = Intent(applicationContext, PackageDownloadService::class.java)
            .setAction(ACTION_START)
            .putExtra(EXTRA_PAYLOAD, payload)

        return try {
            applicationContext.startForegroundService(intent)

            true
        } catch (exception: IllegalStateException) {
            Log.w(CLIENT_TAG, "foreground start refused, trying a plain start: ${exception.message}")

            startPlain(applicationContext, intent)
        }
    }

    private fun startPlain(context: Context, intent: Intent): Boolean {
        return try {
            context.startService(intent)

            true
        } catch (exception: IllegalStateException) {
            Log.w(CLIENT_TAG, "download service could not start: ${exception.message}")

            false
        }
    }

    @JvmStatic
    fun cancel(context: Context, destinationPath: String) {
        val applicationContext = context.applicationContext
        val intent = Intent(applicationContext, PackageDownloadService::class.java)
            .setAction(ACTION_CANCEL)
            .putExtra(EXTRA_DESTINATION, destinationPath)

        try {
            applicationContext.startService(intent)
        } catch (exception: IllegalStateException) {
            Log.w(CLIENT_TAG, "download service could not be told to cancel: ${exception.message}")
        }
    }

    @JvmStatic
    fun acknowledgeFinished(context: Context) {
        val manager = context.applicationContext.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

        manager.cancel(FINISHED_NOTIFICATION_ID)
    }
}
