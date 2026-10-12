package dev.husarnet.android

import android.content.Intent
import android.net.VpnService
import android.os.Build
import android.os.Process
import android.system.OsConstants

class HusarnetVpnService : VpnService() {

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopSelf()
            // The core has no shutdown path; ending the process closes the TUN
            Process.killProcess(Process.myPid())
            return START_NOT_STICKY
        }

        val hostname = intent?.getStringExtra(EXTRA_HOSTNAME) ?: defaultHostname()
        Native.start(this, filesDir.absolutePath, hostname)
        return START_NOT_STICKY
    }

    /** Called from the native Husarnet thread with this device's Husarnet IPv6. */
    @Suppress("unused")
    fun establishTun(ip: String): Int {
        val builder = Builder()
            .setSession("Husarnet")
            .setMtu(1350)
            .addAddress(ip, 16)
            .addRoute("fc94::", 16)
            // Without these, Android blocks every family/route the VPN doesn't cover,
            // including the core's own IPv4 link to the base server
            .allowFamily(OsConstants.AF_INET)
            .allowFamily(OsConstants.AF_INET6)

        // Husarnet multicast range; not critical if the platform rejects it
        runCatching { builder.addRoute("ff15:f2d3:a389::", 48) }

        // Android Auto stops working with this VPN active; it never needs Husarnet, so
        // keep it on the normal network. Throws if the app isn't installed.
        for (pkg in BYPASS_PACKAGES) runCatching { builder.addDisallowedApplication(pkg) }

        if (Build.VERSION.SDK_INT >= 29) builder.setMetered(false)

        val pfd = builder.establish() ?: return -1
        return pfd.detachFd()
    }

    companion object {
        const val ACTION_STOP = "dev.husarnet.android.STOP"
        const val EXTRA_HOSTNAME = "hostname"

        private val BYPASS_PACKAGES = listOf("com.google.android.projection.gearhead")

        fun defaultHostname(): String =
            ("android-" + Build.MODEL).lowercase().replace(Regex("[^a-z0-9-]"), "-")
    }
}
