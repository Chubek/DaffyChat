#include "daffy/rooms/room_database.hpp"

#include <cctype>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <unordered_map>

#include "picotls/minicrypto.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "picotls/pembase64.h"
#ifdef __cplusplus
}
#endif
#include "third_party/ZethaDB/ZethaRDB.hpp"
#include "third_party/ZethaDB/zbase64.hpp"
#include "third_party/sha2/sha2.h"

namespace daffy::rooms {

// RoomRecord serialization
std::string RoomRecord::Serialize() const {
  std::ostringstream oss;
  oss << id << "|" << display_name << "|" << custom_name << "|" 
      << password_hash << "|" << creator_id << "|" << state << "|"
      << created_at << "|" << last_activity_at << "|" 
      << (is_password_protected ? "1" : "0") << "|"
      << (is_e2e_encrypted ? "1" : "0") << "|" << e2e_secret;
  
  // Base64 encode to handle special characters
  return zbase64::encode(oss.str());
}

RoomRecord RoomRecord::Deserialize(const std::string& data) {
  std::string decoded = zbase64::decode(data);
  std::istringstream iss(decoded);
  RoomRecord record;
  
  std::string protected_flag;
  std::string encrypted_flag;
  std::getline(iss, record.id, '|');
  std::getline(iss, record.display_name, '|');
  std::getline(iss, record.custom_name, '|');
  std::getline(iss, record.password_hash, '|');
  std::getline(iss, record.creator_id, '|');
  std::getline(iss, record.state, '|');
  std::getline(iss, record.created_at, '|');
  std::getline(iss, record.last_activity_at, '|');
  std::getline(iss, protected_flag, '|');
  std::getline(iss, encrypted_flag, '|');
  std::getline(iss, record.e2e_secret);

  record.is_password_protected = (protected_flag == "1");
  record.is_e2e_encrypted = (encrypted_flag == "1");
  return record;
}

Room RoomRecord::ToRoom() const {
  Room room;
  room.id = id;
  room.display_name = display_name;
  room.custom_name = custom_name;
  room.password_hash = password_hash;
  room.creator_id = creator_id;
  room.created_at = created_at;
  room.last_activity_at = last_activity_at;
  room.is_password_protected = is_password_protected;
  room.is_e2e_encrypted = is_e2e_encrypted;
  room.e2e_secret = e2e_secret;
  
  // Parse state
  if (state == "active") room.state = RoomState::kActive;
  else if (state == "closing") room.state = RoomState::kClosing;
  else if (state == "closed") room.state = RoomState::kClosed;
  else room.state = RoomState::kProvisioning;
  
  return room;
}

RoomRecord RoomRecord::FromRoom(const Room& room) {
  RoomRecord record;
  record.id = room.id;
  record.display_name = room.display_name;
  record.custom_name = room.custom_name;
  record.password_hash = room.password_hash;
  record.creator_id = room.creator_id;
  record.state = ToString(room.state);
  record.created_at = room.created_at;
  record.last_activity_at = room.last_activity_at;
  record.is_password_protected = room.is_password_protected;
  record.is_e2e_encrypted = room.is_e2e_encrypted;
  record.e2e_secret = room.e2e_secret;
  return record;
}

// Pimpl implementation
struct RoomDatabase::Impl {
  zetha::rdb::ZethaRDB rdb;
  std::unordered_map<std::string, RoomId> name_index;
  bool ready{false};

