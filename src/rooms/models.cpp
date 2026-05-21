#include "daffy/rooms/models.hpp"
#include "daffy/core/time.hpp"

namespace daffy::rooms {

std::string ToString(const RoomState state) {
  switch (state) {
    case RoomState::kProvisioning:
      return "provisioning";
    case RoomState::kActive:
      return "active";
    case RoomState::kClosing:
      return "closing";
    case RoomState::kClosed:
      return "closed";
  }
  return "provisioning";
}

std::string ToString(const ParticipantRole role) {
  switch (role) {
    case ParticipantRole::kMember:
      return "member";
    case ParticipantRole::kAdmin:
      return "admin";
    case ParticipantRole::kBot:
      return "bot";
  }
  return "member";
}

std::string ToString(const SessionState state) {
  switch (state) {
    case SessionState::kPending:
      return "pending";
    case SessionState::kNegotiating:
      return "negotiating";
    case SessionState::kConnected:
      return "connected";
    case SessionState::kDisconnected:
      return "disconnected";
  }
  return "pending";
}

util::json::Value ParticipantToJson(const Participant& participant) {
  return util::json::Value::Object{{"id", participant.id},
                                   {"display_name", participant.display_name},
                                   {"role", ToString(participant.role)},
                                   {"joined_at", participant.joined_at}};
}

util::json::Value PeerSessionToJson(const PeerSession& session) {
  return util::json::Value::Object{{"id", session.id},
                                   {"participant_id", session.participant_id},
                                   {"peer_id", session.peer_id},
                                   {"state", ToString(session.state)},
                                   {"created_at", session.created_at}};
}

util::json::Value RoomToJson(const Room& room) {
  util::json::Value::Array participants;
  for (const auto& participant : room.participants) {
    participants.push_back(ParticipantToJson(participant));
  }

  util::json::Value::Array sessions;
  for (const auto& session : room.sessions) {
    sessions.push_back(PeerSessionToJson(session));
  }

  return util::json::Value::Object{{"id", room.id},
                                   {"display_name", room.display_name},
                                   {"custom_name", room.custom_name},
                                   {"is_password_protected", room.is_password_protected},
                                   {"is_e2e_encrypted", room.is_e2e_encrypted},
                                   {"e2e_secret", room.e2e_secret},
                                   {"state", ToString(room.state)},
                                   {"created_at", room.created_at},
                                   {"participants", participants},
                                   {"sessions", sessions}};
}

bool Room::IsDead(int inactivity_threshold_seconds) const {
  if (state == RoomState::kClosed) {
    return true;
  }
  
  if (last_activity_at.empty()) {
    return false;
  }
  
  // Simple check: if no participants and not active, consider dead
  return participants.empty() && state != RoomState::kActive;
}

}  // namespace daffy::rooms
