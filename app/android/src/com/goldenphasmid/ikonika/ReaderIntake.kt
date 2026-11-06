package com.goldenphasmid.ikonika

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.provider.OpenableColumns
import android.util.Log
import java.io.File

private const val LOG_TAG = "ReaderIntake"

object ReaderIntake {
    @Volatile
    private var pendingPath: String? = null

    @Volatile
    private var pendingName: String = ""

    @JvmStatic
    external fun nativeOnBookArrived()

    @JvmStatic
    fun takePending(): Array<String>? {
        val path = pendingPath ?: return null

        pendingPath = null

        return arrayOf(path, pendingName)
    }

    fun accept(activity: Activity, intent: Intent?) {
        if (intent?.action != Intent.ACTION_VIEW) {
            return
        }

        val uri = intent.data ?: return
        val displayName = queryDisplayName(activity, uri).ifEmpty { uri.lastPathSegment ?: "book" }
        val persistable = intent.flags and Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION != 0

        intent.action = null

        Thread {
            val path = if (persistable && keepAccess(activity, uri)) {
                uri.toString()
            } else {
                copyIntoApp(activity, uri, displayName)
            }

            if (path == null) {
                return@Thread
            }

            pendingName = displayName
            pendingPath = path

            try {
                nativeOnBookArrived()
            } catch (e: UnsatisfiedLinkError) {
                Log.i(LOG_TAG, "book kept until the app is ready: $displayName")
            }
        }.start()
    }

    private fun keepAccess(activity: Activity, uri: Uri): Boolean {
        return try {
            activity.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            true
        } catch (e: Exception) {
            Log.w(LOG_TAG, "could not persist read access: ${e.message}")
            false
        }
    }

    private fun copyIntoApp(activity: Activity, uri: Uri, displayName: String): String? {
        val directory = File(activity.filesDir, "books")
        return try {
            activity.contentResolver.openInputStream(uri)?.use { input ->
                ReaderImportFiles.copy(directory, displayName, input).absolutePath
            }
        } catch (e: Exception) {
            Log.w(LOG_TAG, "could not copy $uri: ${e.message}")
            null
        }
    }

    private fun queryDisplayName(activity: Activity, uri: Uri): String {
        try {
            activity.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
                ?.use { cursor ->
                    val column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)

                    if (column >= 0 && cursor.moveToFirst()) {
                        return cursor.getString(column) ?: ""
                    }
                }
        } catch (e: Exception) {
            Log.w(LOG_TAG, "could not read the name of $uri: ${e.message}")
        }

        return ""
    }
}
