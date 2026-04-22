#include "daffy/services/room_state_service.hpp"

namespace daffy::services {

namespace {

constexpr char kServiceTopic[] = "service.roomstate";
constexpr char kRequestType[] = "request";
constexpr char kReplyType[] = "reply";

}  // namespace

RoomStateService::RoomStateService(core::Logger logger)
    : room_registry_(std::move(logger), event_bus_) {
  subscription_id_ = event_bus_.Subscribe("room.lifecycle", [this](const runtime::EventEnvelope& event) {
    captured_events_.push_back(event);
  });
}

ServiceMetadata RoomStateService::Metadata() {
  return ServiceMetadata{"roomstate",
                         "1.0.0",
                         "Built-in room metadata and lifecycle state service.",
                         "daffy-roomstate-service",
                         {"nng:reqrep", "room-lifecycle"},
                         true};
}

core::Result<rooms::ParticipantRole> RoomStateService::ParseRole(const util::json::Value& value) const {
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

core::Result<rooms::RoomState> RoomStateService::ParseState(const util::json::Value& value) const {
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

util::json::Value RoomStateService::CapturedEventsToJson() const {
  util::json::Value::Array events;
  events.reserve(captured_events_.size());
  for (const auto& event : captured_events_) {
    events.emplace_back(runtime::EventEnvelopeToJson(event));
  }
  return events;
}

core::Result<ipc::MessageEnvelope> RoomStateService::Handle(const ipc::MessageEnvelope& request) {
  if (request.topic != kServiceTopic || request.type != kRequestType) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Unexpected room state service message"};
  }

  const auto* rpc = request.payload.Find("rpc");
  if (rpc == nullptr || !rpc->IsString()) {
    return core::Error{core::ErrorCode::kParseError, "Room state request must include a string `rpc` field"};
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

  if (method == "ListRooms") {
    util::json::Value::Array rooms_json;
    for (const auto& room : room_registry_.List()) {
      rooms_json.emplace_back(rooms::RoomToJson(room));
    }
    return ipc::MessageEnvelope{kServiceTopic,
                                kReplyType,
                                util::json::Value::Object{{"rooms", rooms_json},
                                                          {"count", static_cast<double>(rooms_json.size())}}};
  }

  if (method == "GetRoom") {
    const auto* room_id = request.payload.Find("room_id");
    if (room_id == nullptr || !room_id->IsString()) {
      return core::Error{core::ErrorCode::kParseError, "GetRoom requires a string `room_id` field"};
    }
    auto room = room_registry_.Find(room_id->AsString());
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
    return ipc::MessageEnvelope{kServiceTopic,
                                kReplyType,
                                util::json::Value::Object{{"events", CapturedEventsToJson()},
                                                          {"count", static_cast<double>(captured_events_.size())}}};
  }

  return core::Error{core::ErrorCode::kInvalidArgument, "Unsupported room state RPC: " + method};
}

core::Status RoomStateService::Bind(ipc::NngRequestReplyTransport& transport, std::string url) {
  return transport.Bind(std::move(url), [this](const ipc::MessageEnvelope& request) { return Handle(request); });
}

}  // namespace daffy::services
