#include "daffy/rooms/room_registry.hpp"

#include <algorithm>

#include "daffy/core/id.hpp"
#include "daffy/core/time.hpp"

namespace daffy::rooms {

RoomRegistry::RoomRegistry(core::Logger logger, runtime::EventBus& event_bus)
    : logger_(std::move(logger)), event_bus_(event_bus) {}

core::Result<Room> RoomRegistry::CreateRoom(std::string display_name) {
  Room room;
  room.id = core::GenerateId("room");
  room.display_name = std::move(display_name);
  room.state = RoomState::kActive;
  room.created_at = core::UtcNowIso8601();

  rooms_.emplace(room.id, room);
  logger_.Info("Created room " + room.id + " (" + room.display_name + ")");

  RoomEvent event;
  event.kind = RoomEventKind::kRoomCreated;
  event.room_id = room.id;
  event.occurred_at = room.created_at;
  event.message = "Room created";
  event.room_state = room.state;
  PublishEvent(event);
  return room;
}

core::Result<Participant> RoomRegistry::AddParticipant(const RoomId& room_id,
                                                       std::string display_name,
                                                       const ParticipantRole role) {
  const auto room_it = rooms_.find(room_id);
  if (room_it == rooms_.end()) {
    return core::Error{core::ErrorCode::kNotFound, "Room not found: " + room_id};
  }

  Participant participant;
  participant.id = core::GenerateId("participant");
  participant.display_name = std::move(display_name);
  participant.role = role;
  participant.joined_at = core::UtcNowIso8601();
  room_it->second.participants.push_back(participant);

  logger_.Info("Participant " + participant.id + " joined room " + room_id);

  RoomEvent event;
  event.kind = RoomEventKind::kParticipantJoined;
  event.room_id = room_id;
  event.occurred_at = participant.joined_at;
  event.message = participant.display_name + " joined the room";
  event.participant_id = participant.id;
  event.room_state = room_it->second.state;
  PublishEvent(event);
  return participant;
}

core::Result<PeerSession> RoomRegistry::AttachSession(const RoomId& room_id,
                                                      const ParticipantId& participant_id,
                                                      std::string peer_id) {
  const auto room_it = rooms_.find(room_id);
  if (room_it == rooms_.end()) {
    return core::Error{core::ErrorCode::kNotFound, "Room not found: " + room_id};
  }

  const auto participant_it = std::find_if(room_it->second.participants.begin(), room_it->second.participants.end(),
                                           [&](const Participant& participant) { return participant.id == participant_id; });
  if (participant_it == room_it->second.participants.end()) {
    return core::Error{core::ErrorCode::kNotFound, "Participant not found in room: " + participant_id};
  }

  PeerSession session;
  session.id = core::GenerateId("session");
  session.participant_id = participant_id;
  session.peer_id = std::move(peer_id);
  session.state = SessionState::kPending;
  session.created_at = core::UtcNowIso8601();
  room_it->second.sessions.push_back(session);

  logger_.Info("Attached session " + session.id + " to room " + room_id);

  RoomEvent event;
  event.kind = RoomEventKind::kSessionAttached;
  event.room_id = room_id;
  event.occurred_at = session.created_at;
  event.message = "Session attached to room";
  event.participant_id = participant_id;
  event.session_id = session.id;
  event.room_state = room_it->second.state;
  PublishEvent(event);
  return session;
}

core::Result<Room> RoomRegistry::TransitionRoomState(const RoomId& room_id, const RoomState state) {
  const auto room_it = rooms_.find(room_id);
  if (room_it == rooms_.end()) {
    return core::Error{core::ErrorCode::kNotFound, "Room not found: " + room_id};
  }

  room_it->second.state = state;
  logger_.Info("Room " + room_id + " changed state to " + ToString(state));

  RoomEvent event;
  event.kind = RoomEventKind::kRoomStateChanged;
  event.room_id = room_id;
  event.occurred_at = core::UtcNowIso8601();
  event.message = "Room state changed";
  event.room_state = state;
  PublishEvent(event);
  return room_it->second;
}

core::Result<Room> RoomRegistry::Find(const RoomId& room_id) const {
  const auto room_it = rooms_.find(room_id);
  if (room_it == rooms_.end()) {
    return core::Error{core::ErrorCode::kNotFound, "Room not found: " + room_id};
  }
  return room_it->second;
}

std::vector<Room> RoomRegistry::List() const {
  std::vector<Room> rooms;
  rooms.reserve(rooms_.size());
  for (const auto& [room_id, room] : rooms_) {
    static_cast<void>(room_id);
    rooms.push_back(room);
  }
  return rooms;
}

core::Status RoomRegistry::PublishEvent(const RoomEvent& event) {
  event_bus_.Publish(ToEventEnvelope(event));
  return core::OkStatus();
}

}  // namespace daffy::rooms
