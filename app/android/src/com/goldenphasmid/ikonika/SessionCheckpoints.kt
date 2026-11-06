package com.goldenphasmid.ikonika

import android.content.Context
import org.json.JSONObject

object SessionCheckpoints {
    private const val PREFS_NAME = "session_checkpoints"

    @JvmStatic
    fun save(context: Context, kind: String, sessionKey: String, json: String) {
        prefs(context).edit().putString(keyFor(kind, sessionKey), json).apply()
    }

    @JvmStatic
    fun clear(context: Context, kind: String, sessionKey: String) {
        prefs(context).edit().remove(keyFor(kind, sessionKey)).apply()
    }

    @JvmStatic
    fun loadAll(context: Context, kind: String): String {
        val prefix = "$kind|"
        val result = JSONObject()

        for ((key, value) in prefs(context).all) {
            if (key.startsWith(prefix) && value is String) {
                result.put(key.substring(prefix.length), value)
            }
        }

        return result.toString()
    }

    private fun prefs(context: Context) =
        context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    private fun keyFor(kind: String, sessionKey: String) = "$kind|$sessionKey"
}
