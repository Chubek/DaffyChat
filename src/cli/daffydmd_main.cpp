#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "daffy/runtime/daemon_manager.hpp"

namespace {

void PrintUsage() {
  std::cout << "Usage: daffydmd [--run-directory <path>] [--control-url <nng-url>] [--foreground] [--start <service>]\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string run_directory = "/tmp/daffychat";
  std::string control_url = "ipc:///tmp/daffydmd-control.ipc";
  std::string start_service;
  bool foreground = false;

  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--run-directory" && index + 1 < argc) {
      run_directory = argv[++index];
    } else if (argument == "--control-url" && index + 1 < argc) {
      control_url = argv[++index];
    } else if (argument == "--start" && index + 1 < argc) {
      start_service = argv[++index];
    } else if (argument == "--foreground") {
      foreground = true;
    } else if (argument == "--help" || argument == "-h") {
      PrintUsage();
      return 0;
    } else {
      std::cerr << "Unknown argument: " << argument << '\n';
      PrintUsage();
      return 1;
    }
  }

  daffy::runtime::DaemonManager manager(run_directory);
  std::cout << "daffydmd ready\n";
  std::cout << "run directory: " << run_directory << '\n';
  std::cout << "persisted services: " << manager.ListServices().size() << '\n';

  if (!start_service.empty()) {
    auto start_status = manager.StartService(start_service);
    if (!start_status.ok()) {
      std::cerr << "failed to start service `" << start_service << "`: " << start_status.error().message << '\n';
      return 1;
    }
    std::cout << "started service: " << start_service << '\n';
  }

  if (!foreground) {
    std::cout << "foreground mode disabled; control plane not started\n";
    return 0;
  }

  daffy::ipc::InMemoryRequestReplyTransport control_transport;
  auto bind_status = manager.BindControlPlane(control_transport, control_url);
  if (!bind_status.ok()) {
    std::cerr << "failed to bind control plane: " << bind_status.error().message << '\n';
    return 1;
  }

  std::cout << "control plane listening at " << control_url << '\n';
  std::cout << "press Ctrl+C to stop\n";
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
