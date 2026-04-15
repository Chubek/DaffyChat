#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <sstream>
#include <string>
#include <thread>

#include "daffy/config/app_config.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/signaling/admin_http_server.hpp"
#include "daffy/signaling/server.hpp"

namespace {

std::string HttpGet(const int port, const std::string& path) {
  for (int attempt = 0; attempt < 20; ++attempt) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
      const std::string request =
          "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
      const auto written = send(fd, request.data(), request.size(), 0);
      assert(written == static_cast<ssize_t>(request.size()));

      std::string response;
      char buffer[1024];
      while (true) {
        const auto read = recv(fd, buffer, sizeof(buffer), 0);
        if (read <= 0) {
          break;
        }
        response.append(buffer, buffer + read);
      }
      close(fd);
      return response;
    }

    close(fd);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  assert(false && "failed to connect to admin HTTP server");
  return {};
}

}  // namespace

int main() {
  auto config = daffy::config::DefaultAppConfig();
  config.signaling.bind_address = "127.0.0.1";
  config.signaling.port = 0;

  std::ostringstream logs;
  auto logger = daffy::core::CreateOstreamLogger("signaling-admin-http", daffy::core::LogLevel::kInfo, logs);
  daffy::signaling::SignalingServer signaling_server(config, logger);
  daffy::signaling::SignalingAdminHttpServer admin_server(config, signaling_server, logger);

  signaling_server.OpenConnection({"conn-a", "127.0.0.1", "tier2-test", false});
  const auto join = signaling_server.HandleMessage("conn-a", R"({"type":"join","room":"room-admin","peer_id":"peer-a"})");
  assert(join.accepted);

  const auto start_result = admin_server.Start();
  assert(start_result.ok());
  assert(start_result.value() > 0);

  const auto health = HttpGet(start_result.value(), "/healthz");
  assert(health.find("HTTP/1.1 200 OK") != std::string::npos);
  assert(health.find("Access-Control-Allow-Origin: *") != std::string::npos);
  assert(health.find("\"transport_status\":\"uwebsockets-ready\"") != std::string::npos);
  assert(health.find("\"turn_configured\":true") != std::string::npos);
  assert(health.find("\"turn_credential_mode\":\"static\"") != std::string::npos);
  assert(health.find("\"reconnect_grace_ms\":45000") != std::string::npos);

  const auto rooms = HttpGet(start_result.value(), "/debug/rooms");
  assert(rooms.find("HTTP/1.1 200 OK") != std::string::npos);
  assert(rooms.find("Access-Control-Allow-Origin: *") != std::string::npos);
  assert(rooms.find("\"room\":\"room-admin\"") != std::string::npos);
  assert(rooms.find("\"peer_id\":\"peer-a\"") != std::string::npos);

  const auto turn = HttpGet(start_result.value(), "/debug/turn-credentials?room=room-admin&peer_id=peer-a");
  assert(turn.find("HTTP/1.1 200 OK") != std::string::npos);
  assert(turn.find("\"username\":\"daffy\"") != std::string::npos);

  const auto bad_turn = HttpGet(start_result.value(), "/debug/turn-credentials?room=room-admin");
  assert(bad_turn.find("HTTP/1.1 400 Bad Request") != std::string::npos);
  assert(bad_turn.find("\"error\":\"missing-query\"") != std::string::npos);

  const auto missing = HttpGet(start_result.value(), "/does-not-exist");
  assert(missing.find("HTTP/1.1 404 Not Found") != std::string::npos);

  admin_server.Stop();
  return 0;
}
