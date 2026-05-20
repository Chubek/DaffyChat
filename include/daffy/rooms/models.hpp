#pragma once

#include <string>
#include <optional>
#include <vector>

#include "daffy/util/json.hpp"

namespace daffy::rooms {

using RoomId = std::string;
using ParticipantId = std::string;
using PeerId = std::string;
using SessionId = std::string;

enum class RoomState {
  kProvisioning,
  kActive,
  kClosing,
  kClosed
};

enum class ParticipantRole {
  kMember,
  kAdmin,
  kBot
};

enum class SessionState {
  kPending,
  kNegotiating,
  kConnected,
  kDisconnected
};

struct Participant {
  ParticipantId id;
  std::string display_name;
  ParticipantRole role{ParticipantRole::kMember};
  std::string joined_at;
};

struct PeerSession {
  SessionId id;
  ParticipantId participant_id;
  PeerId peer_id;
  SessionState state{SessionState::kPending};
  std::string created_at;
};

struct Room {
  RoomId id;
  std::string display_name;
  std::string custom_name;  // User-chosen short name (4-10 chars, alphanumeric + dash)
  std::string password_hash;  // SHA-512 hash of password (empty if no password)
  ParticipantId creator_id;  // ID of the participant who created the room
  RoomState state{RoomState::kProvisioning};
  std::string created_at;
  std::string last_activity_at;  // For detecting dead rooms
  bool is_password_protected{false};
  std::vector<Participant> participants;
  std::vector<PeerSession> sessions;
  
  bool IsDead(int inactivity_threshold_seconds = 3600) const;  // Default 1 hour
};

std::string ToString(RoomState state);
std::string ToString(ParticipantRole role);
std::string ToString(SessionState state);
util::json::Value ParticipantToJson(const Participant& participant);
util::json::Value PeerSessionToJson(const PeerSession& session);
util::json::Value RoomToJson(const Room& room);

}  // namespace daffy::rooms
