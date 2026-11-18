package com.goldenphasmid.ikonika

import android.content.Context
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import android.speech.tts.Voice
import android.util.Log
import java.util.Locale
import java.util.concurrent.ConcurrentHashMap

private const val TAG = "PlatformSpeech"
private const val NARRATION_PREFIX = "narration-"
private const val WARMUP_ID = "warmup"
private const val WARMUP_TIMEOUT_MS = 10_000L
private const val MAX_CONSECUTIVE_REBINDS = 3

object PlatformSpeech {
    @JvmStatic
    external fun nativeOnSpeakingChanged(speaking: Boolean)

    @JvmStatic
    external fun nativeOnNarrationStarted(id: Int)

    @JvmStatic
    external fun nativeOnNarrationFinished(id: Int)

    @JvmStatic
    external fun nativeOnNarrationFailed()

    @JvmStatic
    external fun nativeOnDiagnostic(message: String)

    @Volatile private var currentUtterance: String? = null
    @Volatile private var engine: TextToSpeech? = null
    @Volatile private var ready = false
    @Volatile private var warmupComplete = false
    @Volatile private var initReported = false
    @Volatile private var rate = 1.0f
    @Volatile private var engineGeneration = 0
    @Volatile private var consecutiveRebinds = 0
    @Volatile private var engineStartedAt = 0L
    @Volatile private var applicationContext: Context? = null
    private val voiceByLanguage = ConcurrentHashMap<String, Voice>()
    private val preferredRegions = mapOf("pt" to "PT")
    private val mainHandler = Handler(Looper.getMainLooper())

    private class Request(val text: String, val languageCode: String, val rateScale: Float)

    private class Narration(val id: Int, val text: String, val languageCode: String, val rateScale: Float)

    private val requestLock = Any()
    private var heldRequest: Request? = null
    private val heldNarration = ArrayList<Narration>()
    private val spokenNarration = ArrayList<Narration>()
    private var narrationPaused = false
    private val narrationOffsets = ConcurrentHashMap<Int, Int>()

    @JvmStatic
    fun initialize(context: Context) {
        if (engine != null) {
            return
        }

        applicationContext = context.applicationContext
        startEngine(context.applicationContext)
    }

    private fun report(message: String) {
        Log.i(TAG, message)

        try {
            nativeOnDiagnostic(message)
        } catch (exception: UnsatisfiedLinkError) {
            Log.w(TAG, "diagnostic bridge not registered")
        }
    }

    private fun sinceEngineStart(): Long = SystemClock.elapsedRealtime() - engineStartedAt

    private fun startEngine(context: Context) {
        engineGeneration += 1
        val generation = engineGeneration

        initReported = false
        engineStartedAt = SystemClock.elapsedRealtime()

        report("engine starting, generation $generation")

        mainHandler.postDelayed({ onWarmupTimeout(generation) }, WARMUP_TIMEOUT_MS)

        engine = TextToSpeech(context, { status -> onEngineInit(generation, status) }, "com.google.android.tts")
    }

    private fun onWarmupTimeout(generation: Int) {
        if (generation != engineGeneration || warmupComplete) {
            return
        }

        if (!initReported) {
            report("engine did not report init within ${WARMUP_TIMEOUT_MS}ms")

            if (rebind("engine init never reported")) {
                return
            }
        } else {
            report("warm-up produced no result within ${WARMUP_TIMEOUT_MS}ms, releasing held speech")
        }

        finishWarmup()
    }

    private fun rebind(reason: String): Boolean {
        val context = applicationContext ?: return false

        synchronized(requestLock) {
            if (consecutiveRebinds >= MAX_CONSECUTIVE_REBINDS) {
                report("not rebinding after $consecutiveRebinds attempts without audio: $reason")

                return false
            }

            consecutiveRebinds += 1
            warmupComplete = false
            ready = false
        }

        report("rebinding the engine, attempt $consecutiveRebinds: $reason")

        mainHandler.post {
            val old = engine
            engine = null
            voiceByLanguage.clear()

            try {
                old?.shutdown()
            } catch (exception: RuntimeException) {
                report("old engine shutdown failed: ${exception.message}")
            }

            startEngine(context)
        }

        return true
    }

