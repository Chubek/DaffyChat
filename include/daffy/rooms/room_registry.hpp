#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include "daffy/core/error.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/rooms/events.hpp"
#include "daffy/rooms/models.hpp"
#include "daffy/runtime/event_bus.hpp"
#include "daffy/rooms/room_database.hpp"
#include "daffy/rooms/room_auth.hpp"

namespace daffy::rooms {

class RoomRegistry {
 public:
  RoomRegistry(core::Logger logger, runtime::EventBus& event_bus);

  core::Result<Room> CreateRoom(std::string display_name, std::string custom_name, 
                                std::string password = "", std::string creator_id = "");
  core::Result<Participant> AddParticipant(const RoomId& room_id, std::string display_name, ParticipantRole role);
  core::Result<PeerSession> AttachSession(const RoomId& room_id, const ParticipantId& participant_id, std::string peer_id);
  core::Result<Room> TransitionRoomState(const RoomId& room_id, RoomState state);
  core::Result<Room> Find(const RoomId& room_id) const;
  core::Result<Room> FindByCustomName(const std::string& custom_name) const;
  core::Status DeleteRoom(const RoomId& room_id, const ParticipantId& requester_id);
  
  // Authentication
  core::Result<std::string> AuthenticateRoom(const std::string& custom_name, const std::string& password);
  bool IsRoomCreator(const RoomId& room_id, const ParticipantId& participant_id) const;
  
  std::vector<Room> List() const;

 private:
  core::Status PublishEvent(const RoomEvent& event);

  core::Logger logger_;
  runtime::EventBus& event_bus_;
  std::unordered_map<RoomId, Room> rooms_;
  std::unique_ptr<RoomDatabase> db_;
  std::unique_ptr<RoomAuthService> auth_;
};

}  // namespace daffy::rooms
