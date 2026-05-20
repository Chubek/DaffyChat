#include "daffy/rooms/room_registry.hpp"

#include <algorithm>

#include "daffy/core/id.hpp"
#include "daffy/core/time.hpp"

namespace daffy::rooms {

RoomRegistry::RoomRegistry(core::Logger logger, runtime::EventBus& event_bus)
    : logger_(std::move(logger)), event_bus_(event_bus) {
  // Initialize database and auth service
  db_ = std::make_unique<RoomDatabase>("./data/rooms.db", logger_);
  auth_ = std::make_unique<RoomAuthService>("daffychat-secret-key-change-in-production");
}

core::Result<Room> RoomRegistry::CreateRoom(std::string display_name, std::string custom_name,
                                            std::string password, std::string creator_id) {
  // Validate custom name
  if (!ValidateRoomName(custom_name)) {
    return core::Error{core::ErrorCode::kInvalidArgument,
                      "Invalid room name: must be 4-10 chars, alphanumeric and dash only"};
  }
  
  // Check for duplicates and handle dead rooms
  if (db_->CustomNameExists(custom_name)) {
    auto existing_result = db_->GetRoomByCustomName(custom_name);
    if (existing_result.ok() && existing_result.value().IsDead()) {
      // Delete dead room and reuse the name
      db_->DeleteRoom(existing_result.value().id);
      logger_.Info("Deleted dead room to reuse name: " + custom_name);
    } else {
      return core::Error{core::ErrorCode::kAlreadyExists,
                        "Room name already exists: " + custom_name};
    }
  }
  
  Room room;
  room.id = core::GenerateId("room");
  room.display_name = std::move(display_name);
  room.custom_name = std::move(custom_name);
  room.creator_id = std::move(creator_id);
  room.state = RoomState::kActive;
  room.created_at = core::UtcNowIso8601();
  room.last_activity_at = room.created_at;
  
  // Handle password
  if (!password.empty()) {
    room.password_hash = HashPassword(password);
    room.is_password_protected = true;
  }
  
  // Store in memory and database
  rooms_.emplace(room.id, room);
  auto db_result = db_->CreateRoom(room);
  if (!db_result.ok()) {
    rooms_.erase(room.id);
    return db_result.error();
  }
  
  logger_.Info("Created room " + room.id + " (" + room.custom_name + ")");

  RoomEvent event;
  event.kind = RoomEventKind::kRoomCreated;
  event.room_id = room.id;
  event.occurred_at = room.created_at;
  event.message = "Room created: " + room.custom_name;
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
  
  // Update last activity
  room_it->second.last_activity_at = participant.joined_at;
  db_->UpdateRoom(room_it->second);

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
  
  // Update last activity
  room_it->second.last_activity_at = session.created_at;
  db_->UpdateRoom(room_it->second);

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
  room_it->second.last_activity_at = core::UtcNowIso8601();
  db_->UpdateRoom(room_it->second);
  
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
    // Try loading from database
    return db_->GetRoomById(room_id);
  }
  return room_it->second;
}

core::Result<Room> RoomRegistry::FindByCustomName(const std::string& custom_name) const {
  // Search in memory first
  for (const auto& [id, room] : rooms_) {
    if (room.custom_name == custom_name) {
      return room;
    }
  }
  
  // Fall back to database
  return db_->GetRoomByCustomName(custom_name);
}

core::Status RoomRegistry::DeleteRoom(const RoomId& room_id, const ParticipantId& requester_id) {
  const auto room_it = rooms_.find(room_id);
  if (room_it == rooms_.end()) {
    return core::Error{core::ErrorCode::kNotFound, "Room not found: " + room_id};
  }
  
  // Check if requester is the creator
  if (room_it->second.creator_id != requester_id) {
    return core::Error{core::ErrorCode::kPermissionDenied, 
                      "Only the room creator can delete the room"};
  }
  
  // Delete from database and memory
  auto status = db_->DeleteRoom(room_id);
  if (status.ok()) {
    rooms_.erase(room_it);
    logger_.Info("Deleted room: " + room_id);
  }
  
  return status;
}

core::Result<std::string> RoomRegistry::AuthenticateRoom(const std::string& custom_name, 
                                                         const std::string& password) {
  auto room_result = FindByCustomName(custom_name);
  if (!room_result.ok()) {
    return room_result.error();
  }
  
  const auto& room = room_result.value();
  
  if (!room.is_password_protected) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Room is not password protected"};
  }
  
  std::string password_hash = HashPassword(password);
  if (room.password_hash != password_hash) {
    return core::Error{core::ErrorCode::kUnauthenticated, "Invalid password"};
  }
  
  // Generate JWT token
  std::string participant_id = core::GenerateId("participant");
  std::string token = auth_->GenerateToken(room.id, room.custom_name, participant_id);
  
  return token;
}

bool RoomRegistry::IsRoomCreator(const RoomId& room_id, const ParticipantId& participant_id) const {
  auto room_result = Find(room_id);
  if (!room_result.ok()) {
    return false;
  }
  
  return room_result.value().creator_id == participant_id;
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
