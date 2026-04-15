#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "daffy/config/app_config.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/signaling/native_signaling_client.hpp"
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

bool WaitUntil(const std::function<bool()>& predicate, const int attempts = 200, const int sleep_ms = 20) {
  for (int attempt = 0; attempt < attempts; ++attempt) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  }
  return false;
}

}  // namespace

int main() {
  auto config = daffy::config::DefaultAppConfig();
  config.signaling.bind_address = "127.0.0.1";
  config.signaling.port = 7813;
  config.turn.uri.clear();
  config.turn.username.clear();
  config.turn.password.clear();

  const pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    std::ostringstream logs;
    auto logger = daffy::core::CreateOstreamLogger("signaling-native-client", daffy::core::LogLevel::kInfo, logs);
    daffy::signaling::SignalingServer signaling_server(config, logger);
    daffy::signaling::UWebSocketsSignalingServer transport(config, signaling_server, logger);
    const auto run_result = transport.Run();
    _exit(run_result.ok() ? 0 : 1);
  }

  const auto health = HttpGet(config.signaling.port, "/healthz");
  assert(health.find("HTTP/1.1 200 OK") != std::string::npos);

  daffy::signaling::NativeSignalingClientConfig peer_a_config;
  peer_a_config.websocket_url = "ws://127.0.0.1:7813/";
  peer_a_config.room = "native-room";
  peer_a_config.peer_id = "peer-a";
  peer_a_config.turn_fetch_retry_delay_ms = 1;

  daffy::signaling::NativeSignalingClientConfig peer_b_config = peer_a_config;
  peer_b_config.peer_id = "peer-b";

  std::atomic<int> peer_a_turn_fetch_attempts{0};
  std::atomic<int> peer_b_turn_fetch_attempts{0};
  const auto make_fetcher = [](std::atomic<int>& attempts, const bool fail_refresh) {
    return [&attempts, fail_refresh](const std::string&) -> daffy::core::Result<std::string> {
      const int attempt = ++attempts;
      if (attempt < 3) {
        return daffy::core::Error{daffy::core::ErrorCode::kUnavailable, "synthetic turn fetch failure"};
      }
      if (fail_refresh && attempt > 3) {
        return daffy::core::Error{daffy::core::ErrorCode::kUnavailable, "synthetic refresh failure"};
      }
      return std::string(
          R"({"uri":"turn:retry.example.org:3478?transport=udp","username":"retry-user","password":"retry-pass"})");
    };
  };
  peer_a_config.turn_credentials_fetcher = make_fetcher(peer_a_turn_fetch_attempts, true);
  peer_b_config.turn_credentials_fetcher = make_fetcher(peer_b_turn_fetch_attempts, false);

  auto peer_a = daffy::signaling::NativeSignalingClient::Create(peer_a_config);
  auto peer_b = daffy::signaling::NativeSignalingClient::Create(peer_b_config);
  assert(peer_a.ok());
  assert(peer_b.ok());

  std::atomic<bool> peer_a_ready{false};
  std::atomic<bool> peer_b_ready{false};
  std::atomic<bool> peer_b_received_offer{false};
  std::string offer_sdp;

  peer_a.value().SetMessageCallback([&peer_a_ready](const daffy::signaling::Message& message) {
    if (message.type == daffy::signaling::MessageType::kPeerReady && message.peer_id == "peer-b") {
      peer_a_ready = true;
    }
  });
  peer_b.value().SetMessageCallback([&peer_b_ready, &peer_b_received_offer, &offer_sdp](const daffy::signaling::Message& message) {
    if (message.type == daffy::signaling::MessageType::kPeerReady && message.peer_id == "peer-a") {
      peer_b_ready = true;
    }
    if (message.type == daffy::signaling::MessageType::kOffer) {
      peer_b_received_offer = true;
      offer_sdp = message.sdp;
    }
  });

  assert(peer_a.value().Start().ok());
  assert(peer_b.value().Start().ok());

  assert(WaitUntil([&peer_a, &peer_b]() {
    return peer_a.value().state().joined && peer_b.value().state().joined;
  }));
  assert(WaitUntil([&peer_a, &peer_b]() {
    return peer_a.value().state().has_turn_credentials && peer_b.value().state().has_turn_credentials;
  }));
  assert(WaitUntil([&peer_a_ready, &peer_b_ready]() { return peer_a_ready.load() && peer_b_ready.load(); }));

  daffy::signaling::Message offer;
  offer.type = daffy::signaling::MessageType::kOffer;
  offer.room = "native-room";
  offer.peer_id = "peer-a";
  offer.target_peer_id = "peer-b";
  offer.sdp = "native-offer";
  assert(peer_a.value().Send(offer).ok());

  assert(WaitUntil([&peer_b_received_offer]() { return peer_b_received_offer.load(); }));
  assert(offer_sdp == "native-offer");
  const auto bootstrap = peer_a.value().bootstrap();
  assert(bootstrap.turn_uri == "turn:retry.example.org:3478?transport=udp");
  assert(bootstrap.turn_username == "retry-user");
  assert(peer_a.value().state().has_turn_credentials);
  assert(peer_a.value().state().discovered_ice_servers >= 2);
  assert(peer_a.value().telemetry().outbound_messages >= 2);
  assert(peer_b.value().telemetry().inbound_messages >= 2);
  assert(peer_a.value().telemetry().turn_fetch_retry_attempts >= 2);
  assert(peer_a.value().telemetry().turn_fetch_successes >= 1);
  assert(peer_a_turn_fetch_attempts.load() >= 3);

  assert(peer_a.value().Stop().ok());
  assert(peer_a.value().Start().ok());
  assert(WaitUntil([&peer_a]() { return peer_a.value().state().joined; }));
  assert(peer_a.value().state().has_turn_credentials);
  assert(peer_a.value().state().last_turn_fetch_error.find("synthetic refresh failure") != std::string::npos);
  assert(peer_a.value().telemetry().turn_refreshes >= 1);
  assert(peer_a.value().telemetry().turn_fetch_failures >= 1);
  assert(peer_a.value().bootstrap().turn_uri == "turn:retry.example.org:3478?transport=udp");

  assert(peer_a.value().Stop().ok());
  assert(peer_b.value().Stop().ok());

  kill(child, SIGTERM);
  int status = 0;
  waitpid(child, &status, 0);
  assert(WIFSIGNALED(status) || WIFEXITED(status));
  return 0;
}