    private fun onEngineInit(generation: Int, status: Int) {
        if (generation != engineGeneration) {
            return
        }

        initReported = true
        ready = status == TextToSpeech.SUCCESS

        report("engine init status $status after ${sinceEngineStart()}ms")

        if (!ready) {
            if (!rebind("engine init failed with status $status")) {
                finishWarmup()
            }

            return
        }

        engine?.setSpeechRate(rate)
        engine?.setOnUtteranceProgressListener(object : UtteranceProgressListener() {
            override fun onStart(id: String?) {
                if (id != null && id != WARMUP_ID) {
                    consecutiveRebinds = 0
                }

                narrationId(id)?.let {
                    reportNarration { nativeOnNarrationStarted(it) }

                    return
                }

                if (id != null && id == currentUtterance) {
                    reportSpeaking(true)
                }

                val calledAt = callTimestamps.remove(id) ?: return
                val elapsedMs = (System.nanoTime() - calledAt) / 1_000_000

                report("$id: speak() to audio start ${elapsedMs}ms")
            }

            override fun onRangeStart(id: String?, start: Int, end: Int, frame: Int) {
                narrationId(id)?.let { narrationOffsets[it] = start }
            }

            override fun onDone(id: String?) {
                narrationId(id)?.let {
                    forgetNarration(it)
                    reportNarration { nativeOnNarrationFinished(it) }

                    return
                }

                if (id == WARMUP_ID) {
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

                if (id == WARMUP_ID) {
                    finishWarmup()
                }

                reportFinished(id)
            }

            override fun onStop(id: String?, interrupted: Boolean) {
                if (id == WARMUP_ID) {
                    finishWarmup()
                }

                reportFinished(id)
            }

            override fun onError(id: String?, errorCode: Int) {
                report("$id: error code=$errorCode")

                if (narrationId(id) != null) {
                    reportNarration { nativeOnNarrationFailed() }

                    return
                }

                if (id == WARMUP_ID) {
                    finishWarmup()
                }

                reportFinished(id)

                if (errorCode == TextToSpeech.ERROR_SERVICE && id != WARMUP_ID) {
                    rebind("synthesis reported a service error")
                }
            }
        })
        warmUp()
    }

    private val callTimestamps = ConcurrentHashMap<String, Long>()

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
            if (warmupComplete) {
                return
            }

            warmupComplete = true

            narrations.addAll(heldNarration)
            heldNarration.clear()

            val taken = heldRequest
            heldRequest = null
            taken
        }

        report("warm-up complete after ${sinceEngineStart()}ms, held request = ${held != null}")

        if (held != null) {
            speak(held.text, held.languageCode, held.rateScale)
        }

