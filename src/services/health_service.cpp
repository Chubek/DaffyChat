#include "daffy/services/health_service.hpp"

#include "daffy/core/build_info.hpp"

namespace daffy::services {

namespace {

constexpr char kServiceTopic[] = "service.health";
constexpr char kRequestType[] = "request";
constexpr char kReplyType[] = "reply";

}  // namespace

HealthService::HealthService() : started_at_(std::chrono::steady_clock::now()) {}

ServiceMetadata HealthService::Metadata() {
  return ServiceMetadata{"health",
                         "1.0.0",
                         "Built-in health and status service for runtime diagnostics.",
                         "daffy-health-service",
                         {"nng:reqrep", "jsonrpc-like"},
                         true};
}

util::json::Value HealthService::BuildStatusPayload() const {
  const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started_at_);
  util::json::Value::Array protocols;
  for (const auto& protocol : Metadata().protocols) {
    protocols.emplace_back(protocol);
  }

  return util::json::Value::Object{{"service_name", Metadata().name},
                                   {"status", "ok"},
                                   {"version", std::string(DAFFY_VERSION)},
                                   {"build", core::BuildSummary()},
                                   {"uptime_seconds", static_cast<double>(uptime.count())},
                                   {"protocols", protocols}};
}

util::json::Value HealthService::BuildPingPayload() const {
  const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started_at_);
  return util::json::Value::Object{{"service_name", Metadata().name},
                                   {"reply", "pong"},
                                   {"uptime_seconds", static_cast<double>(uptime.count())}};
}

core::Result<ipc::MessageEnvelope> HealthService::Handle(const ipc::MessageEnvelope& request) const {
  if (request.topic != kServiceTopic || request.type != kRequestType) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Unexpected health service message"};
  }

  const auto* rpc = request.payload.Find("rpc");
  const std::string method = rpc != nullptr && rpc->IsString() ? rpc->AsString() : "Status";
  if (method == "Status") {
    return ipc::MessageEnvelope{kServiceTopic, kReplyType, BuildStatusPayload()};
  }
  if (method == "Ping") {
    return ipc::MessageEnvelope{kServiceTopic, kReplyType, BuildPingPayload()};
  }

  return core::Error{core::ErrorCode::kInvalidArgument, "Unsupported health RPC: " + method};
}

core::Status HealthService::Bind(ipc::NngRequestReplyTransport& transport, std::string url) const {
  return transport.Bind(std::move(url), [this](const ipc::MessageEnvelope& request) { return Handle(request); });
}

}  // namespace daffy::services
