package com.goldenphasmid.ikonika

import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.ServiceInfo
import android.media.AudioManager
import android.os.Build
import android.os.IBinder
import android.util.Log

private const val SERVICE_TAG = "ReadAloudService"

class ReadAloudService : Service() {
    private val noisyReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            if (intent.action == AudioManager.ACTION_AUDIO_BECOMING_NOISY) {
                Log.i(SERVICE_TAG, "headphones unplugged, reading pauses")
                ReadAloudSession.handleAction(ReadAloudSession.ACTION_PAUSE)
            }
        }
    }

    override fun onCreate() {
        super.onCreate()

        registerReceiver(noisyReceiver, IntentFilter(AudioManager.ACTION_AUDIO_BECOMING_NOISY))
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        val notification = ReadAloudSession.buildNotification(this)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(ReadAloudSession.NOTIFICATION_ID, notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK)
        } else {
            startForeground(ReadAloudSession.NOTIFICATION_ID, notification)
        }

        ReadAloudSession.handleAction(intent?.action)

        return START_NOT_STICKY
    }

    override fun onTaskRemoved(rootIntent: Intent?) {
        Log.i(SERVICE_TAG, "app removed from recents, reading stops")
        ReadAloudSession.handleAction(ReadAloudSession.ACTION_STOP)
        PlatformSpeech.stopNarration()
        stopSelf()

        super.onTaskRemoved(rootIntent)
    }

    override fun onDestroy() {
        unregisterReceiver(noisyReceiver)
        ReadAloudSession.serviceStopped()

        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null
}
