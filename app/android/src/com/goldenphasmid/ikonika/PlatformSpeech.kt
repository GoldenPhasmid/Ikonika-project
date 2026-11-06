package com.goldenphasmid.ikonika

import android.content.Context
import android.os.Bundle
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import android.speech.tts.Voice
import android.util.Log
import java.util.Locale

private const val TAG = "PlatformSpeech"
private const val NARRATION_PREFIX = "narration-"

object PlatformSpeech {
    @JvmStatic
    external fun nativeOnSpeakingChanged(speaking: Boolean)

    @JvmStatic
    external fun nativeOnNarrationStarted(id: Int)

    @JvmStatic
    external fun nativeOnNarrationFinished(id: Int)

    @JvmStatic
    external fun nativeOnNarrationFailed()

    @Volatile private var currentUtterance: String? = null
    @Volatile private var engine: TextToSpeech? = null
    @Volatile private var ready = false
    @Volatile private var warmupComplete = false
    @Volatile private var rate = 1.0f
    private val voiceByLanguage = HashMap<String, Voice>()

    private class Request(val text: String, val languageCode: String, val rateScale: Float)

    private class Narration(val id: Int, val text: String, val languageCode: String)

    private val requestLock = Any()
    private var heldRequest: Request? = null
    private val heldNarration = ArrayList<Narration>()

    @JvmStatic
    fun initialize(context: Context) {
        if (engine != null) {
            return
        }

        engine = TextToSpeech(context, { status ->
            ready = status == TextToSpeech.SUCCESS
            Log.i(TAG, "init status: $status")

            if (ready) {
                engine?.setSpeechRate(rate)
                engine?.setOnUtteranceProgressListener(object : UtteranceProgressListener() {
                    override fun onStart(id: String?) {
                        narrationId(id)?.let {
                            reportNarration { nativeOnNarrationStarted(it) }

                            return
                        }

                        if (id != null && id == currentUtterance) {
                            reportSpeaking(true)
                        }

                        val calledAt = callTimestamps.remove(id) ?: return
                        val elapsedMs = (System.nanoTime() - calledAt) / 1_000_000

                        Log.i(TAG, "$id: speak() to audio start ${elapsedMs}ms")
                    }

                    override fun onDone(id: String?) {
                        narrationId(id)?.let {
                            reportNarration { nativeOnNarrationFinished(it) }

                            return
                        }

                        if (id == "warmup") {
                            finishWarmup()
                        }

                        reportFinished(id)
                    }

                    override fun onError(id: String?) {
                        if (narrationId(id) != null) {
                            Log.w(TAG, "$id: narration failed")
                            reportNarration { nativeOnNarrationFailed() }

                            return
                        }

                        if (id == "warmup") {
                            finishWarmup()
                        }

                        reportFinished(id)
                    }

                    override fun onStop(id: String?, interrupted: Boolean) {
                        if (id == "warmup") {
                            finishWarmup()
                        }

                        reportFinished(id)
                    }

                    override fun onError(id: String?, errorCode: Int) {
                        Log.i(TAG, "$id: error code=$errorCode")

                        if (narrationId(id) != null) {
                            reportNarration { nativeOnNarrationFailed() }

                            return
                        }

                        if (id == "warmup") {
                            finishWarmup()
                        }

                        reportFinished(id)
                    }
                })
                warmUp()
            } else {
                finishWarmup()
            }
        }, "com.google.android.tts")
    }

    private val callTimestamps = HashMap<String, Long>()

    private fun reportFinished(id: String?) {
        if (id != null && id == currentUtterance) {
            reportSpeaking(false)
        }
    }

    private fun reportSpeaking(speaking: Boolean) {
        try {
            nativeOnSpeakingChanged(speaking)
        } catch (exception: UnsatisfiedLinkError) {
            Log.w(TAG, "speaking bridge not registered")
        }
    }

    private fun narrationId(id: String?): Int? {
        if (id == null || !id.startsWith(NARRATION_PREFIX)) {
            return null
        }

        return id.removePrefix(NARRATION_PREFIX).toIntOrNull()
    }

