#pragma once

#include <string>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/ipc/nng_transport.hpp"
#include "daffy/rooms/room_registry.hpp"
#include "daffy/runtime/event_bus.hpp"
#include "daffy/services/service_metadata.hpp"

namespace daffy::services {

class RoomStateService {
 public:
  explicit RoomStateService(
      core::Logger logger = core::CreateConsoleLogger("room-state-service", core::LogLevel::kInfo));

  static ServiceMetadata Metadata();

  core::Result<ipc::MessageEnvelope> Handle(const ipc::MessageEnvelope& request);
  core::Status Bind(ipc::NngRequestReplyTransport& transport, std::string url);

 private:
  core::Result<rooms::ParticipantRole> ParseRole(const util::json::Value& value) const;
  core::Result<rooms::RoomState> ParseState(const util::json::Value& value) const;
  util::json::Value CapturedEventsToJson() const;

  runtime::InMemoryEventBus event_bus_;
  rooms::RoomRegistry room_registry_;
  std::vector<runtime::EventEnvelope> captured_events_;
  runtime::EventBus::SubscriptionId subscription_id_{0};
};

}  // namespace daffy::services
