package com.goldenphasmid.ikonika

import android.util.Log
import com.google.mlkit.common.model.DownloadConditions
import com.google.mlkit.common.model.RemoteModelManager
import com.google.mlkit.nl.translate.TranslateLanguage
import com.google.mlkit.nl.translate.TranslateRemoteModel
import com.google.mlkit.nl.translate.Translation
import com.google.mlkit.nl.translate.Translator
import com.google.mlkit.nl.translate.TranslatorOptions

private const val TAG = "PlatformTranslation"

object PlatformTranslation {
    @JvmStatic
    external fun nativeOnModelState(source: String, target: String, downloaded: Boolean)

    @JvmStatic
    external fun nativeOnDownloadFinished(source: String, target: String, success: Boolean, error: String)

    @JvmStatic
    external fun nativeOnTranslated(generation: Long, text: String, result: String)

    private val lock = Any()
    private var translator: Translator? = null
    private var translatorPair: String? = null

    @JvmStatic
    fun isSupported(source: String, target: String): Boolean {
        return source != target
            && TranslateLanguage.fromLanguageTag(source) != null
            && TranslateLanguage.fromLanguageTag(target) != null
    }

    @JvmStatic
    fun checkModel(source: String, target: String) {
        if (!isSupported(source, target)) {
            nativeOnModelState(source, target, false)

            return
        }

        val needed = listOfNotNull(TranslateLanguage.fromLanguageTag(source), TranslateLanguage.fromLanguageTag(target))
            .filter { it != TranslateLanguage.ENGLISH }

        RemoteModelManager.getInstance().getDownloadedModels(TranslateRemoteModel::class.java)
            .addOnSuccessListener { models ->
                val downloaded = models.map { it.language }.toSet()

                nativeOnModelState(source, target, downloaded.containsAll(needed))
            }
            .addOnFailureListener { exception ->
                Log.w(TAG, "could not list downloaded models", exception)

                nativeOnModelState(source, target, false)
            }
    }

    private fun translatorFor(source: String, target: String): Translator? {
        synchronized(lock) {
            val pair = "$source-$target"

            if (translatorPair == pair) {
                return translator
            }

            translator?.close()
            translator = null
            translatorPair = null

            val sourceLanguage = TranslateLanguage.fromLanguageTag(source) ?: return null
            val targetLanguage = TranslateLanguage.fromLanguageTag(target) ?: return null

            val options = TranslatorOptions.Builder()
                .setSourceLanguage(sourceLanguage)
                .setTargetLanguage(targetLanguage)
                .build()

            Log.i(TAG, "translator created for $pair")

            translator = Translation.getClient(options)
            translatorPair = pair

            return translator
        }
    }

    @JvmStatic
    fun download(source: String, target: String) {
        val engine = translatorFor(source, target)

        if (engine == null) {
            nativeOnDownloadFinished(source, target, false, "unsupported language pair")

            return
        }

        engine.downloadModelIfNeeded(DownloadConditions.Builder().build())
            .addOnSuccessListener {
                nativeOnDownloadFinished(source, target, true, "")
            }
            .addOnFailureListener { exception ->
                Log.w(TAG, "model download failed for $source-$target", exception)

                nativeOnDownloadFinished(source, target, false, exception.message ?: "")
            }
    }

    @JvmStatic
    fun translate(generation: Long, text: String, source: String, target: String) {
        val engine = translatorFor(source, target)

        if (engine == null) {
            nativeOnTranslated(generation, text, "")

            return
        }

        engine.translate(text)
            .addOnSuccessListener { result ->
                nativeOnTranslated(generation, text, result)
            }
            .addOnFailureListener { exception ->
                Log.w(TAG, "translation failed", exception)

                nativeOnTranslated(generation, text, "")
            }
    }
}
