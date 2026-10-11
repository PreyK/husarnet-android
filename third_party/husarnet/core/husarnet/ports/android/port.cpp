// Copyright (c) 2025 Husarnet sp. z o.o.
// Authors: listed in project_root/README.md
// License: specified in project_root/LICENSE.txt
//
// Android port. Builds on top of the "fat" port (ports/fat) and overrides its
// weak symbols where Linux assumptions (/etc, /var/lib, netlink, c-ares
// resolv.conf discovery, hooks) don't hold on Android.
#include "husarnet/ports/port.h"

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include <android/log.h>
#include <ares.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>

#include "husarnet/ports/fat/filesystem.h"
#include "husarnet/ports/sockets.h"

#include "husarnet/ipaddress.h"
#include "husarnet/logging.h"
#include "husarnet/util.h"

static std::mutex stateMutex;
static std::condition_variable readyCv;
static bool ready = false;
static std::function<int(const std::string&)> tunProvider;
static std::string filesDir = ".";
static std::string selfHostname = "android";
static std::map<std::string, std::string> hosts;

static IpAddress ipAddressFromSockaddr(const struct sockaddr* addr)
{
  if(addr == nullptr)
    return IpAddress();

  if(addr->sa_family == AF_INET) {
    uint32_t addr32 = ((const struct sockaddr_in*)addr)->sin_addr.s_addr;
    return IpAddress::fromBinary4(addr32);
  }

  if(addr->sa_family == AF_INET6) {
    const struct in6_addr& a6 = ((const struct sockaddr_in6*)addr)->sin6_addr;
    return IpAddress::fromBinary((const char*)a6.s6_addr);
  }

  return IpAddress();
}

static std::string storagePath(StorageKey key)
{
  switch(key) {
    case StorageKey::id:
      return filesDir + "/id";
    case StorageKey::config:
      return filesDir + "/config.json";
    case StorageKey::cache:
      return filesDir + "/cache.json";
    case StorageKey::daemonApiToken:
      return filesDir + "/daemon_api_token";
    case StorageKey::defaults:
      return filesDir + "/defaults.ini";
  }
  return filesDir + "/unknown";
}

namespace AndroidPort {
  void setTunProvider(std::function<int(const std::string& ip)> provider)
  {
    std::lock_guard lg(stateMutex);
    tunProvider = std::move(provider);
  }

  void setFilesDir(const std::string& dir)
  {
    std::lock_guard lg(stateMutex);
    filesDir = dir;
  }

  void setHostname(const std::string& hostname)
  {
    std::lock_guard lg(stateMutex);
    selfHostname = hostname;
  }

  bool waitReady(int timeoutMs)
  {
    std::unique_lock ul(stateMutex);
    return readyCv.wait_for(ul, std::chrono::milliseconds(timeoutMs), [] { return ready; });
  }

  std::map<std::string, std::string> getHosts()
  {
    std::lock_guard lg(stateMutex);
    return hosts;
  }
}  // namespace AndroidPort

namespace Port {
  // Not fatInit(): it creates /var/lib/husarnet/, which can't exist here (storage
  // lives in filesDir)
  void init()
  {
    signal(SIGPIPE, SIG_IGN);
    ares_library_init(ARES_LIB_INIT_NONE);
  }

  void notifyReady()
  {
    {
      std::lock_guard lg(stateMutex);
      ready = true;
    }
    readyCv.notify_all();
  }

  void log(const LogLevel level, const std::string& message)
  {
    __android_log_write(ANDROID_LOG_INFO, "husarnet", message.c_str());
  }

  etl::map<EnvKey, std::string, ENV_KEY_OPTIONS> getEnvironmentDefaultsFromIniFile()
  {
    return {};
  }

  IpAddress getIpAddressFromInterfaceName(const std::string& interfaceName)
  {
    // Only used by the daemon HTTP API, which is not built on Android
    return IpAddress();
  }

  std::vector<IpAddress> getLocalAddresses()
  {
    std::vector<IpAddress> ret;

    struct ifaddrs* ifAddrs = nullptr;
    if(getifaddrs(&ifAddrs) != 0) {
      HLOG_ERROR("getifaddrs failed // {error}", strerror(errno));
      return ret;
    }

    for(struct ifaddrs* ifa = ifAddrs; ifa != nullptr; ifa = ifa->ifa_next) {
      if(ifa->ifa_addr == nullptr || ifa->ifa_name == nullptr)
        continue;

      std::string name = ifa->ifa_name;
      if(name.rfind("tun", 0) == 0)
        continue;  // our own (or another) VPN interface - would loop

      IpAddress address = ipAddressFromSockaddr(ifa->ifa_addr);

      if(address.isInvalid() || address.isLoopback() || address.isLinkLocal() || address.isFC94())
        continue;

      ret.push_back(address);
    }

    freeifaddrs(ifAddrs);

    return ret;
  }

  UpperLayer* startTun(const HusarnetAddress& myAddress, const std::string& interfaceName)
  {
    (void)interfaceName;

    std::function<int(const std::string&)> provider;
    {
      std::lock_guard lg(stateMutex);
      provider = tunProvider;
    }

    int fd = provider ? provider(myAddress.toString()) : -1;
    if(fd < 0) {
      HLOG_CRITICAL("unable to obtain TUN fd from VpnService");
      abort();
    }

    HLOG_INFO("TUN ready // {fd} {address}", fd, myAddress.toString());
    return new Tun(fd);
  }

  std::string getSelfHostname()
  {
    std::lock_guard lg(stateMutex);
    return selfHostname;
  }

  bool setSelfHostname(const std::string& newHostname)
  {
    std::lock_guard lg(stateMutex);
    selfHostname = newHostname;
    return true;
  }

  void updateHostsFile(const std::map<std::string, HusarnetAddress>& data)
  {
    std::lock_guard lg(stateMutex);
    hosts.clear();
    for(const auto& [hostname, address] : data) {
      hosts[hostname] = address.toString();
    }
  }

  // c-ares can't discover DNS servers on Android 8+ without JNI init, so use
  // bionic's resolver instead
  IpAddress resolveToIp(const std::string& hostname)
  {
    if(hostname.empty()) {
      HLOG_ERROR("Empty hostname provided for a DNS search");
      return IpAddress();
    }

    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    int err = getaddrinfo(hostname.c_str(), "443", &hints, &result);
    if(err != 0 || result == nullptr) {
      HLOG_ERROR("DNS resolution failed // {hostname} {error}", hostname, gai_strerror(err));
      return IpAddress();
    }

    IpAddress address;
    for(auto* node = result; node != nullptr; node = node->ai_next) {
      address = ipAddressFromSockaddr(node->ai_addr);
      if(address.isValid())
        break;
    }

    freeaddrinfo(result);

    HLOG_DEBUG("DNS resolution done // {hostname} {address}", hostname, address.toString());
    return address;
  }

  bool runHook(HookType hookType)
  {
    return false;
  }

  std::string readStorage(StorageKey key)
  {
    std::string path;
    {
      std::lock_guard lg(stateMutex);
      path = storagePath(key);
    }
    return readFileSilent(path);
  }

  bool writeStorage(StorageKey key, const std::string& data)
  {
    std::string path;
    {
      std::lock_guard lg(stateMutex);
      path = storagePath(key);
    }
    return writeFile(path, data);
  }
}  // namespace Port
