#include "daffy/services/room_management_service.hpp"
#include "daffy/core/id.hpp"

namespace daffy::services {

namespace {

bool IsValidRoomName(const std::string& name) {
  return daffy::rooms::ValidateRoomName(name);
}

}  // namespace

RoomManagementService::RoomManagementService(rooms::RoomRegistry& registry, core::Logger logger)
    : registry_(registry), logger_(std::move(logger)) {}

util::json::Value RoomManagementService::HandleCreateRoom(const util::json::Value& request) {
  try {
    auto obj = request.AsObject();
    std::string display_name = obj.at("display_name").AsString();
    std::string custom_name = obj.at("custom_name").AsString();
    std::string password = obj.count("password") ? obj.at("password").AsString() : "";
    std::string creator_id = obj.count("creator_id") ? obj.at("creator_id").AsString() : core::GenerateId("user");

    if (!IsValidRoomName(custom_name)) {
      return util::json::Value::Object{
        {"success", false},
        {"error", "Invalid room name: must be 4-10 chars, alphanumeric and dash only"}
      };
    }
    
    auto result = registry_.CreateRoom(display_name, custom_name, password, creator_id);
    
    if (!result.ok()) {
      return util::json::Value::Object{
        {"success", false},
        {"error", result.error().message()}
      };
    }
    
    const auto& room = result.value();
    return util::json::Value::Object{
      {"success", true},
      {"room", util::json::Value::Object{
        {"id", room.id},
        {"custom_name", room.custom_name},
        {"display_name", room.display_name},
        {"is_password_protected", room.is_password_protected},
        {"creator_id", room.creator_id},
        {"created_at", room.created_at}
      }}
    };
  } catch (const std::exception& e) {
    return util::json::Value::Object{
      {"success", false},
      {"error", std::string("Invalid request: ") + e.what()}
    };
  }
}

util::json::Value RoomManagementService::HandleAuthenticateRoom(const util::json::Value& request) {
  try {
    auto obj = request.AsObject();
    std::string custom_name = obj.at("custom_name").AsString();
    std::string password = obj.at("password").AsString();
    
    auto result = registry_.AuthenticateRoom(custom_name, password);
    
    if (!result.ok()) {
      return util::json::Value::Object{
        {"success", false},
        {"error", result.error().message()}
      };
    }
    
    return util::json::Value::Object{
      {"success", true},
      {"token", result.value()}
    };
  } catch (const std::exception& e) {
    return util::json::Value::Object{
      {"success", false},
      {"error", std::string("Invalid request: ") + e.what()}
    };
  }
}

util::json::Value RoomManagementService::HandleDeleteRoom(const util::json::Value& request) {
  try {
    auto obj = request.AsObject();
    std::string room_id = obj.at("room_id").AsString();
    std::string requester_id = obj.at("requester_id").AsString();
    
    auto status = registry_.DeleteRoom(room_id, requester_id);
    
    if (!status.ok()) {
      return util::json::Value::Object{
        {"success", false},
        {"error", status.error().message()}
      };
    }
    
    return util::json::Value::Object{
      {"success", true},
      {"message", "Room deleted successfully"}
    };
  } catch (const std::exception& e) {
    return util::json::Value::Object{
      {"success", false},
      {"error", std::string("Invalid request: ") + e.what()}
    };
  }
}

util::json::Value RoomManagementService::HandleGetRoom(const std::string& room_name) {
  auto result = registry_.FindByCustomName(room_name);
  
  if (!result.ok()) {
    return util::json::Value::Object{
      {"success", false},
      {"error", result.error().message()}
    };
  }
  
  const auto& room = result.value();
  return util::json::Value::Object{
    {"success", true},
    {"room", rooms::RoomToJson(room)}
  };
}

util::json::Value RoomManagementService::HandleListRooms() {
  auto rooms = registry_.List();
  
  util::json::Value::Array room_array;
  for (const auto& room : rooms) {
    room_array.push_back(rooms::RoomToJson(room));
  }
  
  return util::json::Value::Object{
    {"success", true},
    {"rooms", room_array}
  };
}

util::json::Value RoomManagementService::HandleSuggestNames(const std::string& attempted_name) {
  // Use Natural.js-like logic for suggestions (simplified here)
  // In production, this would call Natural.js on the frontend
  
  util::json::Value::Array suggestions;
  
  // Generate some variations
  for (int i = 1; i <= 5; ++i) {
    suggestions.push_back(attempted_name + std::to_string(i));
  }
  
  return util::json::Value::Object{
    {"success", true},
    {"suggestions", suggestions}
  };
}

}  // namespace daffy::services