    private fun reportNarration(report: () -> Unit) {
        try {
            report()
        } catch (exception: UnsatisfiedLinkError) {
            Log.w(TAG, "narration bridge not registered")
        }
    }

    private fun finishWarmup() {
        val narrations = ArrayList<Narration>()
        val held = synchronized(requestLock) {
            warmupComplete = true

            narrations.addAll(heldNarration)
            heldNarration.clear()

            val taken = heldRequest
            heldRequest = null
            taken
        }

        if (held != null) {
            Log.i(TAG, "speaking the request held during warmup")
            speak(held.text, held.languageCode, held.rateScale)
        }

        for (narration in narrations) {
            narrate(narration.id, narration.text, narration.languageCode)
        }
    }

    private fun applyVoice(tts: TextToSpeech, languageCode: String) {
        val voice = voiceFor(tts, languageCode)

        if (voice != null) {
            if (tts.voice != voice) {
                tts.voice = voice
            }
        } else {
            tts.language = Locale(languageCode)
        }
    }

    private fun warmUp() {
        val tts = engine ?: return
        val voice = voiceFor(tts, "fr")

        if (voice != null) {
            tts.voice = voice
        } else {
            tts.language = Locale("fr")
        }

        val params = Bundle()
        params.putFloat(TextToSpeech.Engine.KEY_PARAM_VOLUME, 0.0f)

        tts.speak("bonjour", TextToSpeech.QUEUE_FLUSH, params, "warmup")
    }

    private fun voiceFor(tts: TextToSpeech, languageCode: String): Voice? {
        voiceByLanguage[languageCode]?.let { return it }

        val locale = Locale(languageCode)
        tts.language = locale

        val engineDefault = tts.voice?.takeIf {
            it.locale.language == locale.language && !it.isNetworkConnectionRequired
        }
        val chosen = engineDefault ?: tts.voices?.firstOrNull {
            it.locale.language == locale.language && !it.isNetworkConnectionRequired
        } ?: return null

        Log.i(TAG, "voice for $languageCode: ${chosen.name}")
        voiceByLanguage[languageCode] = chosen

        return chosen
    }

    @JvmStatic
    fun speak(text: String, languageCode: String, rateScale: Float) {
        synchronized(requestLock) {
            if (!warmupComplete) {
                Log.i(TAG, "engine is not ready, holding the request")
                heldRequest = Request(text, languageCode, rateScale)

                return
            }
        }

        val tts = engine

        if (tts == null || !ready) {
            return
        }

        applyVoice(tts, languageCode)

        tts.setSpeechRate(rate * rateScale)

        val utteranceId = "ikonika-${System.nanoTime()}"
        callTimestamps[utteranceId] = System.nanoTime()
        currentUtterance = utteranceId

        tts.speak(text, TextToSpeech.QUEUE_FLUSH, Bundle(), utteranceId)
    }

    @JvmStatic
    fun narrate(id: Int, text: String, languageCode: String) {
        synchronized(requestLock) {
            if (!warmupComplete) {
                heldNarration.add(Narration(id, text, languageCode))

                return
            }
        }

        val tts = engine

        if (tts == null || !ready) {
            Log.w(TAG, "narration requested without a speech engine")
            reportNarration { nativeOnNarrationFailed() }

            return
        }

        applyVoice(tts, languageCode)
        tts.setSpeechRate(rate)

        tts.speak(text, TextToSpeech.QUEUE_ADD, Bundle(), "$NARRATION_PREFIX$id")
    }

    @JvmStatic
    fun stopNarration() {
        synchronized(requestLock) {
            heldNarration.clear()
        }

        engine?.stop()
    }

    @JvmStatic
    fun stop() {
        synchronized(requestLock) {
            heldRequest = null
        }

        engine?.stop()
    }

    @JvmStatic
    fun setRate(value: Float) {
        rate = value
        engine?.setSpeechRate(value)
    }
}
