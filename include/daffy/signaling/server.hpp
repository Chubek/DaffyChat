#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "daffy/config/app_config.hpp"
#include "daffy/core/error.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/signaling/messages.hpp"
#include "daffy/util/json.hpp"

namespace daffy::signaling {

enum class PeerSessionPhase {
  kAwaitingJoin,
  kWaitingForPeer,
  kOfferSent,
  kOfferReceived,
  kAnswerSent,
  kNegotiating,
  kConnected,
  kDisconnected
};

struct IceCandidateRecord {
  std::string candidate;
  std::string mid;
  std::string received_at;
};

struct PeerSessionSnapshot {
  std::string room;
  std::string peer_id;
  std::string session_id;
  std::string connection_id;
  PeerSessionPhase phase{PeerSessionPhase::kAwaitingJoin};
  std::string counterpart_peer_id;
  std::string joined_at;
  std::string last_seen_at;
  std::string disconnected_at;
  bool browser_client{false};
  bool has_offer{false};
  bool has_answer{false};
  std::size_t reconnect_count{0};
  std::vector<IceCandidateRecord> candidates;
};

struct RoomSnapshot {
  std::string room;
  std::string created_at;
  std::size_t max_room_size{0};
  std::vector<PeerSessionSnapshot> active_peers;
  std::vector<PeerSessionSnapshot> stale_peers;
};

struct ConnectionContext {
  std::string connection_id;
  std::string remote_address{"local"};
  std::string user_agent{"in-process"};
  bool browser_client{false};
};

struct OutboundEnvelope {
  std::string connection_id;
  Message message;
};

struct DispatchResult {
  bool accepted{true};
  std::vector<OutboundEnvelope> outgoing;
};

struct HealthSnapshot {
  std::size_t active_rooms{0};
  std::size_t active_connections{0};
  std::size_t active_peers{0};
  std::size_t stale_peers{0};
  bool uwebsockets_runtime_available{false};
  bool turn_configured{false};
  std::string transport_status;
  std::string health_endpoint;
  std::string debug_rooms_endpoint;
  std::string turn_credentials_endpoint;
  std::string turn_credential_mode;
  std::int64_t reconnect_grace_ms{0};
};

struct TurnCredentials {
  std::string uri;
  std::string username;
  std::string password;
  std::string credential_mode;
  std::string issued_at;
  std::string expires_at;
};

std::string ToString(PeerSessionPhase phase);
util::json::Value IceCandidateRecordToJson(const IceCandidateRecord& candidate);
util::json::Value PeerSessionSnapshotToJson(const PeerSessionSnapshot& snapshot);
util::json::Value RoomSnapshotToJson(const RoomSnapshot& snapshot);
util::json::Value HealthSnapshotToJson(const HealthSnapshot& snapshot);
util::json::Value TurnCredentialsToJson(const TurnCredentials& credentials);

class SignalingServer {
 public:
  struct ConnectionRecord {
    ConnectionContext context;
    std::string opened_at;
    std::string last_seen_at;
    std::int64_t last_seen_ms{0};
    std::string room;
    std::string peer_id;
  };

  struct PeerRecord {
    std::string room;
    std::string peer_id;
    std::string session_id;
    std::string connection_id;
    PeerSessionPhase phase{PeerSessionPhase::kAwaitingJoin};
    std::string counterpart_peer_id;
    std::string joined_at;
    std::string last_seen_at;
    std::int64_t last_seen_ms{0};
    std::string disconnected_at;
    std::int64_t disconnected_ms{0};
    bool browser_client{false};
    std::size_t reconnect_count{0};
    std::string last_offer_sdp;
    std::string last_answer_sdp;
    std::vector<IceCandidateRecord> candidates;
  };

  struct RoomRecord {
    std::string room;
    std::string created_at;
    std::unordered_map<std::string, PeerRecord> active_peers;
    std::unordered_map<std::string, PeerRecord> stale_peers;
  };

  SignalingServer(config::AppConfig config, core::Logger logger);

  void OpenConnection(ConnectionContext context);
  DispatchResult HandleMessage(const std::string& connection_id, std::string_view json_payload);
  std::vector<OutboundEnvelope> CloseConnection(const std::string& connection_id);
  std::size_t PruneStaleSessions();
  std::size_t PruneStaleSessions(std::int64_t now_ms);

  HealthSnapshot SnapshotHealth() const;
  std::vector<RoomSnapshot> SnapshotRooms() const;
  util::json::Value HealthToJson() const;
  util::json::Value DebugStateToJson() const;

  core::Result<TurnCredentials> IssueTurnCredentials(std::string_view room, std::string_view peer_id) const;
  bool HasUWebSocketsRuntimeDependencies() const;
  std::int64_t reconnect_grace_ms() const;

 private:
  friend struct ServerAccess;

  DispatchResult HandleJoin(ConnectionRecord& connection, const Message& message);
  DispatchResult HandleLeave(ConnectionRecord& connection, const Message& message);
  DispatchResult HandleOffer(ConnectionRecord& connection, const Message& message);
  DispatchResult HandleAnswer(ConnectionRecord& connection, const Message& message);
  DispatchResult HandleIceCandidate(ConnectionRecord& connection, const Message& message);

  DispatchResult Reject(const std::string& connection_id,
                        std::string room,
                        std::string code,
                        std::string description) const;
  std::vector<OutboundEnvelope> DetachConnection(ConnectionRecord& connection, bool keep_stale_session);

  config::AppConfig config_;
  core::Logger logger_;
  std::unordered_map<std::string, ConnectionRecord> connections_;
  std::unordered_map<std::string, RoomRecord> rooms_;
};

}  // namespace daffy::signaling
