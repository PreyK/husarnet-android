package dev.husarnet.android

import android.app.Activity
import android.content.Intent
import android.graphics.Typeface
import android.net.Uri
import android.net.VpnService
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import org.json.JSONObject
import kotlin.concurrent.thread

class MainActivity : Activity() {

    private lateinit var joinCode: EditText
    private lateinit var hostname: EditText
    private lateinit var statusView: TextView
    private lateinit var hostsView: LinearLayout
    private val handler = Handler(Looper.getMainLooper())

    private val poll = object : Runnable {
        override fun run() {
            render(runCatching { JSONObject(Native.status()) }.getOrNull())
            handler.postDelayed(this, 2000)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val pad = (16 * resources.displayMetrics.density).toInt()
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(pad, pad, pad, pad)
        }

        joinCode = EditText(this).apply { hint = "Join code (from Dashboard)" }
        hostname = EditText(this).apply {
            hint = "Hostname"
            setText(HusarnetVpnService.defaultHostname())
        }

        root.addView(button("Connect") { connect() })
        root.addView(joinCode)
        root.addView(hostname)
        root.addView(button("Join network") { join() })
        root.addView(button("Disconnect (closes app)") {
            startService(Intent(this, HusarnetVpnService::class.java).setAction(HusarnetVpnService.ACTION_STOP))
        })

        statusView = TextView(this).apply {
            typeface = Typeface.MONOSPACE
            setPadding(0, pad, 0, pad)
            setTextIsSelectable(true)
        }
        root.addView(statusView)

        hostsView = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        root.addView(hostsView)

        setContentView(ScrollView(this).apply {
            fitsSystemWindows = true
            addView(root)
        })
    }

    override fun onResume() {
        super.onResume()
        handler.post(poll)
    }

    override fun onPause() {
        super.onPause()
        handler.removeCallbacks(poll)
    }

    private fun button(label: String, onClick: () -> Unit) =
        Button(this).apply {
            text = label
            setOnClickListener { onClick() }
        }

    private fun connect() {
        val consent = VpnService.prepare(this)
        if (consent != null) {
            startActivityForResult(consent, REQ_VPN)
        } else {
            startVpn()
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode != REQ_VPN) return
        if (resultCode == RESULT_OK) {
            startVpn()
        } else {
            // Also what happens, without any dialog, when another app is the Always-on VPN
            toast("VPN permission denied. If another VPN is set to Always-on, turn that off in Settings > VPN.")
        }
    }

    private fun startVpn() {
        startService(
            Intent(this, HusarnetVpnService::class.java)
                .putExtra(HusarnetVpnService.EXTRA_HOSTNAME, hostname.text.toString())
        )
    }

    private fun join() {
        val code = joinCode.text.toString().trim()
        val host = hostname.text.toString().trim()
        toast("Joining…")
        thread {
            val error = Native.join(code, host)
            runOnUiThread { toast(error ?: "Joined") }
        }
    }

    private fun render(s: JSONObject?) {
        if (s == null || !s.optBoolean("ready")) {
            statusView.text = "Not running"
            hostsView.removeAllViews()
            return
        }

        val peers = s.optJSONArray("peers")
        val peerLines = buildString {
            for (i in 0 until (peers?.length() ?: 0)) {
                val p = peers!!.getJSONObject(i)
                append("  ${p.getString("address")}")
                append(if (p.optBoolean("active")) " active" else " idle")
                if (p.optBoolean("tunnelled")) append(" (relayed)")
                append('\n')
            }
        }

        statusView.text = buildString {
            append("IP:      ${s.optString("ip")}\n")
            append("Joined:  ${s.optBoolean("claimed")}\n")
            append("Base:    ${s.optString("base")} ${s.optString("base_address")}\n")
            append("Peers:\n").append(peerLines.ifEmpty { "  none\n" })
            append("\nHosts (tap to open in browser):")
        }

        hostsView.removeAllViews()
        val hosts = s.optJSONObject("hosts") ?: return
        for (name in hosts.keys()) {
            val ip = hosts.getString(name)
            hostsView.addView(TextView(this).apply {
                text = "$name  $ip"
                typeface = Typeface.MONOSPACE
                setPadding(0, 12, 0, 12)
                setOnClickListener {
                    startActivity(Intent(Intent.ACTION_VIEW, Uri.parse("http://[$ip]/")))
                }
            })
        }
    }

    private fun toast(msg: String) = Toast.makeText(this, msg, Toast.LENGTH_LONG).show()

    companion object {
        private const val REQ_VPN = 1
    }
}
