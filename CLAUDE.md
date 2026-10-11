# Husarnet Android port - working notes for Claude Code

Goal: get this app building and running on an arm64 phone, then confirm it reaches peers
on a Husarnet network (e.g. an ESP32-CAM at http://[fc94:...]:81/stream).

## Layout
- `third_party/husarnet/core` - upstream Husarnet core (commit d7c3ab0) with
  `husarnet-android.patch` already applied. Android port lives in
  `core/husarnet/ports/android/`. Keep `husarnet-android.patch` in sync if you change core:
  it's the diff against upstream.
- `app/src/main/cpp/husarnet_jni.cpp` - JNI glue (start / join / status).
- `app/src/main/java/dev/husarnet/android/` - VpnService + minimal Activity.

## Build / run loop (Windows)
- Needs `local.properties` with `sdk.dir=...` (or ANDROID_HOME), NDK 27.2.12479018, CMake 3.22.1.
- `gradlew.bat assembleDebug`
- `adb install -r app\build\outputs\apk\debug\app-debug.apk`
- `adb logcat -s husarnet AndroidRuntime`
- Keep the repo at a short path; MAX_PATH issues show up as FetchContent/ninja errors.

## Verified so far
Core + android port + JNI compiled and linked with host g++ (`-D__ANDROID__`, real deps);
stack starts, writes identity to filesDir, reaches license download.
`gradlew.bat assembleDebug` succeeds with NDK 27.2 / SDK CMake 3.22.1 (arm64 APK, JNI symbols
exported). Needed `app/src/main/cpp/cmake/FetchContent.cmake` shim: core passes
DOWNLOAD_EXTRACT_TIMESTAMP (CMake 3.24+).
On device (Galaxy S24, Android 16): license, TUN, base server (TCP+UDP), control-plane
websocket and join all work. Needed `allowFamily(AF_INET/AF_INET6)` in establishTun, else
Android blocks the core's own IPv4 link to base. Phone -> ESP32 (esp32-arduino-webserver,
port 80) verified: secure peer link, ping, page loads in the browser via the app's hosts list.
- Another app set as Always-on VPN makes the consent dialog close instantly (now a toast).
- Android Port::init doesn't call fatInit() (it tried to create /var/lib/husarnet/).
- Hosts list was empty right after joining once; it was populated after an app restart.
  Root cause not found.

## Likely first issues
- quill / libsodium / c-ares compile errors under NDK clang.
- Android jni.h: `AttachCurrentThread(JNIEnv**, void*)` (correct as written for NDK).
- `addRoute("ff15:f2d3:a389::", 48)` rejected - already wrapped in runCatching.
- Core blocks in `ConfigManager::waitInit()` until it downloads the license from
  prod.husarnet.com; no network = TUN is never requested (expected, check logcat).

## Constraints
- Minimal diffs; don't restructure upstream core beyond what Android needs.
- No shutdown path in core: Disconnect kills the process. Don't try to add one unless asked.
