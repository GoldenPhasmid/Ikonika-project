package com.goldenphasmid.ikonika

import android.app.Activity
import android.content.Intent
import android.os.Bundle

class ReaderIntakeActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val forward = Intent(intent).apply {
            setClass(this@ReaderIntakeActivity, IkonikaActivity::class.java)
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }

        startActivity(forward)
        finish()
    }
}
