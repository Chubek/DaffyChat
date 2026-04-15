#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/rooms/events.hpp"
#include "daffy/rooms/models.hpp"
#include "daffy/runtime/event_bus.hpp"

namespace daffy::rooms {

class RoomRegistry {
 public:
  RoomRegistry(core::Logger logger, runtime::EventBus& event_bus);

  core::Result<Room> CreateRoom(std::string display_name);
  core::Result<Participant> AddParticipant(const RoomId& room_id, std::string display_name, ParticipantRole role);
  core::Result<PeerSession> AttachSession(const RoomId& room_id, const ParticipantId& participant_id, std::string peer_id);
  core::Result<Room> TransitionRoomState(const RoomId& room_id, RoomState state);
  core::Result<Room> Find(const RoomId& room_id) const;
  std::vector<Room> List() const;

 private:
  core::Status PublishEvent(const RoomEvent& event);

  core::Logger logger_;
  runtime::EventBus& event_bus_;
  std::unordered_map<RoomId, Room> rooms_;
};

}  // namespace daffy::rooms
