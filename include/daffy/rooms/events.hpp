#pragma once

#include <string>

#include "daffy/rooms/models.hpp"
#include "daffy/runtime/event_bus.hpp"

namespace daffy::rooms {

enum class RoomEventKind {
  kRoomCreated,
  kParticipantJoined,
  kSessionAttached,
  kRoomStateChanged
};

struct RoomEvent {
  RoomEventKind kind{RoomEventKind::kRoomCreated};
  RoomId room_id;
  std::string occurred_at;
  std::string message;
  ParticipantId participant_id;
  SessionId session_id;
  RoomState room_state{RoomState::kProvisioning};
};

std::string ToString(RoomEventKind kind);
util::json::Value RoomEventToJson(const RoomEvent& event);
runtime::EventEnvelope ToEventEnvelope(const RoomEvent& event);

}  // namespace daffy::rooms
