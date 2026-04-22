#include "daffy/services/event_bridge_service.hpp"

namespace daffy::services {

namespace {

constexpr char kServiceTopic[] = "service.eventbridge";
constexpr char kRequestType[] = "request";
constexpr char kReplyType[] = "reply";

}  // namespace

EventBridgeService::EventBridgeService(core::Logger logger)
    : room_registry_(std::move(logger), event_bus_) {
  lifecycle_subscription_id_ = event_bus_.Subscribe("room.lifecycle", [this](const runtime::EventEnvelope& event) {
    events_.push_back(SequencedEvent{next_sequence_++, event});
  });
}

ServiceMetadata EventBridgeService::Metadata() {
  return ServiceMetadata{"eventbridge",
                         "1.0.0",
                         "Built-in room event bus bridge service with replay and webhook dispatch.",
                         "daffy-eventbridge-service",
                         {"nng:reqrep", "eventsource-bridge", "webhook"},
                         true};
}

core::Result<rooms::ParticipantRole> EventBridgeService::ParseRole(const util::json::Value& value) const {
  if (!value.IsString()) {
    return core::Error{core::ErrorCode::kParseError, "Participant role must be a string"};
  }
  const auto& role = value.AsString();
  if (role == "member") {
    return rooms::ParticipantRole::kMember;
  }
  if (role == "admin") {
    return rooms::ParticipantRole::kAdmin;
  }
  if (role == "bot") {
    return rooms::ParticipantRole::kBot;
  }
  return core::Error{core::ErrorCode::kInvalidArgument, "Unsupported participant role: " + role};
}

core::Result<rooms::RoomState> EventBridgeService::ParseState(const util::json::Value& value) const {
  if (!value.IsString()) {
    return core::Error{core::ErrorCode::kParseError, "Room state must be a string"};
  }
  const auto& state = value.AsString();
  if (state == "provisioning") {
    return rooms::RoomState::kProvisioning;
  }
  if (state == "active") {
    return rooms::RoomState::kActive;
  }
  if (state == "closing") {
    return rooms::RoomState::kClosing;
  }
  if (state == "closed") {
    return rooms::RoomState::kClosed;
  }
  return core::Error{core::ErrorCode::kInvalidArgument, "Unsupported room state: " + state};
}

util::json::Value EventBridgeService::EventsToJson(const std::size_t after_sequence, const std::string& topic) const {
  util::json::Value::Array items;
  for (const auto& entry : events_) {
    if (entry.sequence <= after_sequence) {
      continue;
    }
    if (!topic.empty() && entry.event.topic != topic) {
      continue;
    }
    items.emplace_back(util::json::Value::Object{{"sequence", static_cast<double>(entry.sequence)},
                                                 {"event", runtime::EventEnvelopeToJson(entry.event)}});
  }
  return items;
}

util::json::Value EventBridgeService::WebhookSubscriptionsToJson() const {
  util::json::Value::Array items;
  for (const auto& subscription : subscriptions_) {
    items.emplace_back(util::json::Value::Object{{"id", subscription.id},
                                                 {"topic", subscription.topic},
                                                 {"url", subscription.url},
                                                 {"enabled", subscription.enabled}});
  }
  return items;
}

core::Result<ipc::MessageEnvelope> EventBridgeService::Handle(const ipc::MessageEnvelope& request) {
  if (request.topic != kServiceTopic || request.type != kRequestType) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Unexpected event bridge service message"};
  }

  const auto* rpc = request.payload.Find("rpc");
  if (rpc == nullptr || !rpc->IsString()) {
    return core::Error{core::ErrorCode::kParseError, "Event bridge request must include a string `rpc` field"};
  }

  const auto& method = rpc->AsString();
  if (method == "CreateRoom") {
    const auto* display_name = request.payload.Find("display_name");
    if (display_name == nullptr || !display_name->IsString()) {
      return core::Error{core::ErrorCode::kParseError, "CreateRoom requires a string `display_name` field"};
    }
    auto room = room_registry_.CreateRoom(display_name->AsString());
    if (!room.ok()) {
      return room.error();
    }
    return ipc::MessageEnvelope{kServiceTopic, kReplyType, util::json::Value::Object{{"room", rooms::RoomToJson(room.value())}}};
  }

  if (method == "AddParticipant") {
    const auto* room_id = request.payload.Find("room_id");
    const auto* display_name = request.payload.Find("display_name");
    if (room_id == nullptr || !room_id->IsString() || display_name == nullptr || !display_name->IsString()) {
      return core::Error{core::ErrorCode::kParseError,
                         "AddParticipant requires string `room_id` and `display_name` fields"};
    }
    rooms::ParticipantRole role = rooms::ParticipantRole::kMember;
    if (const auto* role_value = request.payload.Find("role"); role_value != nullptr) {
      auto parsed_role = ParseRole(*role_value);
      if (!parsed_role.ok()) {
        return parsed_role.error();
      }
      role = parsed_role.value();
    }
    auto participant = room_registry_.AddParticipant(room_id->AsString(), display_name->AsString(), role);
    if (!participant.ok()) {
      return participant.error();
    }
    return ipc::MessageEnvelope{kServiceTopic,
                                kReplyType,
                                util::json::Value::Object{{"participant", rooms::ParticipantToJson(participant.value())}}};
  }

