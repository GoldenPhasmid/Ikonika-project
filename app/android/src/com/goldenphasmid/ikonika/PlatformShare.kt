package com.goldenphasmid.ikonika

import android.content.ClipData
import android.content.Context
import android.content.Intent
import androidx.core.content.FileProvider
import java.io.File

object PlatformShare {
    @JvmStatic
    fun shareFile(context: Context, path: String, mimeType: String, title: String): Boolean {
        val file = File(path)

        if (!file.exists()) {
            return false
        }

        val uri = try {
            FileProvider.getUriForFile(context, context.packageName + ".qtprovider", file)
        } catch (exception: IllegalArgumentException) {
            return false
        }

        val send = Intent(Intent.ACTION_SEND)
            .setType(mimeType)
            .putExtra(Intent.EXTRA_STREAM, uri)
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)

        send.clipData = ClipData.newRawUri(file.name, uri)

        val chooser = Intent.createChooser(send, title)

        if (context !is android.app.Activity) {
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }

        context.startActivity(chooser)

        return true
    }
}
