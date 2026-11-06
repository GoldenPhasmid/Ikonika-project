package com.goldenphasmid.ikonika

import android.app.PendingIntent
import android.appwidget.AppWidgetManager
import android.appwidget.AppWidgetProvider
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.RectF
import android.os.Bundle
import android.util.Log
import android.view.View
import android.widget.RemoteViews

class HomeWidget : AppWidgetProvider() {
    override fun onUpdate(context: Context, manager: AppWidgetManager, widgetIds: IntArray) {
        render(context, manager, widgetIds)
    }

    override fun onAppWidgetOptionsChanged(
        context: Context, manager: AppWidgetManager, widgetId: Int, options: Bundle) {
        render(context, manager, intArrayOf(widgetId))
    }

    companion object {
        private const val PREFS_NAME = "ikonika_widget"
        private const val KEY_DAY = "day"
        private const val KEY_DONE = "done"
        private const val KEY_GOAL = "goal"
        private const val KEY_REVIEWED = "reviewed"
        private const val KEY_DUE = "due"
        private const val KEY_DUE_TOMORROW = "dueTomorrow"
        private const val KEY_PANEL = "panel"
        private const val KEY_MUTED = "muted"
        private const val KEY_REVIEW = "review"
        private const val KEY_LEARN = "learn"
        private const val REQUEST_CODE = 1002
        private const val REVIEW_REQUEST_CODE = 1003
        private const val LEARN_REQUEST_CODE = 1004
        private const val EXTRA_SESSION = "com.goldenphasmid.ikonika.SESSION"
        private const val SESSION_REVIEW = "review"
        private const val SESSION_LEARN = "learn"
        private const val LOG_TAG = "HomeWidget"
        private const val RING_SIZE_DP = 120
        private const val RING_STROKE_RATIO = 0.115f
        private const val RING_GAP_RATIO = 0.03f
        private const val TRACK_ALPHA = 0x40
        private const val FALLBACK_WIDTH_DP = 140
        private const val FALLBACK_HEIGHT_DP = 140
        private const val TALL_MIN_HEIGHT_DP = 110
        private const val WIDE_RATIO = 1.6f
        private const val COMPACT_MAX_WIDTH_DP = 200
        private const val COMPACT_RING_INSET_DP = 8

        @Volatile
        private var pendingSession: String? = null

        @JvmStatic
        external fun nativeOnSessionRequested()

        @JvmStatic
        fun takePendingSession(): String? {
            val session = pendingSession

            pendingSession = null

            return session
        }

        fun acceptLaunch(intent: Intent?) {
            val session = intent?.getStringExtra(EXTRA_SESSION) ?: return

            intent.removeExtra(EXTRA_SESSION)
            pendingSession = session

            try {
                nativeOnSessionRequested()
            } catch (e: UnsatisfiedLinkError) {
                Log.i(LOG_TAG, "session request kept until the app is ready: $session")
            }
        }

        @JvmStatic
        fun publish(
            context: Context, done: Int, goal: Int, reviewed: Int, due: Int, dueTomorrow: Int,
            panel: Int, muted: Int, review: Int, learn: Int) {
            context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE).edit()
                .putInt(KEY_DAY, ReminderScheduler.currentDayNumber())
                .putInt(KEY_DONE, done)
                .putInt(KEY_GOAL, goal)
                .putInt(KEY_REVIEWED, reviewed)
                .putInt(KEY_DUE, due)
                .putInt(KEY_DUE_TOMORROW, dueTomorrow)
                .putInt(KEY_PANEL, panel)
                .putInt(KEY_MUTED, muted)
                .putInt(KEY_REVIEW, review)
                .putInt(KEY_LEARN, learn)
                .apply()

            val manager = AppWidgetManager.getInstance(context)
            val widgetIds = manager.getAppWidgetIds(ComponentName(context, HomeWidget::class.java))

            render(context, manager, widgetIds)
        }

