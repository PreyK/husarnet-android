// Copyright (c) 2025 Husarnet sp. z o.o.
// Authors: listed in project_root/README.md
// License: specified in project_root/LICENSE.txt
#pragma once
#include <string>

#include "husarnet/ipaddress.h"
#include "husarnet/layer_interfaces.h"
#include "husarnet/ngsocket.h"
#include "husarnet/string_view.h"

// TUN backed by a file descriptor handed over by Android's VpnService.
// Address, MTU and routes are configured on the Java side (VpnService.Builder).
class Tun : public UpperLayer {
 private:
  int fd;
  std::string tunBuffer;

  // This is called by the OsSocket as a callback
  void onTunData();

 public:
  explicit Tun(int fd);

  void onLowerLayerData(HusarnetAddress source, string_view data) override;
};
