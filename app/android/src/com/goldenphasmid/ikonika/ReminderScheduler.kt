package com.goldenphasmid.ikonika

import android.app.AlarmManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import java.time.Instant
import java.time.LocalDate
import java.time.LocalDateTime
import java.time.LocalTime
import java.time.ZoneId
import java.time.temporal.ChronoUnit

object ReminderScheduler {
    private const val PREFS_NAME = "ikonika_reminder"
    private const val KEY_ENABLED = "enabled"
    private const val KEY_HOUR = "hour"
    private const val KEY_MINUTE = "minute"
    private const val KEY_MESSAGE = "message"
    private const val KEY_LAST_STUDIED_DAY = "lastStudiedDay"
    private const val REQUEST_CODE = 1001

    @JvmStatic
    fun schedule(context: Context, hour: Int, minute: Int, message: String) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit()
            .putBoolean(KEY_ENABLED, true)
            .putInt(KEY_HOUR, hour)
            .putInt(KEY_MINUTE, minute)
            .putString(KEY_MESSAGE, message)
            .apply()

        armAlarm(context, hour, minute)
    }

    @JvmStatic
    fun cancel(context: Context) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putBoolean(KEY_ENABLED, false).apply()

        val manager = context.getSystemService(Context.ALARM_SERVICE) as AlarmManager
        manager.cancel(pendingIntent(context))
    }

    @JvmStatic
    fun markStudiedToday(context: Context, dayNumber: Int) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putInt(KEY_LAST_STUDIED_DAY, dayNumber).apply()
    }

    @JvmStatic
    fun rescheduleFromSavedState(context: Context) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

        if (!prefs.getBoolean(KEY_ENABLED, false)) {
            return
        }

        armAlarm(context, prefs.getInt(KEY_HOUR, 20), prefs.getInt(KEY_MINUTE, 0))
    }

    @JvmStatic
    fun fireIfNotStudiedToday(context: Context) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

        if (!prefs.getBoolean(KEY_ENABLED, false)) {
            return
        }

        val lastStudiedDay = prefs.getInt(KEY_LAST_STUDIED_DAY, -1)

        if (lastStudiedDay != currentDayNumber()) {
            val message = prefs.getString(KEY_MESSAGE, null) ?: "Time for a quick review."
            PlatformNotification.notify(context, "Ikonika", message)
        }

        armAlarm(context, prefs.getInt(KEY_HOUR, 20), prefs.getInt(KEY_MINUTE, 0))
    }

    private fun armAlarm(context: Context, hour: Int, minute: Int) {
        val manager = context.getSystemService(Context.ALARM_SERVICE) as AlarmManager
        val triggerAt = nextOccurrenceMillis(hour, minute)

        manager.setAndAllowWhileIdle(AlarmManager.RTC_WAKEUP, triggerAt, pendingIntent(context))
    }

    private fun pendingIntent(context: Context): PendingIntent {
        val intent = Intent(context, ReminderAlarmReceiver::class.java)

        return PendingIntent.getBroadcast(
            context, REQUEST_CODE, intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
    }

    private fun nextOccurrenceMillis(hour: Int, minute: Int): Long {
        val zone = ZoneId.systemDefault()
        val now = LocalDateTime.now(zone)
        var target = LocalDateTime.of(now.toLocalDate(), LocalTime.of(hour, minute))

        if (!target.isAfter(now)) {
            target = target.plusDays(1)
        }

        return target.atZone(zone).toInstant().toEpochMilli()
    }

    fun currentDayNumber(): Int {
        val adjusted = Instant.now().minusSeconds(2 * 60 * 60)
        val date = adjusted.atZone(ZoneId.systemDefault()).toLocalDate()

        return ChronoUnit.DAYS.between(LocalDate.of(1970, 1, 1), date).toInt()
    }
}
