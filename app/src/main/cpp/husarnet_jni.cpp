// JNI glue between the Kotlin VpnService and the Husarnet core (Android port).
#include <jni.h>

#include <atomic>
#include <string>
#include <thread>

#include <android/log.h>
#include <stdlib.h>
#include <unistd.h>

#include "husarnet/ports/port.h"

#include "husarnet/dashboardapi/response.h"
#include "husarnet/husarnet_manager.h"
#include "husarnet/peer.h"

#include "magic_enum/magic_enum.hpp"
#include "nlohmann/json.hpp"

static JavaVM* javaVm = nullptr;
static jobject vpnService = nullptr;  // global ref
static jmethodID establishTunMethod = nullptr;
static HusarnetManager* manager = nullptr;
static std::atomic<bool> started{false};

// Core logs (quill) go to stdout/stderr, which Android discards. Pipe them to logcat.
static void startLogPump()
{
  int fds[2];
  if(pipe(fds) != 0)
    return;

  setvbuf(stdout, nullptr, _IOLBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  dup2(fds[1], STDOUT_FILENO);
  dup2(fds[1], STDERR_FILENO);

  std::thread([readFd = fds[0]]() {
    char buf[1024];
    std::string line;
    ssize_t n;
    while((n = read(readFd, buf, sizeof(buf))) > 0) {
      for(ssize_t i = 0; i < n; i++) {
        if(buf[i] == '\n') {
          __android_log_write(ANDROID_LOG_INFO, "husarnet", line.c_str());
          line.clear();
        } else {
          line += buf[i];
        }
      }
    }
  }).detach();
}

static std::string toStdString(JNIEnv* env, jstring str)
{
  if(str == nullptr)
    return {};
  const char* chars = env->GetStringUTFChars(str, nullptr);
  std::string result(chars);
  env->ReleaseStringUTFChars(str, chars);
  return result;
}

// Runs on the Husarnet thread: ask the VpnService for a TUN fd
static int establishTun(const std::string& ip)
{
  JNIEnv* env = nullptr;
  bool attached = false;

  if(javaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    if(javaVm->AttachCurrentThread(&env, nullptr) != JNI_OK)
      return -1;
    attached = true;
  }

  jstring jip = env->NewStringUTF(ip.c_str());
  jint fd = env->CallIntMethod(vpnService, establishTunMethod, jip);
  if(env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    fd = -1;
  }
  env->DeleteLocalRef(jip);

  if(attached)
    javaVm->DetachCurrentThread();

  return fd;
}

static bool isReady()
{
  return manager != nullptr && AndroidPort::waitReady(0);
}

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
  javaVm = vm;
  startLogPump();
  return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL Java_dev_husarnet_android_Native_start(
    JNIEnv* env,
    jobject thiz,
    jobject service,
    jstring filesDir,
    jstring hostname)
{
  if(started.exchange(true))
    return;

  vpnService = env->NewGlobalRef(service);
  jclass serviceClass = env->GetObjectClass(service);
  establishTunMethod = env->GetMethodID(serviceClass, "establishTun", "(Ljava/lang/String;)I");
  env->DeleteLocalRef(serviceClass);

  AndroidPort::setFilesDir(toStdString(env, filesDir));
  AndroidPort::setHostname(toStdString(env, hostname));
  AndroidPort::setTunProvider(establishTun);

  setenv("HUSARNET_ENABLE_HOOKS", "false", 1);

  manager = new HusarnetManager();

  // runHusarnet() never returns; stopping = killing the process
  std::thread([]() {
    manager->prepareHusarnet();
    manager->runHusarnet();
  }).detach();
}

// Returns null on success, error message otherwise. Blocking - call off the UI thread.
JNIEXPORT jstring JNICALL
Java_dev_husarnet_android_Native_join(JNIEnv* env, jobject thiz, jstring joinCode, jstring hostname)
{
  if(manager == nullptr)
    return env->NewStringUTF("not running - connect first");

  if(!AndroidPort::waitReady(60000))
    return env->NewStringUTF("timed out waiting for Husarnet to start");

  std::string code = toStdString(env, joinCode);
  std::string host = toStdString(env, hostname);
  if(code.empty())
    return env->NewStringUTF("join code is empty");
  if(host.empty())
    host = Port::getSelfHostname();

  auto apiAddress = manager->configManager->getApiAddress();
  if(apiAddress.isInvalid())
    return env->NewStringUTF("API address not set (no connection to Husarnet?)");

  auto response = dashboardapi::postClaim(
      apiAddress, manager->myIdentity, etl::string_view(code.data(), code.size()),
      etl::string_view(host.data(), host.size()));

  if(!response.isSuccessful())
    return env->NewStringUTF(("join failed: " + response.toString()).c_str());

  manager->configManager->triggerGetConfig();
  return nullptr;
}

JNIEXPORT jstring JNICALL Java_dev_husarnet_android_Native_status(JNIEnv* env, jobject thiz)
{
  nlohmann::json j;
  j["ready"] = isReady();

  if(isReady()) {
    j["ip"] = manager->myIdentity->getIpAddress().toString();
    j["claimed"] = manager->configManager->isClaimed();
    j["base"] = std::string(magic_enum::enum_name(manager->ngsocket->getCurrentBaseConnectionType()));
    j["base_address"] = manager->ngsocket->getCurrentBaseAddress().str();
    j["hosts"] = AndroidPort::getHosts();

    j["peers"] = nlohmann::json::array();
    for(auto& [address, peer] : manager->peerContainer->getPeers()) {
      j["peers"].push_back({
          {"address", peer->getIpAddress().toString()},
          {"active", peer->isActive()},
          {"tunnelled", peer->isTunelled()},
          {"secure", peer->isSecure()},
      });
    }
  }

  return env->NewStringUTF(j.dump().c_str());
}

}  // extern "C"
