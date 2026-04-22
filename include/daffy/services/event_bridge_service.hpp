#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/ipc/nng_transport.hpp"
#include "daffy/rooms/room_registry.hpp"
#include "daffy/runtime/event_bus.hpp"
#include "daffy/services/service_metadata.hpp"
#include "daffy/web/webhook.hpp"

namespace daffy::services {

class EventBridgeService {
 public:
  explicit EventBridgeService(
      core::Logger logger = core::CreateConsoleLogger("event-bridge-service", core::LogLevel::kInfo));

  static ServiceMetadata Metadata();

  core::Result<ipc::MessageEnvelope> Handle(const ipc::MessageEnvelope& request);
  core::Status Bind(ipc::NngRequestReplyTransport& transport, std::string url);

 private:
  struct SequencedEvent {
    std::size_t sequence{0};
    runtime::EventEnvelope event;
  };

  core::Result<rooms::ParticipantRole> ParseRole(const util::json::Value& value) const;
  core::Result<rooms::RoomState> ParseState(const util::json::Value& value) const;
  util::json::Value EventsToJson(std::size_t after_sequence, const std::string& topic) const;
  util::json::Value WebhookSubscriptionsToJson() const;

  runtime::InMemoryEventBus event_bus_;
  rooms::RoomRegistry room_registry_;
  web::RecordingWebhookDispatcher webhook_dispatcher_;
  std::vector<SequencedEvent> events_;
  std::vector<web::WebhookSubscription> subscriptions_;
  runtime::EventBus::SubscriptionId lifecycle_subscription_id_{0};
  std::size_t next_sequence_{1};
  std::size_t next_webhook_id_{1};
};

}  // namespace daffy::services
