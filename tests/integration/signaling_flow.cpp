#include <cassert>
#include <sstream>
#include <string>

#include "daffy/config/app_config.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/signaling/server.hpp"

namespace {

std::string FindMessageFor(const daffy::signaling::DispatchResult& result,
                           const std::string& connection_id,
                           daffy::signaling::MessageType type) {
  for (const auto& outbound : result.outgoing) {
    if (outbound.connection_id == connection_id && outbound.message.type == type) {
      return daffy::signaling::SerializeMessage(outbound.message);
    }
  }
  return {};
}

bool ContainsMessageFor(const std::vector<daffy::signaling::OutboundEnvelope>& outgoing,
                        const std::string& connection_id,
                        daffy::signaling::MessageType type) {
  for (const auto& outbound : outgoing) {
    if (outbound.connection_id == connection_id && outbound.message.type == type) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  auto config = daffy::config::DefaultAppConfig();
  config.signaling.allow_browser_signaling = false;
  config.signaling.reconnect_grace_ms = 5000;

  std::ostringstream logs;
  auto logger = daffy::core::CreateOstreamLogger("signaling-flow", daffy::core::LogLevel::kInfo, logs);
  daffy::signaling::SignalingServer server(config, logger);

  server.OpenConnection({"conn-a", "127.0.0.1", "native-a", false});
  server.OpenConnection({"conn-b", "127.0.0.2", "native-b", false});

  auto first_join = server.HandleMessage("conn-a", R"({"type":"join","room":"alpha","peer_id":"peer-a"})");
  assert(first_join.accepted);
  auto first_join_payload = FindMessageFor(first_join, "conn-a", daffy::signaling::MessageType::kJoinAck);
  assert(!first_join_payload.empty());
  assert(first_join_payload.find("join-ack") != std::string::npos);
  assert(first_join_payload.find("stun:stun.l.google.com") != std::string::npos);

  auto second_join = server.HandleMessage("conn-b", R"({"type":"join","room":"alpha","peer_id":"peer-b"})");
  assert(second_join.accepted);
  assert(!FindMessageFor(second_join, "conn-b", daffy::signaling::MessageType::kJoinAck).empty());
  assert(!FindMessageFor(second_join, "conn-b", daffy::signaling::MessageType::kPeerReady).empty());
  assert(!FindMessageFor(second_join, "conn-a", daffy::signaling::MessageType::kPeerReady).empty());

  auto offer = server.HandleMessage(
      "conn-a",
      R"({"type":"offer","room":"alpha","target_peer_id":"peer-b","sdp":"v=0\r\no=- 0 0 IN IP4 127.0.0.1"})");
  assert(offer.accepted);
  const auto relayed_offer = FindMessageFor(offer, "conn-b", daffy::signaling::MessageType::kOffer);
  assert(!relayed_offer.empty());
  assert(relayed_offer.find("peer-a") != std::string::npos);

  auto answer = server.HandleMessage(
      "conn-b",
      R"({"type":"answer","room":"alpha","target_peer_id":"peer-a","sdp":"v=0\r\no=- 1 1 IN IP4 127.0.0.2"})");
  assert(answer.accepted);
  const auto relayed_answer = FindMessageFor(answer, "conn-a", daffy::signaling::MessageType::kAnswer);
  assert(!relayed_answer.empty());
  assert(relayed_answer.find("peer-b") != std::string::npos);

  auto candidate = server.HandleMessage(
      "conn-a",
      R"({"type":"ice-candidate","room":"alpha","target_peer_id":"peer-b","candidate":"candidate:1 1 UDP 2122252543 192.0.2.1 54400 typ host","mid":"audio"})");
  assert(candidate.accepted);
  const auto relayed_candidate = FindMessageFor(candidate, "conn-b", daffy::signaling::MessageType::kIceCandidate);
  assert(!relayed_candidate.empty());
  assert(relayed_candidate.find("candidate:1 1 UDP") != std::string::npos);

  const auto health = server.SnapshotHealth();
  assert(health.active_rooms == 1);
  assert(health.active_peers == 2);
  assert(health.turn_credentials_endpoint == "/debug/turn-credentials");

  const auto rooms = server.SnapshotRooms();
  assert(rooms.size() == 1);
  assert(rooms.front().active_peers.size() == 2);
  bool saw_connected = false;
  for (const auto& peer : rooms.front().active_peers) {
    if (peer.phase == daffy::signaling::PeerSessionPhase::kConnected) {
      saw_connected = true;
    }
  }
  assert(saw_connected);

  server.OpenConnection({"conn-browser", "127.0.0.3", "browser", true});
  auto browser_join = server.HandleMessage("conn-browser", R"({"type":"join","room":"alpha"})");
  assert(!browser_join.accepted);
  assert(!FindMessageFor(browser_join, "conn-browser", daffy::signaling::MessageType::kError).empty());

  daffy::signaling::SignalingServer ordering_server(daffy::config::DefaultAppConfig(), logger);
  ordering_server.OpenConnection({"order-a", "127.0.0.4", "native-order-a", false});
  ordering_server.OpenConnection({"order-b", "127.0.0.5", "native-order-b", false});
  assert(ordering_server.HandleMessage("order-a", R"({"type":"join","room":"beta","peer_id":"beta-a"})")
             .accepted);
  assert(ordering_server.HandleMessage("order-b", R"({"type":"join","room":"beta","peer_id":"beta-b"})")
             .accepted);
  auto answer_before_offer = ordering_server.HandleMessage(
      "order-b",
      R"({"type":"answer","room":"beta","target_peer_id":"beta-a","sdp":"v=0\r\no=- 2 2 IN IP4 127.0.0.3"})");
  assert(!answer_before_offer.accepted);
  assert(!FindMessageFor(answer_before_offer, "order-b", daffy::signaling::MessageType::kError).empty());

  auto disconnect_messages = server.CloseConnection("conn-b");
  assert(ContainsMessageFor(disconnect_messages, "conn-a", daffy::signaling::MessageType::kPeerLeft));
  const auto post_disconnect = server.SnapshotHealth();
  assert(post_disconnect.active_peers == 1);
  assert(post_disconnect.stale_peers == 1);

  server.OpenConnection({"conn-b-reconnect", "127.0.0.2", "native-b", false});
  auto rejoin = server.HandleMessage("conn-b-reconnect", R"({"type":"join","room":"alpha","peer_id":"peer-b"})");
  assert(rejoin.accepted);
  const auto rejoin_payload = FindMessageFor(rejoin, "conn-b-reconnect", daffy::signaling::MessageType::kJoinAck);
  assert(rejoin_payload.find("\"reconnected\":true") != std::string::npos);

  auto stale_disconnect = server.CloseConnection("conn-b-reconnect");
  assert(ContainsMessageFor(stale_disconnect, "conn-a", daffy::signaling::MessageType::kPeerLeft));
  const auto pruned = server.PruneStaleSessions(9'999'999'999'999LL);
  assert(pruned >= 1);

  auto turn_credentials = server.IssueTurnCredentials("alpha", "peer-a");
  assert(turn_credentials.ok());
  assert(turn_credentials.value().username == "daffy");

  assert(logs.str().find("Relayed answer") != std::string::npos);
  return 0;
}
