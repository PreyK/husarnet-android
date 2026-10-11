package dev.husarnet.android

object Native {
    init {
        System.loadLibrary("husarnet_jni")
    }

    /** Starts the Husarnet stack once per process. Calls service.establishTun() when ready. */
    external fun start(service: HusarnetVpnService, filesDir: String, hostname: String)

    /** Blocking. Returns null on success, error message otherwise. */
    external fun join(joinCode: String, hostname: String): String?

    /** JSON: ready, ip, claimed, base, base_address, hosts{name: ip}, peers[] */
    external fun status(): String
}
