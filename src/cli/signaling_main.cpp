#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string_view>
#include <thread>

#include "daffy/config/app_config.hpp"
#include "daffy/core/build_info.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/signaling/admin_http_server.hpp"
#include "daffy/signaling/server.hpp"
#include "daffy/signaling/uwebsockets_server.hpp"

#ifndef DAFFY_SOURCE_DIR
#define DAFFY_SOURCE_DIR "."
#endif

namespace {

std::atomic<bool> g_stop_requested{false};

void HandleSignal(int) { g_stop_requested.store(true); }

int RunSignaling(const std::string& config_path, const std::string_view mode) {
  auto config_result = daffy::config::LoadAppConfigFromFile(config_path);
  if (!config_result.ok()) {
    std::cerr << "failed to load config: " << config_result.error().ToString() << '\n';
    return 1;
  }

  auto logger = daffy::core::CreateConsoleLogger("daffy-signaling",
                                                 daffy::core::ParseLogLevel(config_result.value().server.log_level));
  daffy::signaling::SignalingServer server(config_result.value(), logger);

  std::cout << daffy::core::BuildSummary() << '\n';
  std::cout << "signaling config: " << daffy::config::DescribeAppConfig(config_result.value()) << '\n';

  if (mode == "--health") {
    std::cout << daffy::util::json::Serialize(server.HealthToJson()) << '\n';
    return 0;
  }
  if (mode == "--debug") {
    std::cout << daffy::util::json::Serialize(server.DebugStateToJson()) << '\n';
    return 0;
  }
  if (mode == "--serve") {
    if (!server.HasUWebSocketsRuntimeDependencies()) {
      daffy::signaling::SignalingAdminHttpServer admin_server(config_result.value(), server, logger);
      const auto start_result = admin_server.Start();
      if (!start_result.ok()) {
        std::cerr << "failed to start signaling admin HTTP server: " << start_result.error().ToString() << '\n';
        return 1;
      }

      std::cerr << "uWebSockets WebSocket transport is unavailable in this workspace: missing vendored uSockets sources under "
                << std::string(DAFFY_SOURCE_DIR) + "/third_party/uWebSockets/uSockets" << '\n';

      std::signal(SIGINT, HandleSignal);
      std::signal(SIGTERM, HandleSignal);
      std::cout << "serving admin endpoints on http://" << config_result.value().signaling.bind_address << ':'
                << start_result.value() << '\n';
      std::cout << "health: http://" << config_result.value().signaling.bind_address << ':' << start_result.value()
                << config_result.value().signaling.health_endpoint << '\n';
      std::cout << "rooms: http://" << config_result.value().signaling.bind_address << ':' << start_result.value()
                << config_result.value().signaling.debug_rooms_endpoint << '\n';
      std::cout << "turn: http://" << config_result.value().signaling.bind_address << ':' << start_result.value()
                << config_result.value().signaling.turn_credentials_endpoint << "?room=<room>&peer_id=<peer>" << '\n';

      while (!g_stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
      }
      admin_server.Stop();
      return 0;
    }

    std::cout << "transport: uwebsockets" << '\n';
    std::cout << "websocket_url: ws://" << config_result.value().signaling.bind_address << ':'
              << config_result.value().signaling.port << '/' << '\n';
    std::cout << "health_url: http://" << config_result.value().signaling.bind_address << ':'
              << config_result.value().signaling.port << config_result.value().signaling.health_endpoint << '\n';
    std::cout << "rooms_url: http://" << config_result.value().signaling.bind_address << ':'
              << config_result.value().signaling.port << config_result.value().signaling.debug_rooms_endpoint << '\n';
    std::cout << "turn_url: http://" << config_result.value().signaling.bind_address << ':'
              << config_result.value().signaling.port << config_result.value().signaling.turn_credentials_endpoint
              << "?room=<room>&peer_id=<peer>" << '\n';
    daffy::signaling::UWebSocketsSignalingServer transport(config_result.value(), server, logger);
    const auto run_result = transport.Run();
    if (!run_result.ok()) {
      std::cerr << "uWebSockets signaling server failed: " << run_result.error().ToString() << '\n';
      return 1;
    }
    return 0;
  }

  std::cout << "health: " << daffy::util::json::Serialize(server.HealthToJson()) << '\n';
  std::cout << "status: Tier 2 signaling coordinator ready" << '\n';
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && std::string_view(argv[1]) == "--version") {
    std::cout << daffy::core::ProjectVersion() << '\n';
    return 0;
  }

  std::string_view mode;
  std::string config_path = daffy::config::DefaultConfigPath();
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "--health" || argument == "--debug" || argument == "--serve") {
      mode = argument;
      continue;
    }
    config_path = argv[index];
  }

  return RunSignaling(config_path, mode);
}
