#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/rooms/models.hpp"

namespace daffy::rooms {

// Room database record for persistence
struct RoomRecord {
  std::string id;
  std::string display_name;
  std::string custom_name;
  std::string password_hash;
  std::string creator_id;
  std::string state;
  std::string created_at;
  std::string last_activity_at;
  bool is_password_protected;
  bool is_e2e_encrypted;
  std::string e2e_secret;
  
  std::string Serialize() const;
  static RoomRecord Deserialize(const std::string& data);
  Room ToRoom() const;
  static RoomRecord FromRoom(const Room& room);
};

class RoomDatabase {
 public:
  explicit RoomDatabase(const std::string& db_path, core::Logger logger);
  ~RoomDatabase();

  // Room operations
  core::Result<Room> CreateRoom(const Room& room);
  core::Result<Room> GetRoomById(const RoomId& room_id) const;
  core::Result<Room> GetRoomByCustomName(const std::string& custom_name) const;
  core::Status UpdateRoom(const Room& room);
  core::Status DeleteRoom(const RoomId& room_id);
  
  // Name management
  bool CustomNameExists(const std::string& custom_name) const;
  std::vector<std::string> FindSimilarNames(const std::string& name, int limit = 5) const;
  std::vector<Room> ListAllRooms() const;
  std::vector<Room> FindDeadRooms(int inactivity_threshold_seconds = 3600) const;
  
  // Password verification
  bool VerifyPassword(const std::string& custom_name, const std::string& password_hash) const;

 private:
  std::string db_path_;
  core::Logger logger_;
  
  // ZethaDB instances (using pimpl to avoid header dependency)
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Utility functions for password hashing
std::string HashPassword(const std::string& password);
bool ValidateRoomName(const std::string& name);

}  // namespace daffy::rooms
