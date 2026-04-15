#include "daffy/rooms/events.hpp"

namespace daffy::rooms {

std::string ToString(const RoomEventKind kind) {
  switch (kind) {
    case RoomEventKind::kRoomCreated:
      return "room-created";
    case RoomEventKind::kParticipantJoined:
      return "participant-joined";
    case RoomEventKind::kSessionAttached:
      return "session-attached";
    case RoomEventKind::kRoomStateChanged:
      return "room-state-changed";
  }
  return "room-created";
}

util::json::Value RoomEventToJson(const RoomEvent& event) {
  return util::json::Value::Object{{"kind", ToString(event.kind)},
                                   {"room_id", event.room_id},
                                   {"occurred_at", event.occurred_at},
                                   {"message", event.message},
                                   {"participant_id", event.participant_id},
                                   {"session_id", event.session_id},
                                   {"room_state", ToString(event.room_state)}};
}

runtime::EventEnvelope ToEventEnvelope(const RoomEvent& event) {
  return runtime::EventEnvelope{"room.lifecycle", ToString(event.kind), event.occurred_at, RoomEventToJson(event)};
}

}  // namespace daffy::rooms