        for (narration in narrations) {
            narrate(narration.id, narration.text, narration.languageCode, narration.rateScale)
        }
    }

    private fun applyVoice(tts: TextToSpeech, languageCode: String) {
        val voice = voiceFor(tts, languageCode)

        if (voice != null) {
            if (tts.voice != voice) {
                tts.voice = voice
            }
        } else {
            tts.language = localeFor(languageCode)
        }
    }

    private fun localeFor(languageCode: String): Locale {
        val region = preferredRegions[languageCode] ?: return Locale(languageCode)

        return Locale(languageCode, region)
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

        val result = tts.speak("bonjour", TextToSpeech.QUEUE_FLUSH, params, WARMUP_ID)

        if (result != TextToSpeech.SUCCESS) {
            report("warm-up speak was rejected, result=$result")
            finishWarmup()
        }
    }

    private fun voiceFor(tts: TextToSpeech, languageCode: String): Voice? {
        voiceByLanguage[languageCode]?.let { return it }

        val locale = localeFor(languageCode)
        tts.language = locale

        val candidates = (listOfNotNull(tts.voice) + tts.voices.orEmpty()).filter {
            it.locale.language == locale.language && !it.isNetworkConnectionRequired
        }
        val chosen = candidates.firstOrNull {
            locale.country.isEmpty() || it.locale.country == locale.country
        } ?: candidates.firstOrNull() ?: return null

        if (locale.country.isNotEmpty() && chosen.locale.country != locale.country) {
            report("no offline ${locale.toLanguageTag()} voice, falling back to ${chosen.locale.toLanguageTag()}")
        }

        report("voice for $languageCode: ${chosen.name}")
        voiceByLanguage[languageCode] = chosen

        return chosen
    }

    private fun holdAndRebind(request: Request, reason: String) {
        synchronized(requestLock) {
            heldRequest = request
        }

        if (rebind(reason)) {
            return
        }

        synchronized(requestLock) {
            if (heldRequest === request) {
                heldRequest = null
            }
        }
    }

    @JvmStatic
    fun speak(text: String, languageCode: String, rateScale: Float) {
        synchronized(requestLock) {
            if (!warmupComplete) {
                report("engine is not ready, holding the request")
                heldRequest = Request(text, languageCode, rateScale)

                return
            }
        }

        val tts = engine

        if (tts == null || !ready) {
            holdAndRebind(Request(text, languageCode, rateScale), "speak without a ready engine")

            return
        }

        applyVoice(tts, languageCode)

        tts.setSpeechRate(rate * rateScale)

        val utteranceId = "ikonika-${System.nanoTime()}"
        callTimestamps[utteranceId] = System.nanoTime()
        currentUtterance = utteranceId

        val result = tts.speak(text, TextToSpeech.QUEUE_FLUSH, Bundle(), utteranceId)

        if (result != TextToSpeech.SUCCESS) {
            callTimestamps.remove(utteranceId)
            report("speak was rejected, result=$result")
            holdAndRebind(Request(text, languageCode, rateScale), "speak rejected with $result")
        }
    }

    @JvmStatic
    fun narrate(id: Int, text: String, languageCode: String, rateScale: Float) {
        val narration = Narration(id, text, languageCode, rateScale)

        synchronized(requestLock) {
            if (!warmupComplete) {
                heldNarration.add(narration)

                return
            }

            spokenNarration.add(narration)

            if (narrationPaused) {
                return
            }
        }

        speakNarration(narration)
    }

    private fun speakNarration(narration: Narration): Boolean {
        val tts = engine

        if (tts == null || !ready) {
            report("narration requested without a speech engine")
            reportNarration { nativeOnNarrationFailed() }

            return false
        }

        applyVoice(tts, narration.languageCode)
        tts.setSpeechRate(rate * narration.rateScale)

        val result = tts.speak(narration.text, TextToSpeech.QUEUE_ADD, Bundle(), "$NARRATION_PREFIX${narration.id}")

        if (result != TextToSpeech.SUCCESS) {
            report("narration speak was rejected, result=$result")
            reportNarration { nativeOnNarrationFailed() }
            rebind("narration rejected with $result")

            return false
        }

        return true
    }

    private fun forgetNarration(id: Int) {
        synchronized(requestLock) {
            spokenNarration.removeAll { it.id == id }
        }

        narrationOffsets.remove(id)
    }

    @JvmStatic
    fun stopNarration() {
        synchronized(requestLock) {
            heldNarration.clear()
            spokenNarration.clear()
            narrationPaused = false
        }

        narrationOffsets.clear()
        engine?.stop()
    }

    @JvmStatic
    fun pauseNarration(): Boolean {
        synchronized(requestLock) {
            if (spokenNarration.isEmpty() || narrationPaused) {
                return false
            }

            narrationPaused = true
        }

        engine?.stop()

        return true
    }

    @JvmStatic
    fun resumeNarration(): Boolean {
        val pending = synchronized(requestLock) {
            if (!narrationPaused) {
                return false
            }

            narrationPaused = false

            if (spokenNarration.isEmpty()) {
                return false
            }

            val first = spokenNarration[0]
            val offset = narrationOffsets.remove(first.id) ?: 0

            if (offset > 0 && offset < first.text.length) {
                spokenNarration[0] = Narration(first.id, first.text.substring(offset), first.languageCode, first.rateScale)
            }

            ArrayList(spokenNarration)
        }

        report("narration resumes ${pending.size} sentences, the first from \"${pending[0].text.take(24)}\"")

        for (narration in pending) {
            if (!speakNarration(narration)) {
                break
            }
        }

        return true
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
