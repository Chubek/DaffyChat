#pragma once

#include <string>
#include "daffy/core/error.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/rooms/room_registry.hpp"
#include "daffy/util/json.hpp"

namespace daffy::services {

class RoomManagementService {
 public:
  explicit RoomManagementService(rooms::RoomRegistry& registry, core::Logger logger);
  
  // HTTP endpoint handlers
  util::json::Value HandleCreateRoom(const util::json::Value& request);
  util::json::Value HandleAuthenticateRoom(const util::json::Value& request);
  util::json::Value HandleDeleteRoom(const util::json::Value& request);
  util::json::Value HandleGetRoom(const std::string& room_name);
  util::json::Value HandleListRooms();
  util::json::Value HandleSuggestNames(const std::string& attempted_name);
  
 private:
  rooms::RoomRegistry& registry_;
  core::Logger logger_;
};

}  // namespace daffy::services