        private fun render(context: Context, manager: AppWidgetManager, widgetIds: IntArray) {
            val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
            val isStale = prefs.getInt(KEY_DAY, -1) != ReminderScheduler.currentDayNumber()

            val goal = prefs.getInt(KEY_GOAL, 0)
            val done = if (isStale) 0 else prefs.getInt(KEY_DONE, 0)
            val reviewed = if (isStale) 0 else prefs.getInt(KEY_REVIEWED, 0)
            val due = prefs.getInt(KEY_DUE, 0) + (if (isStale) prefs.getInt(KEY_DUE_TOMORROW, 0) else 0)

            val panel = prefs.getInt(KEY_PANEL, 0xFF262624.toInt())
            val muted = prefs.getInt(KEY_MUTED, 0xFF9C9A92.toInt())
            val review = prefs.getInt(KEY_REVIEW, 0xFF5B9BF5.toInt())
            val learn = prefs.getInt(KEY_LEARN, 0xFF4CAF7D.toInt())

            val published = prefs.contains(KEY_DAY)
            val owed = reviewed + due
            val showReview = !published || owed > 0
            val showLearn = !published || goal > 0
            val reviewRatio = if (owed > 0) reviewed.toFloat() / owed else 0f
            val learnRatio = if (goal > 0) (done.toFloat() / goal).coerceIn(0f, 1f) else 0f

            val density = context.resources.displayMetrics.density
            val ringSize = (RING_SIZE_DP * density).toInt().coerceAtLeast(1)
            val rings = ringsBitmap(ringSize, reviewRatio, learnRatio, review, learn, showReview, showLearn)

            for (widgetId in widgetIds) {
                val options = manager.getAppWidgetOptions(widgetId)
                val widthDp = options.getInt(AppWidgetManager.OPTION_APPWIDGET_MIN_WIDTH, FALLBACK_WIDTH_DP)
                val heightDp = options.getInt(AppWidgetManager.OPTION_APPWIDGET_MAX_HEIGHT, FALLBACK_HEIGHT_DP)
                val tall = heightDp >= TALL_MIN_HEIGHT_DP && widthDp < heightDp * WIDE_RATIO
                val compact = !tall && widthDp < COMPACT_MAX_WIDTH_DP
                val ringInset = if (compact) (COMPACT_RING_INSET_DP * density).toInt() else 0

                val views = RemoteViews(
                    context.packageName, if (tall) R.layout.widget_home_tall else R.layout.widget_home)

                views.setInt(R.id.widget_background, "setColorFilter", panel)
                views.setImageViewBitmap(R.id.widget_progress, rings)
                views.setViewPadding(R.id.widget_progress, 0, ringInset, 0, ringInset)

                views.setViewVisibility(R.id.widget_hint, if (published) View.GONE else View.VISIBLE)
                views.setTextColor(R.id.widget_hint, muted)

                views.setViewVisibility(R.id.widget_actions, if (published) View.VISIBLE else View.GONE)
                views.setInt(R.id.widget_review_button_background, "setColorFilter", review)
                views.setInt(R.id.widget_learn_button_background, "setColorFilter", learn)
                views.setTextColor(R.id.widget_review_button_label, panel)
                views.setTextColor(R.id.widget_learn_button_label, panel)
                views.setOnClickPendingIntent(R.id.widget_review_button,
                    sessionIntent(context, SESSION_REVIEW, REVIEW_REQUEST_CODE))
                views.setOnClickPendingIntent(R.id.widget_learn_button,
                    sessionIntent(context, SESSION_LEARN, LEARN_REQUEST_CODE))

                views.setOnClickPendingIntent(R.id.widget_root, launchIntent(context))

                manager.updateAppWidget(widgetId, views)
            }
        }

        private fun ringsBitmap(
            size: Int, reviewRatio: Float, learnRatio: Float, review: Int, learn: Int,
            showReview: Boolean, showLearn: Boolean): Bitmap {
            val stroke = size * RING_STROKE_RATIO
            val bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888)
            val canvas = Canvas(bitmap)

            if (showReview) {
                drawRing(canvas, size, stroke / 2f, stroke, reviewRatio, review)
            }

            if (showLearn) {
                drawRing(canvas, size, stroke * 1.5f + size * RING_GAP_RATIO, stroke, learnRatio, learn)
            }

            return bitmap
        }

        private fun drawRing(canvas: Canvas, size: Int, inset: Float, stroke: Float, ratio: Float, colour: Int) {
            val paint = Paint(Paint.ANTI_ALIAS_FLAG)
            val bounds = RectF(inset, inset, size - inset, size - inset)

            paint.style = Paint.Style.STROKE
            paint.strokeWidth = stroke
            paint.strokeCap = Paint.Cap.ROUND

            paint.color = (colour and 0x00FFFFFF) or (TRACK_ALPHA shl 24)
            canvas.drawOval(bounds, paint)

            if (ratio > 0f) {
                paint.color = colour
                canvas.drawArc(bounds, -90f, 360f * ratio.coerceIn(0f, 1f), false, paint)
            }
        }

        private fun sessionIntent(context: Context, session: String, requestCode: Int): PendingIntent {
            val intent = Intent(context, IkonikaActivity::class.java)
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP)
                .putExtra(EXTRA_SESSION, session)

            return PendingIntent.getActivity(
                context, requestCode, intent,
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
        }

        private fun launchIntent(context: Context): PendingIntent {
            val intent = context.packageManager.getLaunchIntentForPackage(context.packageName)

            return PendingIntent.getActivity(
                context, REQUEST_CODE, intent,
                PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
        }
    }
}
