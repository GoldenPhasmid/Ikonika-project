package com.goldenphasmid.ikonika

import android.accounts.Account
import android.app.Activity
import android.content.Intent
import android.content.IntentSender
import android.util.Log
import com.google.android.gms.auth.api.identity.AuthorizationRequest
import com.google.android.gms.auth.api.identity.AuthorizationResult
import com.google.android.gms.auth.api.identity.ClearTokenRequest
import com.google.android.gms.auth.api.identity.Identity
import com.google.android.gms.auth.api.identity.RevokeAccessRequest
import com.google.android.gms.common.api.Scope

private const val LOG_TAG = "GoogleAuthorization"
private const val GOOGLE_ACCOUNT_TYPE = "com.google"

object GoogleAuthorization {
    const val AUTHORIZATION_REQUEST_CODE = 8100

    @JvmStatic
    external fun nativeOnAuthorized(accessToken: String)

    @JvmStatic
    external fun nativeOnAuthorizationFailed(message: String, refused: Boolean)

    @JvmStatic
    fun authorize(activity: Activity, scopes: String, silent: Boolean, accountEmail: String) {
        val builder = AuthorizationRequest.builder()
            .setRequestedScopes(scopes.split(",").map { Scope(it) })

        if (accountEmail.isNotEmpty()) {
            builder.setAccount(Account(accountEmail, GOOGLE_ACCOUNT_TYPE))
        } else if (!silent) {
            builder.setPrompt(AuthorizationRequest.Prompt.SELECT_ACCOUNT)
        }

        Identity.getAuthorizationClient(activity)
            .authorize(builder.build())
            .addOnSuccessListener { result -> onAuthorizationResult(activity, result, silent) }
            .addOnFailureListener { e ->
                Log.w(LOG_TAG, "authorize failed: ${e.message ?: e.javaClass.simpleName}")
                nativeOnAuthorizationFailed(e.message ?: e.javaClass.simpleName, false)
            }
    }

    private fun onAuthorizationResult(activity: Activity, result: AuthorizationResult, silent: Boolean) {
        val pendingIntent = result.pendingIntent

        if (result.hasResolution() && pendingIntent != null) {
            if (silent) {
                nativeOnAuthorizationFailed("access was not previously granted", true)
                return
            }

            try {
                activity.startIntentSenderForResult(
                    pendingIntent.intentSender, AUTHORIZATION_REQUEST_CODE, null, 0, 0, 0
                )
            } catch (e: IntentSender.SendIntentException) {
                nativeOnAuthorizationFailed(e.message ?: e.javaClass.simpleName, false)
            }

            return
        }

        finishAuthorization(result)
    }

    @JvmStatic
    fun handleAuthorizationResult(activity: Activity, data: Intent?) {
        try {
            val result = Identity.getAuthorizationClient(activity)
                .getAuthorizationResultFromIntent(data)

            finishAuthorization(result)
        } catch (e: Exception) {
            nativeOnAuthorizationFailed(e.message ?: e.javaClass.simpleName, false)
        }
    }

    @JvmStatic
    fun clearToken(activity: Activity, token: String) {
        val request = ClearTokenRequest.builder()
            .setToken(token)
            .build()

        Identity.getAuthorizationClient(activity)
            .clearToken(request)
            .addOnFailureListener { e ->
                Log.w(LOG_TAG, "clearToken failed: ${e.message ?: e.javaClass.simpleName}")
            }
    }

    @JvmStatic
    fun revoke(activity: Activity, accountEmail: String, scopes: String) {
        if (accountEmail.isEmpty()) {
            Log.w(LOG_TAG, "revoke: no account email, skipping")
            return
        }

        val request = RevokeAccessRequest.builder()
            .setScopes(scopes.split(",").map { Scope(it) })
            .setAccount(Account(accountEmail, GOOGLE_ACCOUNT_TYPE))
            .build()

        Identity.getAuthorizationClient(activity)
            .revokeAccess(request)
            .addOnSuccessListener {
                Log.i(LOG_TAG, "revoke succeeded")
            }
            .addOnFailureListener { e ->
                Log.w(LOG_TAG, "revoke failed: ${e.message ?: e.javaClass.simpleName}")
            }
    }

    private fun finishAuthorization(result: AuthorizationResult) {
        val token = result.accessToken

        if (token == null) {
            nativeOnAuthorizationFailed("no access token granted", false)
            return
        }

        nativeOnAuthorized(token)
    }
}
