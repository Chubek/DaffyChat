#pragma once

#include <cstddef>
#include <cstdint>

struct socket;

struct sockaddr_conn {
  std::uint16_t sconn_family;
  std::uint16_t sconn_port;
  void* sconn_addr;
};

union sctp_notification;
