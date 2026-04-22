#include <atomic>
#include <csignal>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "daffy/services/health_service.hpp"

namespace {

std::atomic<bool> g_stop_requested{false};

void HandleSignal(const int) { g_stop_requested.store(true); }

}  // namespace

int main(int argc, char** argv) {
  std::string url = "tcp://127.0.0.1:39100";
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--url" && i + 1 < argc) {
      url = argv[++i];
    }
  }

  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);

  daffy::ipc::NngRequestReplyTransport transport;
  daffy::services::HealthService service;
  const auto status = service.Bind(transport, url);
  if (!status.ok()) {
    std::cerr << status.error().ToString() << '\n';
    return 1;
  }

  while (!g_stop_requested.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return 0;
}
