#include "daffy/signaling/server.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <unordered_map>
#include <utility>

#include "daffy/core/id.hpp"
#include "daffy/core/time.hpp"

#ifndef DAFFY_SOURCE_DIR
#define DAFFY_SOURCE_DIR "."
#endif

namespace daffy::signaling {
struct ServerAccess {
  static std::unordered_map<std::string, SignalingServer::ConnectionRecord>& Connections(SignalingServer& server) {
    return server.connections_;
  }

  static const std::unordered_map<std::string, SignalingServer::ConnectionRecord>& Connections(
      const SignalingServer& server) {
    return server.connections_;
  }

  static std::unordered_map<std::string, SignalingServer::RoomRecord>& Rooms(SignalingServer& server) {
    return server.rooms_;
  }

  static const std::unordered_map<std::string, SignalingServer::RoomRecord>& Rooms(const SignalingServer& server) {
    return server.rooms_;
  }
};

namespace {

std::int64_t NowUnixMillis() {
  const auto now = std::chrono::system_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

struct ConnectionLookup {
  SignalingServer::ConnectionRecord* connection{nullptr};
  SignalingServer::RoomRecord* room{nullptr};
  SignalingServer::PeerRecord* peer{nullptr};
};

util::json::Value::Array StringsToJson(const std::vector<std::string>& values) {
  util::json::Value::Array json_values;
  for (const auto& value : values) {
    json_values.emplace_back(value);
  }
  return json_values;
}

}  // namespace

namespace {

std::unordered_map<std::string, SignalingServer::ConnectionRecord>& ConnectionStore(SignalingServer& server) {
  return ServerAccess::Connections(server);
}

const std::unordered_map<std::string, SignalingServer::ConnectionRecord>& ConnectionStore(const SignalingServer& server) {
  return ServerAccess::Connections(server);
}

std::unordered_map<std::string, SignalingServer::RoomRecord>& RoomStore(SignalingServer& server) {
  return ServerAccess::Rooms(server);
}

const std::unordered_map<std::string, SignalingServer::RoomRecord>& RoomStore(const SignalingServer& server) {
  return ServerAccess::Rooms(server);
}

ConnectionLookup ResolveConnection(SignalingServer& server, const std::string& connection_id) {
  auto& connections = ConnectionStore(server);
  auto connection_it = connections.find(connection_id);
  if (connection_it == connections.end()) {
    return {};
  }

  ConnectionLookup lookup;
  lookup.connection = &connection_it->second;
  if (lookup.connection->room.empty() || lookup.connection->peer_id.empty()) {
    return lookup;
  }

  auto& rooms = RoomStore(server);
  auto room_it = rooms.find(lookup.connection->room);
  if (room_it == rooms.end()) {
    return lookup;
  }
  lookup.room = &room_it->second;
  auto peer_it = room_it->second.active_peers.find(lookup.connection->peer_id);
  if (peer_it != room_it->second.active_peers.end()) {
    lookup.peer = &peer_it->second;
  }
  return lookup;
}

SignalingServer::PeerRecord* FindPeer(SignalingServer::RoomRecord& room, const std::string& peer_id) {
  const auto it = room.active_peers.find(peer_id);
  if (it == room.active_peers.end()) {
    return nullptr;
  }
  return &it->second;
}

void Touch(SignalingServer::ConnectionRecord& connection) {
  connection.last_seen_ms = NowUnixMillis();
  connection.last_seen_at = core::UtcNowIso8601();
}

void Touch(SignalingServer::PeerRecord& peer) {
  peer.last_seen_ms = NowUnixMillis();
  peer.last_seen_at = core::UtcNowIso8601();
}

Message BuildErrorMessage(std::string room, std::string code, std::string description) {
  Message message;
  message.type = MessageType::kError;
  message.room = std::move(room);
  message.error = std::move(description);
  message.data = util::json::Value::Object{{"code", std::move(code)}};
  return message;
}

Message BuildPeerReadyMessage(const SignalingServer::PeerRecord& sender, const SignalingServer::PeerRecord& recipient) {
  Message message;
  message.type = MessageType::kPeerReady;
  message.room = sender.room;
  message.peer_id = sender.peer_id;
  message.target_peer_id = recipient.peer_id;
  message.session_id = sender.session_id;
  message.data = util::json::Value::Object{{"phase", ToString(sender.phase)},
                                           {"browser_client", sender.browser_client},
                                           {"reconnect_count", static_cast<int>(sender.reconnect_count)}};
  return message;
}

Message BuildPeerLeftMessage(const SignalingServer::PeerRecord& peer, std::string_view reason) {
  Message message;
  message.type = MessageType::kPeerLeft;
  message.room = peer.room;
  message.peer_id = peer.peer_id;
  message.session_id = peer.session_id;
  message.data = util::json::Value::Object{{"reason", std::string(reason)}};
  return message;
}

util::json::Value::Array BuildPeerSummaries(const SignalingServer::RoomRecord& room, std::string_view exclude_peer_id) {
  util::json::Value::Array peers;
  for (const auto& [peer_id, peer] : room.active_peers) {
    if (peer_id == exclude_peer_id) {
      continue;
    }
    peers.emplace_back(util::json::Value::Object{{"peer_id", peer.peer_id},
                                                 {"session_id", peer.session_id},
                                                 {"phase", ToString(peer.phase)},
                                                 {"browser_client", peer.browser_client}});
  }
  return peers;
}

Message BuildJoinAckMessage(const config::AppConfig& config,
                            const SignalingServer::RoomRecord& room,
                            const SignalingServer::PeerRecord& peer,
                            bool reconnected,
                            const core::Result<TurnCredentials>& turn_credentials) {
  Message message;
  message.type = MessageType::kJoinAck;
  message.room = room.room;
  message.peer_id = peer.peer_id;
  message.session_id = peer.session_id;

  util::json::Value::Object payload{{"reconnected", reconnected},
                                    {"phase", ToString(peer.phase)},
                                    {"stun_servers", StringsToJson(config.signaling.stun_servers)},
                                    {"peers", BuildPeerSummaries(room, peer.peer_id)},
                                    {"limits", util::json::Value::Object{{"max_room_size", config.signaling.max_room_size}}},
                                    {"endpoints", util::json::Value::Object{{"health", config.signaling.health_endpoint},
                                                                              {"rooms", config.signaling.debug_rooms_endpoint},
                                                                              {"turn", config.signaling.turn_credentials_endpoint}}}};
  if (turn_credentials.ok()) {
    payload.emplace("turn", TurnCredentialsToJson(turn_credentials.value()));
  }
  message.data = std::move(payload);
  return message;
}

PeerSessionSnapshot ToSnapshot(const SignalingServer::PeerRecord& peer) {
  PeerSessionSnapshot snapshot;
  snapshot.room = peer.room;
  snapshot.peer_id = peer.peer_id;
  snapshot.session_id = peer.session_id;
  snapshot.connection_id = peer.connection_id;
  snapshot.phase = peer.phase;
  snapshot.counterpart_peer_id = peer.counterpart_peer_id;
  snapshot.joined_at = peer.joined_at;
  snapshot.last_seen_at = peer.last_seen_at;
  snapshot.disconnected_at = peer.disconnected_at;
  snapshot.browser_client = peer.browser_client;
  snapshot.has_offer = !peer.last_offer_sdp.empty();
  snapshot.has_answer = !peer.last_answer_sdp.empty();
  snapshot.reconnect_count = peer.reconnect_count;
  snapshot.candidates = peer.candidates;
  return snapshot;
}

void MaybeEraseRoom(std::unordered_map<std::string, SignalingServer::RoomRecord>& rooms, std::string_view room_id) {
  const auto room_it = rooms.find(std::string(room_id));
  if (room_it == rooms.end()) {
    return;
  }
  if (room_it->second.active_peers.empty() && room_it->second.stale_peers.empty()) {
    rooms.erase(room_it);
  }
}

}  // namespace

std::string ToString(const PeerSessionPhase phase) {
  switch (phase) {
    case PeerSessionPhase::kAwaitingJoin:
      return "awaiting-join";
    case PeerSessionPhase::kWaitingForPeer:
      return "waiting-for-peer";
    case PeerSessionPhase::kOfferSent:
      return "offer-sent";
    case PeerSessionPhase::kOfferReceived:
      return "offer-received";
    case PeerSessionPhase::kAnswerSent:
      return "answer-sent";
    case PeerSessionPhase::kNegotiating:
      return "negotiating";
    case PeerSessionPhase::kConnected:
      return "connected";
    case PeerSessionPhase::kDisconnected:
      return "disconnected";
  }
  return "awaiting-join";
}

util::json::Value IceCandidateRecordToJson(const IceCandidateRecord& candidate) {
  return util::json::Value::Object{{"candidate", candidate.candidate},
                                   {"mid", candidate.mid},
                                   {"received_at", candidate.received_at}};
}

util::json::Value PeerSessionSnapshotToJson(const PeerSessionSnapshot& snapshot) {
  util::json::Value::Array candidates;
  for (const auto& candidate : snapshot.candidates) {
    candidates.emplace_back(IceCandidateRecordToJson(candidate));
  }
  return util::json::Value::Object{{"room", snapshot.room},
                                   {"peer_id", snapshot.peer_id},
                                   {"session_id", snapshot.session_id},
                                   {"connection_id", snapshot.connection_id},
                                   {"phase", ToString(snapshot.phase)},
                                   {"counterpart_peer_id", snapshot.counterpart_peer_id},
                                   {"joined_at", snapshot.joined_at},
                                   {"last_seen_at", snapshot.last_seen_at},
                                   {"disconnected_at", snapshot.disconnected_at},
                                   {"browser_client", snapshot.browser_client},
                                   {"has_offer", snapshot.has_offer},
                                   {"has_answer", snapshot.has_answer},
                                   {"reconnect_count", static_cast<int>(snapshot.reconnect_count)},
                                   {"candidate_count", static_cast<int>(snapshot.candidates.size())},
                                   {"candidates", candidates}};
}

util::json::Value RoomSnapshotToJson(const RoomSnapshot& snapshot) {
  util::json::Value::Array active_peers;
  for (const auto& peer : snapshot.active_peers) {
    active_peers.emplace_back(PeerSessionSnapshotToJson(peer));
  }
  util::json::Value::Array stale_peers;
  for (const auto& peer : snapshot.stale_peers) {
    stale_peers.emplace_back(PeerSessionSnapshotToJson(peer));
  }
  return util::json::Value::Object{{"room", snapshot.room},
                                   {"created_at", snapshot.created_at},
                                   {"max_room_size", static_cast<int>(snapshot.max_room_size)},
                                   {"active_peers", active_peers},
                                   {"stale_peers", stale_peers}};
}

util::json::Value HealthSnapshotToJson(const HealthSnapshot& snapshot) {
  return util::json::Value::Object{{"active_rooms", static_cast<int>(snapshot.active_rooms)},
                                   {"active_connections", static_cast<int>(snapshot.active_connections)},
                                   {"active_peers", static_cast<int>(snapshot.active_peers)},
                                   {"stale_peers", static_cast<int>(snapshot.stale_peers)},
                                   {"uwebsockets_runtime_available", snapshot.uwebsockets_runtime_available},
                                   {"turn_configured", snapshot.turn_configured},
                                   {"transport_status", snapshot.transport_status},
                                   {"health_endpoint", snapshot.health_endpoint},
                                   {"debug_rooms_endpoint", snapshot.debug_rooms_endpoint},
                                   {"turn_credentials_endpoint", snapshot.turn_credentials_endpoint},
                                   {"turn_credential_mode", snapshot.turn_credential_mode},
                                   {"reconnect_grace_ms", static_cast<int>(snapshot.reconnect_grace_ms)}};
}

util::json::Value TurnCredentialsToJson(const TurnCredentials& credentials) {
  return util::json::Value::Object{{"uri", credentials.uri},
                                   {"username", credentials.username},
                                   {"password", credentials.password},
                                   {"credential_mode", credentials.credential_mode},
                                   {"issued_at", credentials.issued_at},
                                   {"expires_at", credentials.expires_at}};
}

SignalingServer::SignalingServer(config::AppConfig config, core::Logger logger)
    : config_(std::move(config)), logger_(std::move(logger)) {}

void SignalingServer::OpenConnection(ConnectionContext context) {
  ConnectionRecord record;
  record.context = std::move(context);
  record.opened_at = core::UtcNowIso8601();
  record.last_seen_at = record.opened_at;
  record.last_seen_ms = NowUnixMillis();
  ConnectionStore(*this)[record.context.connection_id] = record;
  logger_.Info("Opened signaling connection " + record.context.connection_id);
}

DispatchResult SignalingServer::HandleMessage(const std::string& connection_id, std::string_view json_payload) {
  auto parsed = ParseMessage(json_payload);
  if (!parsed.ok()) {
    return Reject(connection_id, "", "parse-error", parsed.error().ToString());
  }

  auto lookup = ResolveConnection(*this, connection_id);
  if (lookup.connection == nullptr) {
    return Reject(connection_id, parsed.value().room, "unknown-connection", "Connection is not registered");
  }
  Touch(*lookup.connection);

  switch (parsed.value().type) {
    case MessageType::kJoin:
      return HandleJoin(*lookup.connection, parsed.value());
    case MessageType::kLeave:
      return HandleLeave(*lookup.connection, parsed.value());
    case MessageType::kOffer:
      return HandleOffer(*lookup.connection, parsed.value());
    case MessageType::kAnswer:
      return HandleAnswer(*lookup.connection, parsed.value());
    case MessageType::kIceCandidate:
      return HandleIceCandidate(*lookup.connection, parsed.value());
    case MessageType::kJoinAck:
    case MessageType::kPeerReady:
    case MessageType::kPeerLeft:
    case MessageType::kError:
      return Reject(connection_id, parsed.value().room, "invalid-direction", "Server-only signaling message type");
  }

  return Reject(connection_id, parsed.value().room, "unknown-message", "Unhandled signaling message type");
}

DispatchResult SignalingServer::HandleJoin(ConnectionRecord& connection, const Message& message) {
  if (message.room.empty()) {
    return Reject(connection.context.connection_id, "", "missing-room", "Join requires a room name");
  }
  if (!config_.signaling.allow_browser_signaling && connection.context.browser_client) {
    return Reject(connection.context.connection_id, message.room, "browser-signaling-disabled",
                  "Browser signaling is disabled for this deployment");
  }
  if (!connection.room.empty()) {
    return Reject(connection.context.connection_id, connection.room, "already-joined", "Connection already joined a room");
  }

  auto& rooms = RoomStore(*this);
  auto& room = rooms[message.room];
  if (room.room.empty()) {
    room.room = message.room;
    room.created_at = core::UtcNowIso8601();
    logger_.Info("Created signaling room " + room.room);
  }

  const std::string requested_peer_id = message.peer_id.empty() ? core::GenerateId("peer") : message.peer_id;
  if (room.active_peers.find(requested_peer_id) != room.active_peers.end()) {
    return Reject(connection.context.connection_id, message.room, "duplicate-peer-id", "Peer id is already active in this room");
  }

  const auto stale_it = room.stale_peers.find(requested_peer_id);
  const bool reconnected = stale_it != room.stale_peers.end();
  if (!reconnected && static_cast<int>(room.active_peers.size()) >= config_.signaling.max_room_size) {
    return Reject(connection.context.connection_id, message.room, "room-full", "Room is already at capacity");
  }

  PeerRecord peer;
  if (reconnected) {
    peer = stale_it->second;
    peer.reconnect_count += 1;
    room.stale_peers.erase(stale_it);
  } else {
    peer.room = message.room;
    peer.peer_id = requested_peer_id;
    peer.session_id = core::GenerateId("session");
    peer.joined_at = core::UtcNowIso8601();
  }

  peer.connection_id = connection.context.connection_id;
  peer.browser_client = connection.context.browser_client;
  peer.phase = room.active_peers.empty() ? PeerSessionPhase::kWaitingForPeer : PeerSessionPhase::kWaitingForPeer;
  peer.counterpart_peer_id.clear();
  peer.disconnected_at.clear();
  peer.disconnected_ms = 0;
  Touch(peer);
  room.active_peers[peer.peer_id] = peer;

  connection.room = room.room;
  connection.peer_id = peer.peer_id;

  auto turn_credentials = IssueTurnCredentials(room.room, peer.peer_id);

  DispatchResult result;
  result.outgoing.push_back(
      OutboundEnvelope{connection.context.connection_id, BuildJoinAckMessage(config_, room, room.active_peers.at(peer.peer_id),
                                                                            reconnected, turn_credentials)});

  for (auto& [other_id, other] : room.active_peers) {
    if (other_id == peer.peer_id) {
      continue;
    }
    other.counterpart_peer_id = peer.peer_id;
    room.active_peers.at(peer.peer_id).counterpart_peer_id = other.peer_id;
    result.outgoing.push_back(OutboundEnvelope{connection.context.connection_id,
                                               BuildPeerReadyMessage(other, room.active_peers.at(peer.peer_id))});
    result.outgoing.push_back(
        OutboundEnvelope{other.connection_id, BuildPeerReadyMessage(room.active_peers.at(peer.peer_id), other)});
  }

  logger_.Info("Peer " + peer.peer_id + " joined signaling room " + room.room);
  return result;
}

DispatchResult SignalingServer::HandleLeave(ConnectionRecord& connection, const Message& message) {
  static_cast<void>(message);
  if (connection.room.empty()) {
    return Reject(connection.context.connection_id, "", "not-joined", "Connection is not attached to a room");
  }

  DispatchResult result;
  result.outgoing = DetachConnection(connection, false);
  logger_.Info("Peer left signaling room via leave message: " + connection.context.connection_id);
  return result;
}

DispatchResult SignalingServer::HandleOffer(ConnectionRecord& connection, const Message& message) {
  auto lookup = ResolveConnection(*this, connection.context.connection_id);
  if (lookup.room == nullptr || lookup.peer == nullptr) {
    return Reject(connection.context.connection_id, message.room, "not-joined", "Join a room before sending offers");
  }
  if (message.target_peer_id.empty() || message.sdp.empty()) {
    return Reject(connection.context.connection_id, lookup.room->room, "invalid-offer",
                  "Offer requires target_peer_id and sdp");
  }
  if (message.target_peer_id == lookup.peer->peer_id) {
    return Reject(connection.context.connection_id, lookup.room->room, "invalid-target", "Peer cannot offer itself");
  }

  auto* target = FindPeer(*lookup.room, message.target_peer_id);
  if (target == nullptr) {
    return Reject(connection.context.connection_id, lookup.room->room, "peer-not-found", "Offer target is not active");
  }

  Touch(*lookup.peer);
  Touch(*target);
  lookup.peer->last_offer_sdp = message.sdp;
  lookup.peer->last_answer_sdp.clear();
  lookup.peer->candidates.clear();
  lookup.peer->phase = PeerSessionPhase::kOfferSent;
  lookup.peer->counterpart_peer_id = target->peer_id;

  target->last_offer_sdp = message.sdp;
  target->last_answer_sdp.clear();
  target->candidates.clear();
  target->phase = PeerSessionPhase::kOfferReceived;
  target->counterpart_peer_id = lookup.peer->peer_id;

  Message relay = message;
  relay.room = lookup.room->room;
  relay.peer_id = lookup.peer->peer_id;
  relay.session_id = lookup.peer->session_id;

  logger_.Info("Relayed offer from " + lookup.peer->peer_id + " to " + target->peer_id);
  return DispatchResult{true, {OutboundEnvelope{target->connection_id, relay}}};
}

DispatchResult SignalingServer::HandleAnswer(ConnectionRecord& connection, const Message& message) {
  auto lookup = ResolveConnection(*this, connection.context.connection_id);
  if (lookup.room == nullptr || lookup.peer == nullptr) {
    return Reject(connection.context.connection_id, message.room, "not-joined", "Join a room before sending answers");
  }
  if (message.target_peer_id.empty() || message.sdp.empty()) {
    return Reject(connection.context.connection_id, lookup.room->room, "invalid-answer",
                  "Answer requires target_peer_id and sdp");
  }

  auto* target = FindPeer(*lookup.room, message.target_peer_id);
  if (target == nullptr) {
    return Reject(connection.context.connection_id, lookup.room->room, "peer-not-found", "Answer target is not active");
  }
  if (lookup.peer->last_offer_sdp.empty() || target->last_offer_sdp.empty()) {
    return Reject(connection.context.connection_id, lookup.room->room, "answer-before-offer",
                  "Cannot answer before an offer has been recorded");
  }
  if (lookup.peer->counterpart_peer_id != target->peer_id && target->counterpart_peer_id != lookup.peer->peer_id) {
    return Reject(connection.context.connection_id, lookup.room->room, "invalid-answer-order",
                  "Answer target does not match the recorded offer pair");
  }

  Touch(*lookup.peer);
  Touch(*target);
  lookup.peer->last_answer_sdp = message.sdp;
  lookup.peer->phase = PeerSessionPhase::kNegotiating;
  lookup.peer->counterpart_peer_id = target->peer_id;

  target->last_answer_sdp = message.sdp;
  target->phase = PeerSessionPhase::kNegotiating;
  target->counterpart_peer_id = lookup.peer->peer_id;

  Message relay = message;
  relay.room = lookup.room->room;
  relay.peer_id = lookup.peer->peer_id;
  relay.session_id = lookup.peer->session_id;

  logger_.Info("Relayed answer from " + lookup.peer->peer_id + " to " + target->peer_id);
  return DispatchResult{true, {OutboundEnvelope{target->connection_id, relay}}};
}

DispatchResult SignalingServer::HandleIceCandidate(ConnectionRecord& connection, const Message& message) {
  auto lookup = ResolveConnection(*this, connection.context.connection_id);
  if (lookup.room == nullptr || lookup.peer == nullptr) {
    return Reject(connection.context.connection_id, message.room, "not-joined",
                  "Join a room before sending ICE candidates");
  }
  if (message.target_peer_id.empty() || message.candidate.empty() || message.mid.empty()) {
    return Reject(connection.context.connection_id, lookup.room->room, "invalid-candidate",
                  "ICE candidate requires target_peer_id, candidate, and mid");
  }

  auto* target = FindPeer(*lookup.room, message.target_peer_id);
  if (target == nullptr) {
    return Reject(connection.context.connection_id, lookup.room->room, "peer-not-found",
                  "ICE candidate target is not active");
  }
  if (lookup.peer->last_offer_sdp.empty() && target->last_offer_sdp.empty()) {
    return Reject(connection.context.connection_id, lookup.room->room, "candidate-before-offer",
                  "ICE candidates require an offer to exist first");
  }

  Touch(*lookup.peer);
  Touch(*target);
  lookup.peer->candidates.push_back(IceCandidateRecord{message.candidate, message.mid, core::UtcNowIso8601()});
  if (!lookup.peer->last_answer_sdp.empty() && !target->last_answer_sdp.empty()) {
    lookup.peer->phase = PeerSessionPhase::kConnected;
    target->phase = PeerSessionPhase::kConnected;
  } else if (lookup.peer->phase != PeerSessionPhase::kOfferSent &&
             lookup.peer->phase != PeerSessionPhase::kOfferReceived) {
    lookup.peer->phase = PeerSessionPhase::kNegotiating;
  }

  Message relay = message;
  relay.room = lookup.room->room;
  relay.peer_id = lookup.peer->peer_id;
  relay.session_id = lookup.peer->session_id;

  logger_.Info("Relayed ICE candidate from " + lookup.peer->peer_id + " to " + target->peer_id);
  return DispatchResult{true, {OutboundEnvelope{target->connection_id, relay}}};
}

DispatchResult SignalingServer::Reject(const std::string& connection_id,
                                       std::string room,
                                       std::string code,
                                       std::string description) const {
  logger_.Warn("Rejected signaling action for " + connection_id + ": " + description);
  return DispatchResult{false,
                        {OutboundEnvelope{connection_id,
                                          BuildErrorMessage(std::move(room), std::move(code), std::move(description))}}};
}

std::vector<OutboundEnvelope> SignalingServer::DetachConnection(ConnectionRecord& connection, bool keep_stale_session) {
  std::vector<OutboundEnvelope> outgoing;
  if (connection.room.empty() || connection.peer_id.empty()) {
    return outgoing;
  }

  auto& rooms = RoomStore(*this);
  auto room_it = rooms.find(connection.room);
  if (room_it == rooms.end()) {
    connection.room.clear();
    connection.peer_id.clear();
    return outgoing;
  }

  const auto peer_it = room_it->second.active_peers.find(connection.peer_id);
  if (peer_it == room_it->second.active_peers.end()) {
    connection.room.clear();
    connection.peer_id.clear();
    MaybeEraseRoom(rooms, room_it->first);
    return outgoing;
  }

  PeerRecord departing = peer_it->second;
  room_it->second.active_peers.erase(peer_it);
  departing.phase = PeerSessionPhase::kDisconnected;
  departing.counterpart_peer_id.clear();
  departing.disconnected_at = core::UtcNowIso8601();
  departing.disconnected_ms = NowUnixMillis();
  departing.connection_id.clear();
  if (keep_stale_session) {
    room_it->second.stale_peers[departing.peer_id] = departing;
  }

  for (auto& [other_id, other] : room_it->second.active_peers) {
    static_cast<void>(other_id);
    if (other.counterpart_peer_id == departing.peer_id) {
      other.counterpart_peer_id.clear();
      if (other.phase != PeerSessionPhase::kDisconnected) {
        other.phase = PeerSessionPhase::kWaitingForPeer;
      }
    }
    outgoing.push_back(OutboundEnvelope{other.connection_id,
                                        BuildPeerLeftMessage(departing, keep_stale_session ? "disconnect" : "leave")});
  }

  connection.room.clear();
  connection.peer_id.clear();
  MaybeEraseRoom(rooms, room_it->first);
  return outgoing;
}

std::vector<OutboundEnvelope> SignalingServer::CloseConnection(const std::string& connection_id) {
  auto& connections = ConnectionStore(*this);
  const auto connection_it = connections.find(connection_id);
  if (connection_it == connections.end()) {
    return {};
  }

  auto outgoing = DetachConnection(connection_it->second, true);
  logger_.Info("Closed signaling connection " + connection_id);
  connections.erase(connection_it);
  return outgoing;
}

std::size_t SignalingServer::PruneStaleSessions() { return PruneStaleSessions(NowUnixMillis()); }

std::size_t SignalingServer::PruneStaleSessions(const std::int64_t now_ms) {
  std::size_t removed = 0;
  auto& rooms = RoomStore(*this);
  std::vector<std::string> empty_rooms;
  for (auto& [room_id, room] : rooms) {
    for (auto it = room.stale_peers.begin(); it != room.stale_peers.end();) {
      if (now_ms - it->second.disconnected_ms >= reconnect_grace_ms()) {
        it = room.stale_peers.erase(it);
        ++removed;
      } else {
        ++it;
      }
    }
    if (room.active_peers.empty() && room.stale_peers.empty()) {
      empty_rooms.push_back(room_id);
    }
  }
  for (const auto& room_id : empty_rooms) {
    rooms.erase(room_id);
  }
  return removed;
}

HealthSnapshot SignalingServer::SnapshotHealth() const {
  HealthSnapshot snapshot;
  snapshot.uwebsockets_runtime_available = HasUWebSocketsRuntimeDependencies();
  snapshot.turn_configured = !config_.turn.uri.empty();
  snapshot.transport_status = snapshot.uwebsockets_runtime_available ? "uwebsockets-ready" : "uwebsockets-missing-usockets";
  snapshot.health_endpoint = config_.signaling.health_endpoint;
  snapshot.debug_rooms_endpoint = config_.signaling.debug_rooms_endpoint;
  snapshot.turn_credentials_endpoint = config_.signaling.turn_credentials_endpoint;
  snapshot.turn_credential_mode = config_.turn.credential_mode;
  snapshot.reconnect_grace_ms = config_.signaling.reconnect_grace_ms;

  snapshot.active_connections = ConnectionStore(*this).size();
  for (const auto& [room_id, room] : RoomStore(*this)) {
    static_cast<void>(room_id);
    if (!room.active_peers.empty()) {
      ++snapshot.active_rooms;
    }
    snapshot.active_peers += room.active_peers.size();
    snapshot.stale_peers += room.stale_peers.size();
  }
  return snapshot;
}

std::vector<RoomSnapshot> SignalingServer::SnapshotRooms() const {
  std::vector<RoomSnapshot> rooms;
  rooms.reserve(RoomStore(*this).size());
  for (const auto& [room_id, room] : RoomStore(*this)) {
    static_cast<void>(room_id);
    RoomSnapshot snapshot;
    snapshot.room = room.room;
    snapshot.created_at = room.created_at;
    snapshot.max_room_size = static_cast<std::size_t>(config_.signaling.max_room_size);
    for (const auto& [peer_id, peer] : room.active_peers) {
      static_cast<void>(peer_id);
      snapshot.active_peers.push_back(ToSnapshot(peer));
    }
    for (const auto& [peer_id, peer] : room.stale_peers) {
      static_cast<void>(peer_id);
      snapshot.stale_peers.push_back(ToSnapshot(peer));
    }
    rooms.push_back(std::move(snapshot));
  }
  return rooms;
}

util::json::Value SignalingServer::HealthToJson() const { return HealthSnapshotToJson(SnapshotHealth()); }

util::json::Value SignalingServer::DebugStateToJson() const {
  util::json::Value::Array rooms;
  for (const auto& snapshot : SnapshotRooms()) {
    rooms.emplace_back(RoomSnapshotToJson(snapshot));
  }
  return util::json::Value::Object{{"health", HealthToJson()}, {"rooms", rooms}};
}

core::Result<TurnCredentials> SignalingServer::IssueTurnCredentials(std::string_view room, std::string_view peer_id) const {
  static_cast<void>(room);
  static_cast<void>(peer_id);
  if (config_.turn.uri.empty()) {
    return core::Error{core::ErrorCode::kUnavailable, "TURN is not configured"};
  }
  if (config_.turn.credential_mode != "static") {
    return core::Error{core::ErrorCode::kUnavailable,
                       "Only static TURN credentials are supported in Tier 2 scaffolding"};
  }

  TurnCredentials credentials;
  credentials.uri = config_.turn.uri;
  credentials.username = config_.turn.username;
  credentials.password = config_.turn.password;
  credentials.credential_mode = config_.turn.credential_mode;
  credentials.issued_at = core::UtcNowIso8601();
  credentials.expires_at = core::UtcNowIso8601();
  return credentials;
}

bool SignalingServer::HasUWebSocketsRuntimeDependencies() const {
  return std::filesystem::exists(std::string(DAFFY_SOURCE_DIR) + "/third_party/uWebSockets/src/App.h") &&
         std::filesystem::exists(std::string(DAFFY_SOURCE_DIR) + "/third_party/uWebSockets/uSockets/src/libusockets.h");
}

std::int64_t SignalingServer::reconnect_grace_ms() const { return config_.signaling.reconnect_grace_ms; }

}  // namespace daffy::signaling
