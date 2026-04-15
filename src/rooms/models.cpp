#include "daffy/rooms/models.hpp"

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
                                   {"state", ToString(room.state)},
                                   {"created_at", room.created_at},
                                   {"participants", participants},
                                   {"sessions", sessions}};
}

}  // namespace daffy::rooms
