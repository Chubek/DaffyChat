#pragma once

#include <atomic>
#include <string>
#include <thread>

#include "daffy/config/app_config.hpp"
#include "daffy/core/error.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/signaling/server.hpp"

namespace daffy::signaling {

class SignalingAdminHttpServer {
 public:
  SignalingAdminHttpServer(config::AppConfig config, SignalingServer& signaling_server, core::Logger logger);
  ~SignalingAdminHttpServer();

  core::Result<int> Start();
  void Stop();

  bool is_running() const;
  int bound_port() const;

 private:
  void AcceptLoop();

  config::AppConfig config_;
  SignalingServer& signaling_server_;
  core::Logger logger_;
  std::atomic<bool> running_{false};
  int listen_fd_{-1};
  int bound_port_{0};
  std::thread worker_;
};

}  // namespace daffy::signaling
