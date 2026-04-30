#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "daffy/config/app_config.hpp"
#include "daffy/core/error.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/util/json.hpp"
#include "daffy/voice/native_voice_client.hpp"

namespace daffy::web {

class VoiceDiagnosticsHttpServer {
 public:
  using SnapshotProvider = std::function<util::json::Value()>;

  VoiceDiagnosticsHttpServer(config::AppConfig config, core::Logger logger, SnapshotProvider provider);
  ~VoiceDiagnosticsHttpServer();

  core::Result<int> Start();
  void Stop();

 private:
  struct Impl;

  std::unique_ptr<Impl> impl_;
};

util::json::Value BuildNativeVoiceDiagnosticsPayload(
    const voice::NativeVoiceClientStateSnapshot& state,
    const voice::NativeVoiceClientTelemetry& telemetry,
    std::string_view voice_transport = "browser-socketio");

}  // namespace daffy::web
