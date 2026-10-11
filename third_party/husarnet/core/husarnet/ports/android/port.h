// Copyright (c) 2025 Husarnet sp. z o.o.
// Authors: listed in project_root/README.md
// License: specified in project_root/LICENSE.txt
#pragma once
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <functional>
#include <map>
#include <string>

#include "husarnet/ports/dummy_task_priorities.h"

#include "husarnet/ports/android/tun.h"

#define SOCKFUNC(name) ::name
#define SOCKFUNC_close SOCKFUNC(close)

#define ENABLE_IPV6

// Hooks for the embedding app (JNI glue). Must be set before
// HusarnetManager::prepareHusarnet() is called.
namespace AndroidPort {
  // Called from Port::startTun() with this device's Husarnet IPv6 address.
  // Must return a TUN fd (from VpnService.Builder.establish()) or -1.
  void setTunProvider(std::function<int(const std::string& ip)> provider);

  // Directory for identity/config/cache (Context.getFilesDir()).
  void setFilesDir(const std::string& dir);

  void setHostname(const std::string& hostname);

  // Blocks until the stack is up (Port::notifyReady()). False on timeout.
  bool waitReady(int timeoutMs);

  // hostname -> Husarnet IP, as pushed by the Dashboard (replaces /etc/hosts)
  std::map<std::string, std::string> getHosts();
}  // namespace AndroidPort
