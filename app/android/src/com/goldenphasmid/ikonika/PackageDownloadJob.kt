package com.goldenphasmid.ikonika

import android.util.Log
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest

private const val JOB_TAG = "PackageDownloadJob"
private const val TIMEOUT_MILLISECONDS = 30000
private const val BUFFER_BYTES = 64 * 1024
private const val PROGRESS_INTERVAL_MILLISECONDS = 500L

class PackageDownloadJob(
    val destinationPath: String,
    val title: String,
    private val url: String,
    private val sha256: String,
    private val expectedBytes: Long,
    private val onProgress: (Long) -> Unit
) {
    enum class Outcome { Completed, Failed, Cancelled, Interrupted }

    @Volatile
    private var cancelled = false

    @Volatile
    private var interrupted = false

    private val partFile = File("$destinationPath.part")
    private val finalFile = File(destinationPath)
    private val errorFile = File("$destinationPath.error")

    val wasCancelled: Boolean
        get() = cancelled

    fun cancel() {
        cancelled = true
    }

    fun interrupt() {
        interrupted = true
    }

    fun run(): Outcome {
        errorFile.delete()

        val outcome = try {
            transfer()
        } catch (exception: IOException) {
            fail("network", exception.message ?: "")
        } catch (exception: Exception) {
            Log.e(JOB_TAG, "unexpected failure", exception)

            fail("unexpected", exception.message ?: exception.javaClass.simpleName)
        }

        when (outcome) {
            Outcome.Cancelled -> partFile.delete()
            Outcome.Interrupted -> recordError("paused", "")
            else -> {}
        }

        return outcome
    }

    private fun abortOutcome(): Outcome? {
        if (cancelled) {
            return Outcome.Cancelled
        }

        if (interrupted) {
            return Outcome.Interrupted
        }

        return null
    }

    private fun transfer(): Outcome {
        abortOutcome()?.let { return it }

        val existing = if (partFile.exists()) partFile.length() else 0L
        val complete = expectedBytes > 0 && existing == expectedBytes
        val resumeOffset = if (expectedBytes > 0 && existing in 1 until expectedBytes) existing else 0L

        if (!complete) {
            val outcome = download(resumeOffset)

            if (outcome != null) {
                return outcome
            }
        }

        if (expectedBytes > 0 && partFile.length() != expectedBytes) {
            val received = partFile.length()

            partFile.delete()

            return fail("ended_early", received.toString())
        }

        val matches = digestMatches() ?: return abortOutcome() ?: Outcome.Failed

        if (!matches) {
            partFile.delete()

            return fail("checksum", "")
        }

        abortOutcome()?.let { return it }

        finalFile.delete()

        if (!partFile.renameTo(finalFile)) {
            partFile.delete()

            return fail("move", "")
        }

        Log.i(JOB_TAG, "downloaded $destinationPath, checksum ok")

        return Outcome.Completed
    }

    private fun download(resumeOffset: Long): Outcome? {
        partFile.parentFile?.mkdirs()

        val connection = URL(url).openConnection() as HttpURLConnection

        connection.connectTimeout = TIMEOUT_MILLISECONDS
        connection.readTimeout = TIMEOUT_MILLISECONDS
        connection.instanceFollowRedirects = true

        if (resumeOffset > 0) {
            connection.setRequestProperty("Range", "bytes=$resumeOffset-")

            Log.i(JOB_TAG, "resuming $url at $resumeOffset of $expectedBytes bytes")
        } else {
            Log.i(JOB_TAG, "downloading $url to $destinationPath")
        }

        try {
            val status = connection.responseCode

            if (resumeOffset > 0 && status != HttpURLConnection.HTTP_PARTIAL) {
                partFile.delete()

                return fail("whole_file", "")
            }

            if (status !in 200..299) {
                return fail("http", status.toString())
            }

            return copyBody(connection, resumeOffset)
        } finally {
            connection.disconnect()
        }
    }

    private fun copyBody(connection: HttpURLConnection, resumeOffset: Long): Outcome? {
        val buffer = ByteArray(BUFFER_BYTES)
        var received = resumeOffset
        var lastReport = 0L

        FileOutputStream(partFile, resumeOffset > 0).use { output ->
            connection.inputStream.use { input ->
                while (true) {
                    abortOutcome()?.let { return it }

                    val count = input.read(buffer)

                    if (count < 0) {
                        break
                    }

                    output.write(buffer, 0, count)
                    received += count

                    val now = System.currentTimeMillis()

                    if (now - lastReport >= PROGRESS_INTERVAL_MILLISECONDS) {
                        lastReport = now
                        onProgress(received)
                    }
                }
            }
        }

        onProgress(received)

        return null
    }

    private fun digestMatches(): Boolean? {
        if (sha256.isEmpty()) {
            return true
        }

        val digest = MessageDigest.getInstance("SHA-256")
        val buffer = ByteArray(BUFFER_BYTES)

        partFile.inputStream().use { input ->
            while (true) {
                if (abortOutcome() != null) {
                    return null
                }

                val count = input.read(buffer)

                if (count < 0) {
                    break
                }

                digest.update(buffer, 0, count)
            }
        }

        return digest.digest().joinToString("") { "%02x".format(it) } == sha256.lowercase()
    }

    private fun fail(code: String, detail: String): Outcome {
        Log.w(JOB_TAG, "download failed: $code $detail")

        recordError(code, detail)

        return Outcome.Failed
    }

    private fun recordError(code: String, detail: String) {
        try {
            errorFile.writeText("$code\n$detail")
        } catch (exception: IOException) {
            Log.w(JOB_TAG, "could not record the failure: ${exception.message}")
        }
    }
}
