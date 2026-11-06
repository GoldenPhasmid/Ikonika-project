package com.goldenphasmid.ikonika

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.provider.OpenableColumns
import android.util.Log

private const val LOG_TAG = "ReaderFilePicker"

object ReaderFilePicker {
    const val PICK_REQUEST_CODE = 8200

    @JvmStatic
    external fun nativeOnPicked(uri: String, displayName: String)

    @JvmStatic
    external fun nativeOnPickFailed(message: String)

    @JvmStatic
    fun pickDocument(activity: Activity) {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
        }

        try {
            activity.startActivityForResult(intent, PICK_REQUEST_CODE)
        } catch (e: Exception) {
            nativeOnPickFailed(e.message ?: e.javaClass.simpleName)
        }
    }

    @JvmStatic
    fun handleResult(activity: Activity, resultCode: Int, data: Intent?) {
        if (resultCode != Activity.RESULT_OK || data?.data == null) {
            return
        }

        val uri = data.data!!

        try {
            activity.contentResolver.takePersistableUriPermission(
                uri, Intent.FLAG_GRANT_READ_URI_PERMISSION
            )
        } catch (e: Exception) {
            Log.w(LOG_TAG, "could not persist read access: ${e.message}")
        }

        val displayName = queryDisplayName(activity, uri)
        val supported = listOf(".epub", ".pdf", ".fb2", ".fb2.zip", ".txt").any {
            displayName.endsWith(it, ignoreCase = true)
        }

        if (!supported) {
            nativeOnPickFailed("Only .pdf, .epub, .fb2 and .txt books are supported")
            return
        }

        nativeOnPicked(uri.toString(), displayName)
    }

    @JvmStatic
    fun showDocument(activity: Activity, uriString: String) {
        val uri = Uri.parse(uriString)
        val mimeType = activity.contentResolver.getType(uri) ?: "*/*"

        val intent = Intent(Intent.ACTION_VIEW).apply {
            setDataAndType(uri, mimeType)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }

        try {
            activity.startActivity(Intent.createChooser(intent, null))
        } catch (e: Exception) {
            Log.w(LOG_TAG, "could not open viewer for $uriString: ${e.message}")
        }
    }

    private fun queryDisplayName(activity: Activity, uri: android.net.Uri): String {
        activity.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)
            ?.use { cursor ->
                val column = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)

                if (column >= 0 && cursor.moveToFirst()) {
                    return cursor.getString(column) ?: ""
                }
            }

        return ""
    }
}
