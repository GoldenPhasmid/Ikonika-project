package com.goldenphasmid.ikonika

import java.io.File
import java.io.IOException
import java.io.InputStream

object ReaderImportFiles {
    fun copy(directory: File, displayName: String, input: InputStream): File {
        if (!directory.isDirectory && !directory.mkdirs() && !directory.isDirectory) {
            throw IOException("Could not create book directory")
        }

        val name = displayName.replace(Regex("[\\\\/:*?\"<>|]"), "_")
            .takeUnless { it.isEmpty() || it == "." || it == ".." } ?: "book"
        val extensionStart = if (name.endsWith(".fb2.zip", ignoreCase = true)) {
            name.length - 8
        } else {
            name.lastIndexOf('.').takeIf { it > 0 } ?: name.length
        }
        val stem = name.substring(0, extensionStart)
        val extension = name.substring(extensionStart)
        var target = File(directory, name)
        var suffix = 1

        while (!target.createNewFile()) {
            target = File(directory, "$stem (${suffix++})$extension")
        }

        try {
            target.outputStream().use { output -> input.copyTo(output) }
            return target
        } catch (exception: Exception) {
            target.delete()
            throw exception
        }
    }
}
