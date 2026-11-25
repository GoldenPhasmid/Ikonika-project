package com.goldenphasmid.ikonika

import android.app.ActivityManager
import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.content.pm.ServiceInfo
import android.graphics.drawable.Icon
import android.os.Build
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.util.Log
import java.io.File
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.Executors
import org.json.JSONException
import org.json.JSONObject

private const val SERVICE_TAG = "PackageDownloadService"
private const val NOTIFICATION_ID = 3
private const val CHANNEL_ID = "ikonika_download"
private const val LAUNCH_REQUEST_CODE = 1004
private const val ACCENT_COLOR = 0xFF5B9DFA.toInt()

class PackageDownloadService : Service() {
    private val executor = Executors.newSingleThreadExecutor()
    private val mainHandler = Handler(Looper.getMainLooper())
    private val jobs = ConcurrentHashMap<String, PackageDownloadJob>()

    private var runningJob: PackageDownloadJob? = null
    private var title = ""
    private var percent = -1
    private var channelName = "Downloads"
    private var finishedText = ""

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            PackageDownloadClient.ACTION_START -> startJobs(intent)
            PackageDownloadClient.ACTION_CANCEL -> cancelJob(intent)
        }

        if (jobs.isEmpty()) {
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelf()
        }

        return START_NOT_STICKY
    }

    override fun onTimeout(startId: Int, fgsType: Int) {
        Log.w(SERVICE_TAG, "the system ended the data sync allowance, downloads pause until they are started again")

        jobs.values.forEach { it.interrupt() }

        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    override fun onDestroy() {
        executor.shutdown()

        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun startJobs(intent: Intent) {
        val payload = try {
            JSONObject(intent.getStringExtra(PackageDownloadClient.EXTRA_PAYLOAD) ?: "{}")
        } catch (exception: JSONException) {
            Log.w(SERVICE_TAG, "unreadable download request: ${exception.message}")

            JSONObject()
        }

        channelName = payload.optString("channel", channelName)
        finishedText = payload.optString("finished", finishedText)

        val specs = payload.optJSONArray("jobs")

        if (title.isEmpty() && specs != null && specs.length() > 0) {
            title = specs.optJSONObject(0)?.optString("title") ?: ""
        }

        if (!startForegroundNow() || specs == null) {
            return
        }

        for (index in 0 until specs.length()) {
            val spec = specs.optJSONObject(index) ?: continue

            try {
                enqueue(spec)
            } catch (exception: JSONException) {
                Log.w(SERVICE_TAG, "skipped an unreadable download: ${exception.message}")
            }
        }
    }

    private fun enqueue(spec: JSONObject) {
        val destination = spec.getString("destination")

        if (jobs.containsKey(destination)) {
            Log.i(SERVICE_TAG, "already downloading $destination, the app attaches to it")

            return
        }

        val expectedBytes = spec.optLong("bytes", 0L)
        var lastPercent = -1
        lateinit var job: PackageDownloadJob

        job = PackageDownloadJob(destination, spec.optString("title"), spec.getString("url"),
            spec.optString("sha256"), expectedBytes) { received ->
            val current = if (expectedBytes > 0) (received * 100 / expectedBytes).toInt() else -1

            if (current != lastPercent) {
                lastPercent = current
                mainHandler.post { updateProgress(job, current) }
            }
        }

        jobs[destination] = job

        Log.i(SERVICE_TAG, "download queued: ${job.title}")

        executor.execute {
            var outcome = PackageDownloadJob.Outcome.Failed

            mainHandler.post { beginJob(job) }

            try {
                outcome = job.run()
            } finally {
                mainHandler.post { finishJob(job, outcome) }
            }
        }
    }

    private fun cancelJob(intent: Intent) {
        val destination = intent.getStringExtra(PackageDownloadClient.EXTRA_DESTINATION) ?: return
        val job = jobs[destination]

        if (job != null) {
            Log.i(SERVICE_TAG, "download cancelled: $destination")

            job.cancel()

            return
        }

        File("$destination.part").delete()
        File("$destination.error").delete()
        File(destination).delete()
    }

    private fun beginJob(job: PackageDownloadJob) {
        runningJob = job
        title = job.title
        percent = -1

        val manager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager

        manager.notify(NOTIFICATION_ID, buildProgressNotification())
    }

    private fun finishJob(job: PackageDownloadJob, outcome: PackageDownloadJob.Outcome) {
        jobs.remove(job.destinationPath)

        if (runningJob === job) {
            runningJob = null
        }

        if (job.wasCancelled && outcome == PackageDownloadJob.Outcome.Completed) {
            File(job.destinationPath).delete()
        }

        Log.i(SERVICE_TAG, "download of ${job.destinationPath} ended: $outcome")

        if (!jobs.isEmpty()) {
            return
        }

        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()

        if (outcome == PackageDownloadJob.Outcome.Completed && !job.wasCancelled && !isAppProcessRunning()) {
            notifyFinished(job.title)
        }
    }

    private fun updateProgress(job: PackageDownloadJob, newPercent: Int) {
        if (runningJob !== job) {
            return
        }

        percent = newPercent

        val manager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager

        manager.notify(NOTIFICATION_ID, buildProgressNotification())
    }

    private fun startForegroundNow(): Boolean {
        val notification = buildProgressNotification()

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                startForeground(NOTIFICATION_ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC)
            } else {
                startForeground(NOTIFICATION_ID, notification)
            }
        } catch (exception: IllegalStateException) {
            Log.w(SERVICE_TAG, "the system refused a foreground start: ${exception.message}")

            return false
        }

        return true
    }

    private fun isAppProcessRunning(): Boolean {
        val manager = getSystemService(ACTIVITY_SERVICE) as ActivityManager

        return manager.runningAppProcesses?.any { it.processName == packageName } == true
    }

    private fun buildProgressNotification(): Notification {
        ensureChannel()

        val builder = baseBuilder()
            .setContentTitle(title)
            .setCategory(Notification.CATEGORY_PROGRESS)
            .setOngoing(true)

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            builder.setForegroundServiceBehavior(Notification.FOREGROUND_SERVICE_IMMEDIATE)
        }

        if (percent >= 0) {
            builder.setContentText("$percent%")
            builder.setProgress(100, percent, false)
        } else {
            builder.setProgress(0, 0, true)
        }

        return builder.build()
    }

    private fun notifyFinished(finishedTitle: String) {
        ensureChannel()

        val notification = baseBuilder()
            .setContentTitle(finishedTitle)
            .setContentText(finishedText)
            .setAutoCancel(true)
            .build()

        val manager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager

        manager.notify(PackageDownloadClient.FINISHED_NOTIFICATION_ID, notification)
    }

    private fun baseBuilder(): Notification.Builder {
        return Notification.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_notification)
            .setLargeIcon(Icon.createWithResource(this, R.mipmap.ic_launcher_round))
            .setColor(ACCENT_COLOR)
            .setContentIntent(launchIntent())
            .setOnlyAlertOnce(true)
            .setShowWhen(false)
    }

    private fun launchIntent(): PendingIntent {
        val intent = Intent(this, IkonikaActivity::class.java)
            .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP)

        return PendingIntent.getActivity(this, LAUNCH_REQUEST_CODE, intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
    }

    private fun ensureChannel() {
        val manager = getSystemService(NOTIFICATION_SERVICE) as NotificationManager

        val channel = NotificationChannel(CHANNEL_ID, channelName, NotificationManager.IMPORTANCE_LOW)
        channel.setShowBadge(false)

        manager.createNotificationChannel(channel)
    }
}
