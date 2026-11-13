package com.goldenphasmid.ikonika

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.graphics.drawable.Icon
import android.media.AudioAttributes
import android.media.AudioFocusRequest
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.media.MediaMetadata
import android.media.session.MediaSession
import android.media.session.PlaybackState
import android.os.Handler
import android.os.Looper
import android.util.Log

private const val SESSION_TAG = "ReadAloudSession"

object ReadAloudSession {
    @JvmStatic
    external fun nativeOnPlayRequested()

    @JvmStatic
    external fun nativeOnPauseRequested()

    @JvmStatic
    external fun nativeOnStopRequested()

    const val NOTIFICATION_ID = 2
    const val ACTION_PLAY = "com.goldenphasmid.ikonika.READ_ALOUD_PLAY"
    const val ACTION_PAUSE = "com.goldenphasmid.ikonika.READ_ALOUD_PAUSE"
    const val ACTION_STOP = "com.goldenphasmid.ikonika.READ_ALOUD_STOP"

    private const val CHANNEL_ID = "ikonika_read_aloud"
    private const val LAUNCH_REQUEST_CODE = 1003
    private const val ACCENT_COLOR = 0xFF5B9DFA.toInt()
    private const val SILENCE_SAMPLE_RATE = 8000

    private val mainHandler = Handler(Looper.getMainLooper())

    private var appContext: Context? = null
    private var session: MediaSession? = null
    private var focusRequest: AudioFocusRequest? = null
    private var silence: AudioTrack? = null
    private var title = ""
    private var playing = false
    private var serviceStarted = false
    private var pausedForFocus = false

    private val focusListener = AudioManager.OnAudioFocusChangeListener { change ->
        when (change) {
            AudioManager.AUDIOFOCUS_GAIN -> {
                if (pausedForFocus) {
                    Log.i(SESSION_TAG, "audio focus back, reading resumes")
                    pausedForFocus = false
                    report { nativeOnPlayRequested() }
                }
            }
            AudioManager.AUDIOFOCUS_LOSS_TRANSIENT, AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK -> {
                if (playing) {
                    Log.i(SESSION_TAG, "audio focus lost for a while, reading pauses")
                    pausedForFocus = true
                    report { nativeOnPauseRequested() }
                }
            }
            AudioManager.AUDIOFOCUS_LOSS -> {
                Log.i(SESSION_TAG, "audio focus lost, reading pauses")
                pausedForFocus = false
                abandonFocus()
                report { nativeOnPauseRequested() }
            }
        }
    }

    @JvmStatic
    fun update(context: Context, bookTitle: String, started: Boolean, nowPlaying: Boolean) {
        val applicationContext = context.applicationContext

        mainHandler.post {
            appContext = applicationContext
            title = bookTitle

            if (!started) {
                end()

                return@post
            }

            playing = nowPlaying

            if (playing && !requestFocus()) {
                Log.w(SESSION_TAG, "audio focus refused, reading pauses")
                playing = false
                report { nativeOnPauseRequested() }
            }

            if (playing) {
                pausedForFocus = false
            }

            publish()
        }
    }

    fun handleAction(action: String?) {
        when (action) {
            ACTION_PLAY -> report { nativeOnPlayRequested() }
            ACTION_PAUSE -> report { nativeOnPauseRequested() }
            ACTION_STOP -> report { nativeOnStopRequested() }
        }
    }

    fun buildNotification(context: Context): Notification {
        ensureChannel(context)

        val toggle = if (playing) {
            action(context, android.R.drawable.ic_media_pause, "Pause", ACTION_PAUSE)
        } else {
            action(context, android.R.drawable.ic_media_play, "Play", ACTION_PLAY)
        }

        val style = Notification.MediaStyle().setShowActionsInCompactView(0, 1)

        session?.let { style.setMediaSession(it.sessionToken) }

        return Notification.Builder(context, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_notification)
            .setLargeIcon(Icon.createWithResource(context, R.mipmap.ic_launcher_round))
            .setColor(ACCENT_COLOR)
            .setContentTitle(title)
            .setContentIntent(launchIntent(context))
            .setDeleteIntent(serviceIntent(context, ACTION_STOP))
            .setVisibility(Notification.VISIBILITY_PUBLIC)
            .setOngoing(playing)
            .setShowWhen(false)
            .addAction(toggle)
            .addAction(action(context, android.R.drawable.ic_menu_close_clear_cancel, "Stop", ACTION_STOP))
            .setStyle(style)
            .build()
    }

    fun serviceStopped() {
        serviceStarted = false
    }

