package com.goldenphasmid.ikonika

import android.Manifest
import android.app.Activity
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.media.AudioAttributes
import android.net.Uri
import android.os.Build
import android.util.Log
import androidx.core.app.ActivityCompat
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import androidx.core.content.ContextCompat

private const val LOG_TAG = "PlatformNotification"

object PlatformNotification {
    const val PERMISSION_REQUEST_CODE = 8200
    private const val CHANNEL_ID = "ikonika_reminder_sound"
    private const val SILENT_CHANNEL_ID = "ikonika_reminder"
    private const val NOTIFICATION_ID = 1
    private const val LAUNCH_REQUEST_CODE = 1002
    private const val ACCENT_COLOR = 0xFF5B9DFA.toInt()

    @JvmStatic
    fun hasPermission(context: Context): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return true
        }

        return ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) ==
            PackageManager.PERMISSION_GRANTED
    }

    @JvmStatic
    fun requestPermission(context: Context) {
        if (hasPermission(context)) {
            return
        }

        val activity = context as? Activity ?: return

        ActivityCompat.requestPermissions(activity, arrayOf(Manifest.permission.POST_NOTIFICATIONS),
            PERMISSION_REQUEST_CODE)
    }

    fun handlePermissionResult(grantResults: IntArray) {
        val granted = grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED

        Log.i(LOG_TAG, "notification permission granted: $granted")
    }

    @JvmStatic
    fun notify(context: Context, title: String, text: String) {
        ensureChannel(context)

        val builder = NotificationCompat.Builder(context, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_notification)
            .setColor(ACCENT_COLOR)
            .setContentTitle(title)
            .setContentText(text)
            .setContentIntent(launchPendingIntent(context))
            .setAutoCancel(true)
            .setSound(soundUri(context))
            .setPriority(NotificationCompat.PRIORITY_DEFAULT)

        try {
            NotificationManagerCompat.from(context).notify(NOTIFICATION_ID, builder.build())
        } catch (exception: SecurityException) {
        }
    }

    private fun launchPendingIntent(context: Context): PendingIntent {
        val intent = Intent(context, IkonikaActivity::class.java)
            .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP)

        return PendingIntent.getActivity(
            context, LAUNCH_REQUEST_CODE, intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
    }

    private fun soundUri(context: Context): Uri {
        return Uri.parse("android.resource://${context.packageName}/${R.raw.sfx_complete}")
    }

    private fun ensureChannel(context: Context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return
        }

        val manager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        val channel = NotificationChannel(
            CHANNEL_ID, "Daily reminder", NotificationManager.IMPORTANCE_DEFAULT)

        channel.setSound(soundUri(context), AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_NOTIFICATION)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build())

        manager.deleteNotificationChannel(SILENT_CHANNEL_ID)
        manager.createNotificationChannel(channel)
    }
}
