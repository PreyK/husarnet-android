# Husarnet for Android (unofficial port)

Minimal Android client for Husarnet: `VpnService` + the upstream C++ core via JNI.

## Build

Patched Husarnet core (upstream d7c3ab0 + `husarnet-android.patch`) is vendored in
`third_party/husarnet`. Open the folder in Android Studio (NDK 27.2 + CMake 3.22.1 from
SDK Manager) and Run, or `gradlew.bat assembleDebug`.

Windows: keep the project at a short path (e.g. `C:\dev\hn`) - FetchContent sources under
`.cxx` can exceed MAX_PATH. Git must be on PATH (libsodium is fetched via git).

First native build fetches libsodium, c-ares, quill, etl, nlohmann_json, magic_enum,
better-enums, cpp-httplib via CMake FetchContent (needs internet). arm64-v8a only.

## Use

1. Connect → accept the VPN prompt.
2. Paste the join code from the Husarnet Dashboard, Join. (Identity persists in app storage;
   later launches only need Connect.)
3. Peers' hostnames appear under Hosts; tap to open `http://[ip]/` in the browser.
   No /etc/hosts on Android, so hostnames don't resolve - use the IPs.

Disconnect kills the app process (the core has no shutdown path).
Logs: `adb logcat -s husarnet`.

## What the patch does

- `ports/port.h`: detect `__ANDROID__` before `__linux__` (NDK defines both).
- `ports/android/`: new port on top of `ports/fat`. TUN fd comes from
  `VpnService.Builder.establish()` (address/route/MTU set in Kotlin); storage in
  `filesDir`; DNS via `getaddrinfo` (c-ares can't find servers on Android 8+);
  hosts table kept in memory; hooks disabled.
- `ports/fat/port.cpp`: skip `gethostid()` on Android (not in bionic).
- `husarnet.cmake`: Android source/dep wiring; httplib without the daemon HTTP API.

## Status

Builds with NDK 27.2 / SDK CMake 3.22.1 and runs on an arm64 phone (tested on a
Galaxy S24, Android 16): connects to the Husarnet base server, joins a network with a
Dashboard join code, and reaches peers (an ESP32 web server, opened in the phone's
browser from the hosts list).

Only one VPN can be active on Android. If another app is set as Always-on VPN, the
consent prompt is skipped and Connect fails; turn that off in Settings > VPN.

## License

GPL-3.0 (see `LICENSE`). This repository contains a modified copy of the Husarnet core
(© Husarnet Sp. z o.o., upstream commit d7c3ab0), changed in October 2026 to add an
Android port; the changes are listed above and in `husarnet-android.patch`. Under the
Husarnet Open Source Dual License (`third_party/husarnet/LICENSE.txt`), a VPN client
built from that code is covered by GPL-3.0.

This is an unofficial port, not affiliated with or endorsed by Husarnet.
