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
#include "daffy/web/voice_diagnostics_http_server.hpp"

namespace {

std::string HttpRequest(const int port, const std::string& method, const std::string& path) {
  for (int attempt = 0; attempt < 20; ++attempt) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));
    inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);

    if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
      const std::string request =
          method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
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

  assert(false && "failed to connect to voice diagnostics HTTP server");
  return {};
}

struct FakeNativeVoiceFacade {
  daffy::voice::NativeVoiceClientStateSnapshot state{};
  daffy::voice::NativeVoiceClientTelemetry telemetry{};

  daffy::util::json::Value Snapshot() const {
    return daffy::web::BuildNativeVoiceDiagnosticsPayload(state, telemetry);
  }
};

}  // namespace

int main() {
  auto config = daffy::config::DefaultAppConfig();
  config.server.bind_address = "127.0.0.1";
  config.server.port = 0;
  config.frontend_bridge.enabled = true;
  config.frontend_bridge.bridge_endpoint = "/bridge/events";
  config.frontend_bridge.voice_transport = "native-client-only";

  FakeNativeVoiceFacade fake_voice;
  fake_voice.state.running = true;
  fake_voice.state.negotiation_started = true;
  fake_voice.state.negotiation_resets = 2;
  fake_voice.state.negotiation_reoffers = 1;
  fake_voice.state.last_negotiation_reset_reason = "peer-left";
  fake_voice.state.last_reoffer_reason = "peer-ready";
  fake_voice.state.last_error = "none";
  fake_voice.state.signaling.last_turn_fetch_error = "turn-refresh-failed";
  fake_voice.state.live.peer.last_transport_reset_reason = "peer-left";

  fake_voice.telemetry.negotiation_resets = 2;
  fake_voice.telemetry.negotiation_reoffers = 1;
  fake_voice.telemetry.last_negotiation_reset_reason = "peer-left";
  fake_voice.telemetry.last_reoffer_reason = "peer-ready";
  fake_voice.telemetry.signaling.turn_fetch_retry_attempts = 3;
  fake_voice.telemetry.signaling.turn_refreshes = 1;
  fake_voice.telemetry.live.peer.transport_resets = 4;
  fake_voice.telemetry.live.peer.last_transport_reset_reason = "peer-left";

  std::ostringstream logs;
  auto logger = daffy::core::CreateOstreamLogger("voice-diagnostics-http", daffy::core::LogLevel::kInfo, logs);
  daffy::web::VoiceDiagnosticsHttpServer server(
      config, logger, [&fake_voice]() { return fake_voice.Snapshot(); });

  const auto started = server.Start();
  assert(started.ok());
  assert(started.value() > 0);

  const auto root = HttpRequest(started.value(), "GET", "/");
  assert(root.find("HTTP/1.1 200 OK") != std::string::npos);
  assert(root.find("Access-Control-Allow-Origin: *") != std::string::npos);
  assert(root.find("\"service\":\"daffy-backend-voice-diagnostics\"") != std::string::npos);
  assert(root.find("\"voice_bridge\":\"/bridge/events\"") != std::string::npos);

  const auto health = HttpRequest(started.value(), "GET", "/healthz");
  assert(health.find("HTTP/1.1 200 OK") != std::string::npos);
  assert(health.find("Access-Control-Allow-Origin: *") != std::string::npos);
  assert(health.find("\"status\":\"ok\"") != std::string::npos);
  assert(health.find("\"bridge_enabled\":true") != std::string::npos);

  const auto bridge = HttpRequest(started.value(), "GET", "/bridge/events");
  assert(bridge.find("HTTP/1.1 200 OK") != std::string::npos);
  assert(bridge.find("Access-Control-Allow-Origin: *") != std::string::npos);
  assert(bridge.find("\"negotiation_resets\":2") != std::string::npos);
  assert(bridge.find("\"last_negotiation_reset_reason\":\"peer-left\"") != std::string::npos);
  assert(bridge.find("\"turn_fetch_retry_attempts\":3") != std::string::npos);
  assert(bridge.find("\"transport_resets\":4") != std::string::npos);
  assert(bridge.find("\"voice_transport\":\"native-client-only\"") != std::string::npos);

  const auto bad_method = HttpRequest(started.value(), "POST", "/bridge/events");
  assert(bad_method.find("HTTP/1.1 405 Method Not Allowed") != std::string::npos);
  assert(bad_method.find("Access-Control-Allow-Origin: *") != std::string::npos);

  const auto missing = HttpRequest(started.value(), "GET", "/missing");
  assert(missing.find("HTTP/1.1 404 Not Found") != std::string::npos);
  assert(missing.find("Access-Control-Allow-Origin: *") != std::string::npos);

  server.Stop();
  return 0;
}