    private fun publish() {
        val context = appContext ?: return
        val mediaSession = session ?: createSession(context)

        mediaSession.setMetadata(MediaMetadata.Builder()
            .putString(MediaMetadata.METADATA_KEY_TITLE, title)
            .build())

        mediaSession.setPlaybackState(PlaybackState.Builder()
            .setActions(PlaybackState.ACTION_PLAY or PlaybackState.ACTION_PAUSE
                or PlaybackState.ACTION_PLAY_PAUSE or PlaybackState.ACTION_STOP)
            .setState(if (playing) PlaybackState.STATE_PLAYING else PlaybackState.STATE_PAUSED,
                PlaybackState.PLAYBACK_POSITION_UNKNOWN, 1.0f)
            .build())

        mediaSession.isActive = true

        if (playing) {
            startSilence()
        } else {
            stopSilence()
        }

        if (!serviceStarted) {
            if (!playing) {
                return
            }

            try {
                context.startForegroundService(Intent(context, ReadAloudService::class.java))
                serviceStarted = true
            } catch (exception: IllegalStateException) {
                Log.w(SESSION_TAG, "read aloud service could not start: ${exception.message}")
            }

            return
        }

        val manager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        manager.notify(NOTIFICATION_ID, buildNotification(context))
    }

    private fun end() {
        playing = false
        pausedForFocus = false
        stopSilence()
        abandonFocus()

        session?.let {
            it.isActive = false
            it.release()
        }
        session = null

        val context = appContext ?: return

        if (serviceStarted) {
            context.stopService(Intent(context, ReadAloudService::class.java))
            serviceStarted = false
        }

        val manager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        manager.cancel(NOTIFICATION_ID)
    }

    private fun createSession(context: Context): MediaSession {
        val created = MediaSession(context, SESSION_TAG)

        created.setCallback(object : MediaSession.Callback() {
            override fun onPlay() {
                report { nativeOnPlayRequested() }
            }

            override fun onPause() {
                report { nativeOnPauseRequested() }
            }

            override fun onStop() {
                report { nativeOnStopRequested() }
            }
        }, mainHandler)

        session = created

        return created
    }

    private fun startSilence() {
        if (silence != null) {
            return
        }

        val frames = SILENCE_SAMPLE_RATE
        val track = AudioTrack.Builder()
            .setAudioAttributes(AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                .build())
            .setAudioFormat(AudioFormat.Builder()
                .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                .setSampleRate(SILENCE_SAMPLE_RATE)
                .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                .build())
            .setTransferMode(AudioTrack.MODE_STATIC)
            .setBufferSizeInBytes(frames * 2)
            .build()

        track.write(ShortArray(frames), 0, frames)
        track.setLoopPoints(0, frames, -1)
        track.play()

        silence = track
    }

    private fun stopSilence() {
        val track = silence ?: return

        track.stop()
        track.release()
        silence = null
    }

    private fun requestFocus(): Boolean {
        if (focusRequest != null) {
            return true
        }

        val context = appContext ?: return false
        val manager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        val request = AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
            .setAudioAttributes(AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                .build())
            .setWillPauseWhenDucked(true)
            .setOnAudioFocusChangeListener(focusListener, mainHandler)
            .build()

        if (manager.requestAudioFocus(request) != AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
            return false
        }

        focusRequest = request

        return true
    }

    private fun abandonFocus() {
        val request = focusRequest ?: return
        val context = appContext ?: return
        val manager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager

        manager.abandonAudioFocusRequest(request)
        focusRequest = null
    }

    private fun action(context: Context, icon: Int, label: String, intentAction: String): Notification.Action {
        return Notification.Action.Builder(Icon.createWithResource(context, icon), label,
            serviceIntent(context, intentAction)).build()
    }

    private fun serviceIntent(context: Context, intentAction: String): PendingIntent {
        val intent = Intent(context, ReadAloudService::class.java).setAction(intentAction)

        return PendingIntent.getService(context, intentAction.hashCode(), intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
    }

    private fun launchIntent(context: Context): PendingIntent {
        val intent = Intent(context, IkonikaActivity::class.java)
            .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP)

        return PendingIntent.getActivity(context, LAUNCH_REQUEST_CODE, intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
    }

    private fun ensureChannel(context: Context) {
        val manager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

        if (manager.getNotificationChannel(CHANNEL_ID) != null) {
            return
        }

        val channel = NotificationChannel(CHANNEL_ID, "Read aloud", NotificationManager.IMPORTANCE_LOW)
        channel.setShowBadge(false)

        manager.createNotificationChannel(channel)
    }

    private fun report(call: () -> Unit) {
        try {
            call()
        } catch (exception: UnsatisfiedLinkError) {
            Log.w(SESSION_TAG, "read aloud bridge not registered")
        }
    }
}
