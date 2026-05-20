#pragma once

#include <cstdint>
#include <string>
#include <optional>

#include "daffy/core/error.hpp"

namespace daffy::rooms {

struct RoomAuthToken {
  std::string room_id;
  std::string custom_name;
  std::string participant_id;
  std::int64_t issued_at;
  std::int64_t expires_at;
};

class RoomAuthService {
 public:
  explicit RoomAuthService(const std::string& jwt_secret);
  
  // Generate JWT token for authenticated room access
  std::string GenerateToken(const std::string& room_id, 
                           const std::string& custom_name,
                           const std::string& participant_id,
                           int expiry_seconds = 3600);
  
  // Verify and decode JWT token
  core::Result<RoomAuthToken> VerifyToken(const std::string& token) const;
  
 private:
  std::string jwt_secret_;
};

}  // namespace daffy::rooms
