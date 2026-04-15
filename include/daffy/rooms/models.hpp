#pragma once

#include <string>
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
  RoomState state{RoomState::kProvisioning};
  std::string created_at;
  std::vector<Participant> participants;
  std::vector<PeerSession> sessions;
};

std::string ToString(RoomState state);
std::string ToString(ParticipantRole role);
std::string ToString(SessionState state);
util::json::Value ParticipantToJson(const Participant& participant);
util::json::Value PeerSessionToJson(const PeerSession& session);
util::json::Value RoomToJson(const Room& room);

}  // namespace daffy::rooms
