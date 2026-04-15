#include "daffy/signaling/messages.hpp"

namespace daffy::signaling {
namespace {

core::Result<std::string> RequireString(const util::json::Value& object, std::string_view field) {
  const auto* value = object.Find(field);
  if (value == nullptr || !value->IsString()) {
    return core::Error{core::ErrorCode::kParseError, "Missing or invalid signaling field: " + std::string(field)};
  }
  return value->AsString();
}

void ReadOptionalString(const util::json::Value& object, std::string_view field, std::string& output) {
  if (const auto* value = object.Find(field); value != nullptr && value->IsString()) {
    output = value->AsString();
  }
}

}  // namespace

std::string ToString(const MessageType type) {
  switch (type) {
    case MessageType::kJoin:
      return "join";
    case MessageType::kLeave:
      return "leave";
    case MessageType::kOffer:
      return "offer";
    case MessageType::kAnswer:
      return "answer";
    case MessageType::kIceCandidate:
      return "ice-candidate";
    case MessageType::kJoinAck:
      return "join-ack";
    case MessageType::kPeerReady:
      return "peer-ready";
    case MessageType::kPeerLeft:
      return "peer-left";
    case MessageType::kError:
      return "error";
  }
  return "error";
}

core::Result<MessageType> ParseMessageType(std::string_view type) {
  if (type == "join") {
    return MessageType::kJoin;
  }
  if (type == "leave") {
    return MessageType::kLeave;
  }
  if (type == "offer") {
    return MessageType::kOffer;
  }
  if (type == "answer") {
    return MessageType::kAnswer;
  }
  if (type == "ice-candidate") {
    return MessageType::kIceCandidate;
  }
  if (type == "join-ack") {
    return MessageType::kJoinAck;
  }
  if (type == "peer-ready") {
    return MessageType::kPeerReady;
  }
  if (type == "peer-left") {
    return MessageType::kPeerLeft;
  }
  if (type == "error") {
    return MessageType::kError;
  }
  return core::Error{core::ErrorCode::kParseError, "Unknown signaling message type"};
}

util::json::Value MessageToJson(const Message& message) {
  util::json::Value::Object object{{"type", ToString(message.type)}};
  if (!message.room.empty()) {
    object.emplace("room", message.room);
  }
  if (!message.peer_id.empty()) {
    object.emplace("peer_id", message.peer_id);
  }
  if (!message.target_peer_id.empty()) {
    object.emplace("target_peer_id", message.target_peer_id);
  }
  if (!message.session_id.empty()) {
    object.emplace("session_id", message.session_id);
  }
  if (!message.sdp.empty()) {
    object.emplace("sdp", message.sdp);
  }
  if (!message.candidate.empty()) {
    object.emplace("candidate", message.candidate);
  }
  if (!message.mid.empty()) {
    object.emplace("mid", message.mid);
  }
  if (!message.error.empty()) {
    object.emplace("error", message.error);
  }
  if (!message.data.IsNull()) {
    object.emplace("data", message.data);
  }
  return object;
}

std::string SerializeMessage(const Message& message) { return util::json::Serialize(MessageToJson(message)); }

core::Result<Message> ParseMessage(std::string_view json_text) {
  auto parsed = util::json::Parse(json_text);
  if (!parsed.ok()) {
    return parsed.error();
  }
  if (!parsed.value().IsObject()) {
    return core::Error{core::ErrorCode::kParseError, "Signaling message must be a JSON object"};
  }

  auto type_field = RequireString(parsed.value(), "type");
  if (!type_field.ok()) {
    return type_field.error();
  }
  auto type = ParseMessageType(type_field.value());
  if (!type.ok()) {
    return type.error();
  }

  Message message;
  message.type = type.value();
  ReadOptionalString(parsed.value(), "room", message.room);
  ReadOptionalString(parsed.value(), "peer_id", message.peer_id);
  ReadOptionalString(parsed.value(), "target_peer_id", message.target_peer_id);
  ReadOptionalString(parsed.value(), "session_id", message.session_id);
  ReadOptionalString(parsed.value(), "sdp", message.sdp);
  ReadOptionalString(parsed.value(), "candidate", message.candidate);
  ReadOptionalString(parsed.value(), "mid", message.mid);
  ReadOptionalString(parsed.value(), "error", message.error);
  if (const auto* data = parsed.value().Find("data"); data != nullptr) {
    message.data = *data;
  }
  return message;
}

}  // namespace daffy::signaling
