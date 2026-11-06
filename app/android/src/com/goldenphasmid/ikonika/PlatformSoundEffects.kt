package com.goldenphasmid.ikonika

import android.content.Context
import android.media.AudioAttributes
import android.media.SoundPool
import android.util.Log

private const val TAG = "PlatformSoundEffects"

object PlatformSoundEffects {
    private val names = listOf("sfx_incorrect", "sfx_complete", "sfx_goal")
    private val soundByName = HashMap<String, Int>()
    private val loaded = HashSet<Int>()

    @Volatile private var pool: SoundPool? = null
    @Volatile private var pendingSound = 0

    @JvmStatic
    fun initialize(context: Context) {
        if (pool != null) {
            return
        }

        val attributes = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_GAME)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build()

        val created = SoundPool.Builder()
            .setMaxStreams(2)
            .setAudioAttributes(attributes)
            .build()

        created.setOnLoadCompleteListener { _, soundId, status ->
            if (status == 0) {
                synchronized(loaded) { loaded.add(soundId) }

                if (pendingSound == soundId) {
                    pendingSound = 0
                    created.play(soundId, 1.0f, 1.0f, 1, 0, 1.0f)
                }
            } else {
                Log.w(TAG, "sound $soundId failed to load, status $status")
            }
        }

        for (name in names) {
            val resource = context.resources.getIdentifier(name, "raw", context.packageName)

            if (resource == 0) {
                Log.w(TAG, "no raw resource named $name")
                continue
            }

            soundByName[name] = created.load(context, resource, 1)
        }

        pool = created
    }

    @JvmStatic
    fun play(name: String) {
        val soundId = soundByName[name] ?: return

        if (!synchronized(loaded) { loaded.contains(soundId) }) {
            Log.i(TAG, "$name is not loaded yet, playing it once it is")
            pendingSound = soundId
            return
        }

        pool?.play(soundId, 1.0f, 1.0f, 1, 0, 1.0f)
    }
}
