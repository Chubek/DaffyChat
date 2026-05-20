#include "daffy/rooms/room_auth.hpp"

#include <chrono>
#include <jwt-cpp/jwt.h>

namespace daffy::rooms {

RoomAuthService::RoomAuthService(const std::string& jwt_secret)
    : jwt_secret_(jwt_secret) {}

std::string RoomAuthService::GenerateToken(const std::string& room_id,
                                          const std::string& custom_name,
                                          const std::string& participant_id,
                                          int expiry_seconds) {
  auto token = jwt::create()
    .set_issuer("daffychat")
    .set_type("JWT")
    .set_issued_now()
    .set_expires_in(std::chrono::seconds(expiry_seconds))
    .set_payload_claim("room_id", jwt::claim(room_id))
    .set_payload_claim("custom_name", jwt::claim(custom_name))
    .set_payload_claim("participant_id", jwt::claim(participant_id))
    .sign(jwt::algorithm::hs256{jwt_secret_});
  
  return token;
}

core::Result<RoomAuthToken> RoomAuthService::VerifyToken(const std::string& token) const {
  try {
    auto verifier = jwt::verify()
      .allow_algorithm(jwt::algorithm::hs256{jwt_secret_})
      .with_issuer("daffychat");
    
    auto decoded = jwt::decode(token);
    verifier.verify(decoded);
    
    RoomAuthToken auth_token;
    auth_token.room_id = decoded.get_payload_claim("room_id").as_string();
    auth_token.custom_name = decoded.get_payload_claim("custom_name").as_string();
    auth_token.participant_id = decoded.get_payload_claim("participant_id").as_string();
    auth_token.issued_at = std::chrono::duration_cast<std::chrono::seconds>(
                               decoded.get_issued_at().time_since_epoch())
                               .count();
    auth_token.expires_at = std::chrono::duration_cast<std::chrono::seconds>(
                                decoded.get_expires_at().time_since_epoch())
                                .count();
    
    return auth_token;
  } catch (const std::exception& e) {
    return core::Error{core::ErrorCode::kUnauthenticated, 
                      "Invalid or expired token: " + std::string(e.what())};
  }
}

}  // namespace daffy::rooms
