#pragma once

#include <string>

#include "daffy/core/error.hpp"
#include "daffy/util/json.hpp"

namespace daffy::signaling {

enum class MessageType {
  kJoin,
  kLeave,
  kOffer,
  kAnswer,
  kIceCandidate,
  kJoinAck,
  kPeerReady,
  kPeerLeft,
  kError
};

struct Message {
  MessageType type{MessageType::kJoin};
  std::string room;
  std::string peer_id;
  std::string target_peer_id;
  std::string session_id;
  std::string sdp;
  std::string candidate;
  std::string mid;
  std::string error;
  util::json::Value data;
};

std::string ToString(MessageType type);
core::Result<MessageType> ParseMessageType(std::string_view type);
util::json::Value MessageToJson(const Message& message);
std::string SerializeMessage(const Message& message);
core::Result<Message> ParseMessage(std::string_view json_text);

}  // namespace daffy::signaling
