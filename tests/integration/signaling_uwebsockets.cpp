#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "daffy/config/app_config.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/signaling/server.hpp"
#include "daffy/signaling/uwebsockets_server.hpp"

namespace {

int ConnectTcp(const int port) {
  for (int attempt = 0; attempt < 60; ++attempt) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
    if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
      return fd;
    }
    close(fd);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  assert(false && "failed to connect to signaling server");
  return -1;
}

std::string ReadUntil(const int fd, const std::string& delimiter) {
  std::string buffer;
  char chunk[1024];
  while (buffer.find(delimiter) == std::string::npos) {
    const auto read_bytes = recv(fd, chunk, sizeof(chunk), 0);
    assert(read_bytes > 0);
    buffer.append(chunk, chunk + read_bytes);
  }
  return buffer;
}

std::string HttpGet(const int port, const std::string& path) {
  const int fd = ConnectTcp(port);
  const std::string request =
      "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
  const auto written = send(fd, request.data(), request.size(), 0);
  assert(written == static_cast<ssize_t>(request.size()));

  std::string response;
  char buffer[1024];
  while (true) {
    const auto read_bytes = recv(fd, buffer, sizeof(buffer), 0);
    if (read_bytes <= 0) {
      break;
    }
    response.append(buffer, buffer + read_bytes);
  }
  close(fd);
  return response;
}

int OpenWebSocket(const int port) {
  const int fd = ConnectTcp(port);
  const std::string request =
      "GET / HTTP/1.1\r\n"
      "Host: 127.0.0.1\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Version: 13\r\n"
      "User-Agent: daffy-native-test\r\n\r\n";
  const auto written = send(fd, request.data(), request.size(), 0);
  assert(written == static_cast<ssize_t>(request.size()));
  const auto response = ReadUntil(fd, "\r\n\r\n");
  assert(response.find("101 Switching Protocols") != std::string::npos);
  return fd;
}

void SendTextFrame(const int fd, const std::string& payload) {
  std::vector<unsigned char> frame;
  frame.push_back(0x81);
  if (payload.size() < 126) {
    frame.push_back(static_cast<unsigned char>(0x80 | payload.size()));
  } else {
    frame.push_back(0x80 | 126);
    frame.push_back(static_cast<unsigned char>((payload.size() >> 8) & 0xff));
    frame.push_back(static_cast<unsigned char>(payload.size() & 0xff));
  }

  const unsigned char mask[4] = {0x12, 0x34, 0x56, 0x78};
  frame.insert(frame.end(), mask, mask + 4);
  for (std::size_t index = 0; index < payload.size(); ++index) {
    frame.push_back(static_cast<unsigned char>(payload[index] ^ mask[index % 4]));
  }

  const auto written = send(fd, frame.data(), frame.size(), 0);
  assert(written == static_cast<ssize_t>(frame.size()));
}

std::string ReadTextFrame(const int fd) {
  unsigned char header[2];
  assert(recv(fd, header, sizeof(header), MSG_WAITALL) == 2);
  assert((header[0] & 0x0f) == 0x1);

  std::uint64_t payload_length = header[1] & 0x7f;
  if (payload_length == 126) {
    unsigned char extended[2];
    assert(recv(fd, extended, sizeof(extended), MSG_WAITALL) == 2);
    payload_length = (static_cast<std::uint64_t>(extended[0]) << 8) | extended[1];
  } else if (payload_length == 127) {
    unsigned char extended[8];
    assert(recv(fd, extended, sizeof(extended), MSG_WAITALL) == 8);
    payload_length = 0;
    for (unsigned char byte : extended) {
      payload_length = (payload_length << 8) | byte;
    }
  }

  std::string payload(payload_length, '\0');
  assert(recv(fd, payload.data(), payload.size(), MSG_WAITALL) == static_cast<ssize_t>(payload.size()));
  return payload;
}

}  // namespace

int main() {
  auto config = daffy::config::DefaultAppConfig();
  config.signaling.bind_address = "127.0.0.1";
  config.signaling.port = 7811;

  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    std::ostringstream logs;
    auto logger = daffy::core::CreateOstreamLogger("signaling-uwebsockets", daffy::core::LogLevel::kInfo, logs);
    daffy::signaling::SignalingServer signaling_server(config, logger);
    daffy::signaling::UWebSocketsSignalingServer transport(config, signaling_server, logger);
    const auto run_result = transport.Run();
    _exit(run_result.ok() ? 0 : 1);
  }

  const auto health = HttpGet(config.signaling.port, "/healthz");
  assert(health.find("HTTP/1.1 200 OK") != std::string::npos);
  assert(health.find("\"transport_status\":\"uwebsockets-ready\"") != std::string::npos);

  const int peer_a = OpenWebSocket(config.signaling.port);
  const int peer_b = OpenWebSocket(config.signaling.port);

  SendTextFrame(peer_a, R"({"type":"join","room":"uv-room","peer_id":"peer-a"})");
  const auto join_ack_a = ReadTextFrame(peer_a);
  assert(join_ack_a.find("\"type\":\"join-ack\"") != std::string::npos);
  assert(join_ack_a.find("\"peer_id\":\"peer-a\"") != std::string::npos);

  SendTextFrame(peer_b, R"({"type":"join","room":"uv-room","peer_id":"peer-b"})");
  const auto join_ack_b = ReadTextFrame(peer_b);
  const auto ready_for_b = ReadTextFrame(peer_b);
  const auto ready_for_a = ReadTextFrame(peer_a);
  assert(join_ack_b.find("\"type\":\"join-ack\"") != std::string::npos);
  assert(ready_for_b.find("\"type\":\"peer-ready\"") != std::string::npos);
  assert(ready_for_a.find("\"type\":\"peer-ready\"") != std::string::npos);

  SendTextFrame(peer_a, R"({"type":"offer","room":"uv-room","target_peer_id":"peer-b","sdp":"offer-sdp"})");
  const auto relayed_offer = ReadTextFrame(peer_b);
  assert(relayed_offer.find("\"type\":\"offer\"") != std::string::npos);
  assert(relayed_offer.find("\"peer_id\":\"peer-a\"") != std::string::npos);
  assert(relayed_offer.find("\"sdp\":\"offer-sdp\"") != std::string::npos);

  close(peer_a);
  close(peer_b);
  kill(child, SIGTERM);
  int status = 0;
  waitpid(child, &status, 0);
  assert(WIFSIGNALED(status) || WIFEXITED(status));
  return 0;
}
