#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/ipc/nng_transport.hpp"
#include "daffy/rooms/room_registry.hpp"
#include "daffy/runtime/event_bus.hpp"
#include "daffy/services/service_metadata.hpp"

namespace daffy::services {

/// Bot registration and authentication record.
struct BotRecord {
  std::string bot_id;
  std::string display_name;
  bool enabled{true};
  std::string created_at;
  std::string updated_at;
  std::vector<std::string> capabilities;
  std::vector<std::string> room_scope;
  std::unordered_map<std::string, std::string> metadata;
};

/// Bot authentication token.
struct BotToken {
  std::string token_id;
  std::string bearer;
};

/// Bot room session state.
struct BotSession {
  std::string session_id;
  std::string bot_id;
  std::string room_id;
  std::string joined_at;
  std::string last_seen_at;
  std::string state;  // "joining", "active", "suspended", "left"
};

/// Bot event cursor for replay.
struct BotEventCursor {
  std::string bot_id;
  std::string room_id;
  std::size_t last_sequence{0};
  std::string updated_at;
};

/// Bot command record.
struct BotCommand {
  std::string command_id;
  std::string room_id;
  std::string bot_id;
  std::string name;
  std::string args;
  std::string issued_by;
  std::string issued_at;
};

/// Built-in Bot API service for automated agent integration.
class BotApiService {
 public:
  explicit BotApiService(
      core::Logger logger = core::CreateConsoleLogger("bot-api-service", core::LogLevel::kInfo));

  static ServiceMetadata Metadata();

  core::Result<ipc::MessageEnvelope> Handle(const ipc::MessageEnvelope& request);
  core::Status Bind(ipc::NngRequestReplyTransport& transport, std::string url);

 private:
  struct SequencedEvent {
    std::size_t sequence{0};
    runtime::EventEnvelope event;
  };

  // RPC handlers
  core::Result<util::json::Value> HandleRegisterBot(const util::json::Value& payload);
  core::Result<util::json::Value> HandleGetBot(const util::json::Value& payload);
  core::Result<util::json::Value> HandleListBots(const util::json::Value& payload);
  core::Result<util::json::Value> HandleJoinRoom(const util::json::Value& payload);
  core::Result<util::json::Value> HandleLeaveRoom(const util::json::Value& payload);
  core::Result<util::json::Value> HandlePostMessage(const util::json::Value& payload);
  core::Result<util::json::Value> HandlePollEvents(const util::json::Value& payload);
  core::Result<util::json::Value> HandleHandleCommand(const util::json::Value& payload);
  core::Result<util::json::Value> HandleModerateParticipant(const util::json::Value& payload);
  core::Result<util::json::Value> HandleStatus(const util::json::Value& payload);

  // Authentication and authorization
  core::Result<BotRecord> AuthenticateToken(const std::string& token) const;
  core::Status CheckCapability(const BotRecord& bot, const std::string& capability) const;
  core::Status CheckRoomScope(const BotRecord& bot, const std::string& room_id) const;

  // Utility methods
  std::string GenerateBotId();
  std::string GenerateTokenId();
  std::string GenerateBearerToken();
  std::string GenerateSessionId();
  std::string GenerateCommandId();
  std::string GetCurrentTimestamp() const;
  util::json::Value BotRecordToJson(const BotRecord& bot) const;
  util::json::Value BotSessionToJson(const BotSession& session) const;
  util::json::Value BotCommandToJson(const BotCommand& command) const;

  runtime::InMemoryEventBus event_bus_;
  rooms::RoomRegistry room_registry_;

  // Bot storage
  std::unordered_map<std::string, BotRecord> bots_;           // bot_id -> BotRecord
  std::unordered_map<std::string, std::string> tokens_;       // bearer_token -> bot_id
  std::unordered_map<std::string, BotSession> sessions_;      // session_id -> BotSession
  std::unordered_map<std::string, BotEventCursor> cursors_;   // bot_id:room_id -> cursor
  std::vector<SequencedEvent> events_;
  std::vector<BotCommand> commands_;

  // Counters
  std::size_t next_bot_id_{1};
  std::size_t next_token_id_{1};
  std::size_t next_session_id_{1};
  std::size_t next_command_id_{1};
  std::size_t next_sequence_{1};

  runtime::EventBus::SubscriptionId event_subscription_id_{0};
};

}  // namespace daffy::services
