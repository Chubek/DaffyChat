#pragma once

#include "daffy/config/app_config.hpp"
#include "daffy/core/error.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/signaling/server.hpp"

namespace daffy::signaling {

class UWebSocketsSignalingServer {
 public:
  UWebSocketsSignalingServer(config::AppConfig config, SignalingServer& signaling_server, core::Logger logger);

  core::Status Run();

 private:
  config::AppConfig config_;
  SignalingServer& signaling_server_;
  core::Logger logger_;
};

}  // namespace daffy::signaling
