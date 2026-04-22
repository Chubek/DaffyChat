#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

#include "daffy/ipc/nng_transport.hpp"
#include "daffy/services/room_ops_service.hpp"

namespace {

volatile std::sig_atomic_t g_running = 1;

void HandleSignal(const int) { g_running = 0; }

void PrintUsage() {
  std::cout << "Usage: daffy-roomops-service --url <nng-url>\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string service_url;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--url" && index + 1 < argc) {
      service_url = argv[++index];
    } else if (argument == "--help" || argument == "-h") {
      PrintUsage();
      return 0;
    } else {
      std::cerr << "Unknown argument: " << argument << '\n';
      PrintUsage();
      return 1;
    }
  }

  if (service_url.empty()) {
    std::cerr << "Missing required `--url` argument\n";
    PrintUsage();
    return 1;
  }

  std::signal(SIGTERM, HandleSignal);
  std::signal(SIGINT, HandleSignal);

  daffy::ipc::NngRequestReplyTransport transport;
  daffy::services::RoomOpsService service;
  auto bind_status = service.Bind(transport, service_url);
  if (!bind_status.ok()) {
    std::cerr << "failed to bind roomops service: " << bind_status.error().message << '\n';
    return 1;
  }

  while (g_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return 0;
}
