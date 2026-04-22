#pragma once

#include <chrono>
#include <string>

#include "daffy/core/error.hpp"
#include "daffy/ipc/nng_transport.hpp"
#include "daffy/services/service_metadata.hpp"
#include "daffy/util/json.hpp"

namespace daffy::services {

class HealthService {
 public:
  HealthService();

  static ServiceMetadata Metadata();

  core::Result<ipc::MessageEnvelope> Handle(const ipc::MessageEnvelope& request) const;
  core::Status Bind(ipc::NngRequestReplyTransport& transport, std::string url) const;

 private:
  util::json::Value BuildStatusPayload() const;
  util::json::Value BuildPingPayload() const;

  std::chrono::steady_clock::time_point started_at_;
};

}  // namespace daffy::services
