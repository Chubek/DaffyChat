#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <thread>

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
  assert(false && "failed to connect");
  return -1;
}

std::string HttpRequest(const int port, const std::string& method, const std::string& path, const std::string& body = "") {
  const int fd = ConnectTcp(port);
  std::ostringstream request;
  request << method << " " << path << " HTTP/1.1\r\n";
  request << "Host: 127.0.0.1\r\n";
  request << "Connection: close\r\n";
  if (!body.empty()) {
    request << "Content-Type: application/json\r\n";
    request << "Content-Length: " << body.size() << "\r\n";
  }
  request << "\r\n";
  request << body;

  const auto wire = request.str();
  const auto written = send(fd, wire.data(), wire.size(), 0);
  assert(written == static_cast<ssize_t>(wire.size()));

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

std::string ExtractJsonField(const std::string& json_text, const std::string& field) {
  const std::string marker = "\"" + field + "\":\"";
  const auto start = json_text.find(marker);
  assert(start != std::string::npos);
  const auto value_begin = start + marker.size();
  const auto value_end = json_text.find('"', value_begin);
  assert(value_end != std::string::npos);
  return json_text.substr(value_begin, value_end - value_begin);
}

}  // namespace

int main() {
  auto config = daffy::config::DefaultAppConfig();
  config.signaling.bind_address = "127.0.0.1";
  config.signaling.port = 7812;

  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    std::ostringstream logs;
    auto logger = daffy::core::CreateOstreamLogger("signaling-http-api", daffy::core::LogLevel::kInfo, logs);
    daffy::signaling::SignalingServer signaling_server(config, logger);
    daffy::signaling::UWebSocketsSignalingServer transport(config, signaling_server, logger);
    const auto run_result = transport.Run();
    _exit(run_result.ok() ? 0 : 1);
  }

  const auto connect_response =
      HttpRequest(config.signaling.port, "POST", "/api/signaling/connect", R"({"peer_id":"peer-http-a"})");
  assert(connect_response.find("HTTP/1.1 200 OK") != std::string::npos);
  const std::string connection_id = ExtractJsonField(connect_response, "connection_id");
  assert(!connection_id.empty());

  const std::string join_message =
      "{\\\"type\\\":\\\"join\\\",\\\"room\\\":\\\"api-room\\\",\\\"peer_id\\\":\\\"peer-http-a\\\"}";
  const auto send_response = HttpRequest(config.signaling.port,
                                         "POST",
                                         "/api/signaling/send",
                                         std::string("{\"connection_id\":\"") + connection_id + "\",\"message\":\"" +
                                             join_message + "\"}");
  assert(send_response.find("HTTP/1.1 202 Accepted") != std::string::npos);

  const auto events_response =
      HttpRequest(config.signaling.port, "GET", "/api/signaling/events?connection_id=" + connection_id);
  assert(events_response.find("HTTP/1.1 200 OK") != std::string::npos);
  assert(events_response.find("\"join-ack\"") != std::string::npos);
  assert(events_response.find("\"peer-http-a\"") != std::string::npos);

  kill(child, SIGTERM);
  int status = 0;
  waitpid(child, &status, 0);
  assert(WIFSIGNALED(status) || WIFEXITED(status));
  return 0;
}