  Impl(const std::string& db_path) {
    auto opened = zetha::rdb::ZethaRDB::open(db_path);
    if (opened) {
      rdb = std::move(*opened.value);
      ready = true;
      return;
    }

    zetha::rdb::CreateOptions options;
    options.schema_text = "database daffy_rooms table rooms(id text primary key, payload text)";
    auto created = zetha::rdb::ZethaRDB::create(db_path, options);
    if (created) {
      rdb = std::move(*created.value);
      ready = true;
    }
  }
};

RoomDatabase::RoomDatabase(const std::string& db_path, core::Logger logger)
    : db_path_(db_path), logger_(std::move(logger)), impl_(std::make_unique<Impl>(db_path)) {
  logger_.Info("RoomDatabase initialized at: " + db_path);
}

RoomDatabase::~RoomDatabase() = default;

core::Result<Room> RoomDatabase::CreateRoom(const Room& room) {
  if (!ValidateRoomName(room.custom_name)) {
    return core::Error{core::ErrorCode::kInvalidArgument, 
                      "Invalid room name: must be 4-10 chars, alphanumeric and dash only"};
  }
  
  if (CustomNameExists(room.custom_name)) {
    return core::Error{core::ErrorCode::kAlreadyExists, 
                      "Room name already exists: " + room.custom_name};
  }
  
  RoomRecord record = RoomRecord::FromRoom(room);
  if (!impl_->ready) {
    return core::Error{core::ErrorCode::kUnavailable, "Room database is not initialized"};
  }
  zetha::rdb::Row row;
  row["id"] = zetha::rdb::Value::Text(room.id);
  row["payload"] = zetha::rdb::Value::Text(record.Serialize());
  auto inserted = impl_->rdb.insert("rooms", std::move(row));
  if (!inserted) {
    return core::Error{core::ErrorCode::kStateError, "Failed to insert room record"};
  }
  impl_->name_index[room.custom_name] = room.id;
  
  logger_.Info("Created room: " + room.custom_name + " (ID: " + room.id + ")");
  return room;
}

core::Result<Room> RoomDatabase::GetRoomById(const RoomId& room_id) const {
  if (!impl_->ready) {
    return core::Error{core::ErrorCode::kUnavailable, "Room database is not initialized"};
  }
  auto fetched = impl_->rdb.get("rooms", room_id);
  if (!fetched) {
    return core::Error{core::ErrorCode::kNotFound, "Room not found: " + room_id};
  }
  if (!fetched.value->has_value()) {
    return core::Error{core::ErrorCode::kNotFound, "Room not found: " + room_id};
  }
  const auto& row = **fetched.value;
  const auto payload_it = row.find("payload");
  if (payload_it == row.end()) {
    return core::Error{core::ErrorCode::kStateError, "Room payload is missing"};
  }
  return RoomRecord::Deserialize(payload_it->second.data).ToRoom();
}

core::Result<Room> RoomDatabase::GetRoomByCustomName(const std::string& custom_name) const {
  const auto it = impl_->name_index.find(custom_name);
  if (it == impl_->name_index.end()) {
    return core::Error{core::ErrorCode::kNotFound, "Room not found: " + custom_name};
  }
  return GetRoomById(it->second);
}

core::Status RoomDatabase::UpdateRoom(const Room& room) {
  RoomRecord record = RoomRecord::FromRoom(room);
  if (!impl_->ready) {
    return core::Error{core::ErrorCode::kUnavailable, "Room database is not initialized"};
  }
  zetha::rdb::Row patch;
  patch["payload"] = zetha::rdb::Value::Text(record.Serialize());
  auto updated = impl_->rdb.update("rooms", room.id, patch);
  if (!updated) {
    return core::Error{core::ErrorCode::kStateError, "Failed to update room record"};
  }
  impl_->name_index[room.custom_name] = room.id;
  logger_.Info("Updated room: " + room.custom_name);
  return core::OkStatus();
}

core::Status RoomDatabase::DeleteRoom(const RoomId& room_id) {
  auto room_result = GetRoomById(room_id);
  if (!room_result.ok()) {
    return room_result.error();
  }
  
  auto erased = impl_->rdb.erase("rooms", room_id);
  if (!erased) {
    return core::Error{core::ErrorCode::kStateError, "Failed to delete room record"};
  }
  impl_->name_index.erase(room_result.value().custom_name);
  logger_.Info("Deleted room: " + room_id);
  return core::OkStatus();
}

bool RoomDatabase::CustomNameExists(const std::string& custom_name) const {
  return impl_->name_index.find(custom_name) != impl_->name_index.end();
}

std::vector<std::string> RoomDatabase::FindSimilarNames(const std::string& name, int limit) const {
  // Simple similarity: find names with similar prefixes or edit distance
  std::vector<std::string> similar;
  
  // This is a placeholder - in production, implement proper string similarity
  // For now, just return empty vector
  return similar;
}

std::vector<Room> RoomDatabase::ListAllRooms() const {
  std::vector<Room> rooms;
  rooms.reserve(impl_->name_index.size());
  for (const auto& [custom_name, room_id] : impl_->name_index) {
    static_cast<void>(custom_name);
    auto room = GetRoomById(room_id);
    if (room.ok()) {
      rooms.push_back(room.value());
    }
  }
  return rooms;
}

std::vector<Room> RoomDatabase::FindDeadRooms(int inactivity_threshold_seconds) const {
  std::vector<Room> dead_rooms;
  auto all_rooms = ListAllRooms();
  
  for (const auto& room : all_rooms) {
    if (room.IsDead(inactivity_threshold_seconds)) {
      dead_rooms.push_back(room);
    }
  }
  
  return dead_rooms;
}

bool RoomDatabase::VerifyPassword(const std::string& custom_name, const std::string& password_hash) const {
  auto room_result = GetRoomByCustomName(custom_name);
  if (!room_result.ok()) {
    return false;
  }
  
  return room_result.value().password_hash == password_hash;
}

// Utility functions
std::string HashPassword(const std::string& password) {
  uint8 hash[SHA512_DIGEST_SIZE];
  sha512_ctx ctx;
  sha512_init(&ctx);
  sha512_update(&ctx, reinterpret_cast<const uint8*>(password.data()), static_cast<uint64>(password.size()));
  sha512_final(&ctx, hash);
  
  // Convert to hex string
  std::ostringstream oss;
  for (int i = 0; i < SHA512_DIGEST_SIZE; ++i) {
    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
  }
  
  return oss.str();
}

bool ValidateRoomName(const std::string& name) {
  if (name.length() < 4 || name.length() > 10) {
    return false;
  }
  
  for (char c : name) {
    const unsigned char ch = static_cast<unsigned char>(c);
    const bool is_ascii_alnum =
        (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
    if (!is_ascii_alnum && c != '-') {
      return false;
    }
  }
  
  return true;
}

std::string GenerateRoomSecret() {
  unsigned char secret[32];
  ptls_minicrypto_random_bytes(secret, sizeof(secret));

  const auto encoded_size = ptls_base64_howlong(sizeof(secret));
  std::string encoded(encoded_size + 1, '\0');
  if (ptls_base64_encode(secret, sizeof(secret), encoded.data()) != 0) {
    return {};
  }
  encoded.resize(encoded_size);
  return encoded;
}

}  // namespace daffy::rooms