  if (method == "AttachSession") {
    const auto* room_id = request.payload.Find("room_id");
    const auto* participant_id = request.payload.Find("participant_id");
    const auto* peer_id = request.payload.Find("peer_id");
    if (room_id == nullptr || !room_id->IsString() || participant_id == nullptr || !participant_id->IsString() ||
        peer_id == nullptr || !peer_id->IsString()) {
      return core::Error{core::ErrorCode::kParseError,
                         "AttachSession requires string `room_id`, `participant_id`, and `peer_id` fields"};
    }
    auto session = room_registry_.AttachSession(room_id->AsString(), participant_id->AsString(), peer_id->AsString());
    if (!session.ok()) {
      return session.error();
    }
    return ipc::MessageEnvelope{kServiceTopic,
                                kReplyType,
                                util::json::Value::Object{{"session", rooms::PeerSessionToJson(session.value())}}};
  }

  if (method == "SetRoomState") {
    const auto* room_id = request.payload.Find("room_id");
    const auto* state = request.payload.Find("state");
    if (room_id == nullptr || !room_id->IsString() || state == nullptr) {
      return core::Error{core::ErrorCode::kParseError, "SetRoomState requires `room_id` and `state` fields"};
    }
    auto parsed_state = ParseState(*state);
    if (!parsed_state.ok()) {
      return parsed_state.error();
    }
    auto room = room_registry_.TransitionRoomState(room_id->AsString(), parsed_state.value());
    if (!room.ok()) {
      return room.error();
    }
    return ipc::MessageEnvelope{kServiceTopic, kReplyType, util::json::Value::Object{{"room", rooms::RoomToJson(room.value())}}};
  }

  if (method == "PollEvents") {
    std::size_t after_sequence = 0;
    if (const auto* value = request.payload.Find("after_sequence"); value != nullptr && value->IsNumber()) {
      after_sequence = static_cast<std::size_t>(value->AsNumber());
    }
    std::string topic;
    if (const auto* value = request.payload.Find("topic"); value != nullptr && value->IsString()) {
      topic = value->AsString();
    }
    return ipc::MessageEnvelope{kServiceTopic,
                                kReplyType,
                                util::json::Value::Object{{"events", EventsToJson(after_sequence, topic)},
                                                          {"next_sequence", static_cast<double>(next_sequence_)}}};
  }

  if (method == "RegisterWebhook") {
    const auto* topic = request.payload.Find("topic");
    const auto* url = request.payload.Find("url");
    if (topic == nullptr || !topic->IsString() || url == nullptr || !url->IsString()) {
      return core::Error{core::ErrorCode::kParseError, "RegisterWebhook requires string `topic` and `url` fields"};
    }
    web::WebhookSubscription subscription{"webhook-" + std::to_string(next_webhook_id_++), topic->AsString(), url->AsString(), true};
    subscriptions_.push_back(subscription);
    return ipc::MessageEnvelope{kServiceTopic,
                                kReplyType,
                                util::json::Value::Object{{"subscription",
                                                           util::json::Value::Object{{"id", subscription.id},
                                                                                     {"topic", subscription.topic},
                                                                                     {"url", subscription.url},
                                                                                     {"enabled", subscription.enabled}}}}};
  }

  if (method == "ListWebhooks") {
    return ipc::MessageEnvelope{kServiceTopic,
                                kReplyType,
                                util::json::Value::Object{{"subscriptions", WebhookSubscriptionsToJson()},
                                                          {"count", static_cast<double>(subscriptions_.size())}}};
  }

  if (method == "DispatchWebhooks") {
    util::json::Value::Array deliveries;
    for (const auto& subscription : subscriptions_) {
      for (const auto& entry : events_) {
        if (entry.event.topic != subscription.topic) {
          continue;
        }
        auto delivery = webhook_dispatcher_.Dispatch(subscription, entry.event);
        if (delivery.ok()) {
          deliveries.emplace_back(web::WebhookDeliveryToJson(delivery.value()));
        }
      }
    }
    return ipc::MessageEnvelope{kServiceTopic,
                                kReplyType,
                                util::json::Value::Object{{"deliveries", deliveries},
                                                          {"count", static_cast<double>(deliveries.size())}}};
  }

  if (method == "Status") {
    return ipc::MessageEnvelope{kServiceTopic,
                                kReplyType,
                                util::json::Value::Object{{"service_name", Metadata().name},
                                                          {"rooms", static_cast<double>(room_registry_.List().size())},
                                                          {"events", static_cast<double>(events_.size())},
                                                          {"subscriptions", static_cast<double>(subscriptions_.size())}}};
  }

  return core::Error{core::ErrorCode::kInvalidArgument, "Unsupported event bridge RPC: " + method};
}

core::Status EventBridgeService::Bind(ipc::NngRequestReplyTransport& transport, std::string url) {
  return transport.Bind(std::move(url), [this](const ipc::MessageEnvelope& request) { return Handle(request); });
}

}  // namespace daffy::services
