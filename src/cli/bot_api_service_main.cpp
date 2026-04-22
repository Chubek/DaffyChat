#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "daffy/core/logger.hpp"
#include "daffy/ipc/nng_transport.hpp"
#include "daffy/services/bot_api_service.hpp"

namespace {

volatile std::sig_atomic_t g_shutdown_requested = 0;

void SignalHandler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    g_shutdown_requested = 1;
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string url = "ipc:///tmp/daffy-botapi.ipc";
  daffy::core::LogLevel log_level = daffy::core::LogLevel::kInfo;

  // Parse command-line arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--url" && i + 1 < argc) {
      url = argv[++i];
    } else if (arg == "--log-level" && i + 1 < argc) {
      std::string level = argv[++i];
      if (level == "debug") {
        log_level = daffy::core::LogLevel::kDebug;
      } else if (level == "info") {
        log_level = daffy::core::LogLevel::kInfo;
      } else if (level == "warn") {
        log_level = daffy::core::LogLevel::kWarn;
      } else if (level == "error") {
        log_level = daffy::core::LogLevel::kError;
      }
    } else if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: " << argv[0] << " [OPTIONS]\n"
                << "Options:\n"
                << "  --url <url>          IPC URL to bind (default: ipc:///tmp/daffy-botapi.ipc)\n"
                << "  --log-level <level>  Log level: debug, info, warn, error (default: info)\n"
                << "  --help, -h           Show this help message\n";
      return EXIT_SUCCESS;
    }
  }

  // Install signal handlers
  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  auto logger = daffy::core::CreateConsoleLogger("bot-api-service", log_level);
  logger.Info("Starting DaffyChat Bot API service");
  logger.Info("Binding to: " + url);

  // Create service and transport
  auto service = std::make_unique<daffy::services::BotApiService>(logger);
  auto transport = std::make_unique<daffy::ipc::NngRequestReplyTransport>();

  // Bind service
  auto bind_status = service->Bind(*transport, url);
  if (!bind_status.ok()) {
    logger.Error("Failed to bind service: " + bind_status.error().message);
    return EXIT_FAILURE;
  }

  logger.Info("Bot API service ready");

  // Main loop
  while (!g_shutdown_requested) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  logger.Info("Shutting down Bot API service");
  return EXIT_SUCCESS;
}
