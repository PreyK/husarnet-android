// Copyright (c) 2025 Husarnet sp. z o.o.
// Authors: listed in project_root/README.md
// License: specified in project_root/LICENSE.txt
#include "husarnet/ports/android/tun.h"

#include <functional>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "husarnet/ports/port.h"
#include "husarnet/ports/sockets.h"

#include "husarnet/logging.h"
#include "husarnet/util.h"

Tun::Tun(int fd) : fd(fd)
{
  tunBuffer.resize(4096);

  int flags = fcntl(fd, F_GETFL, 0);
  if(flags >= 0) {
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  }

  OsSocket::bindCustomFd(fd, std::bind(&Tun::onTunData, this));
}

void Tun::onTunData()
{
  long size = read(fd, &tunBuffer[0], tunBuffer.size());

  if(size <= 0) {
    return;  // EAGAIN, or the VpnService went away (process gets killed then)
  }

  string_view packet = string_view(tunBuffer).substr(0, size);
  sendToLowerLayer(IpAddress{}, packet);
}

void Tun::onLowerLayerData(HusarnetAddress source, string_view data)
{
  long wr = write(fd, data.data(), data.size());
  if(wr != (long)data.size()) {
    HLOG_INFO("short tun write");
  }
}
