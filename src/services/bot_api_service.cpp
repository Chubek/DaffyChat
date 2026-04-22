#include "daffy/services/bot_api_service.hpp"

#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>

namespace daffy::services {

namespace {

constexpr char kServiceTopic[] = "service.botapi";
constexpr char kRequestType[] = "request";
constexpr char kReplyType[] = "reply";

// Supported capabilities
constexpr char kCapRoomsRead[] = "rooms.read";
constexpr char kCapRoomsJoin[] = "rooms.join";
constexpr char kCapEventsRead[] = "events.read";
constexpr char kCapMessagesWrite[] = "messages.write";
constexpr char kCapCommandsHandle[] = "commands.handle";
constexpr char kCapParticipantsRead[] = "participants.read";
constexpr char kCapParticipantsKick[] = "participants.kick";
constexpr char kCapParticipantsMute[] = "participants.mute";
constexpr char kCapWebhooksWrite[] = "webhooks.write";

}  // namespace

BotApiService::BotApiService(core::Logger logger)
    : room_registry_(std::move(logger), event_bus_) {
  // Subscribe to all room events for bot consumption
  event_subscription_id_ = event_bus_.Subscribe("room.*", [this](const runtime::EventEnvelope& event) {
    events_.push_back(SequencedEvent{next_sequence_++, event});
  });
}

ServiceMetadata BotApiService::Metadata() {
  return ServiceMetadata{"botapi",
                         "1.0.0",
                         "Built-in Bot API service for automated agent integration.",
                         "daffy-botapi-service",
                         {"nng:reqrep", "bot-auth", "bot-events", "bot-commands"},
                         true};
}

std::string BotApiService::GenerateBotId() {
  return "bot-" + std::to_string(next_bot_id_++);
}

std::string BotApiService::GenerateTokenId() {
  return "token-" + std::to_string(next_token_id_++);
}

std::string BotApiService::GenerateBearerToken() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);
  
  std::stringstream ss;
  ss << "dcb_";
  for (int i = 0; i < 32; ++i) {
    ss << std::hex << dis(gen);
  }
  return ss.str();
}

std::string BotApiService::GenerateSessionId() {
  return "session-" + std::to_string(next_session_id_++);
}

std::string BotApiService::GenerateCommandId() {
  return "cmd-" + std::to_string(next_command_id_++);
}

std::string BotApiService::GetCurrentTimestamp() const {
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
  return ss.str();
}

util::json::Value BotApiService::BotRecordToJson(const BotRecord& bot) const {
  util::json::Value::Array caps;
  for (const auto& cap : bot.capabilities) {
    caps.emplace_back(cap);
  }
  
  util::json::Value::Array scope;
  for (const auto& room : bot.room_scope) {
    scope.emplace_back(room);
  }
  
  return util::json::Value(util::json::Value::Object{
      {"bot_id", bot.bot_id},
      {"display_name", bot.display_name},
      {"enabled", bot.enabled},
      {"created_at", bot.created_at},
      {"updated_at", bot.updated_at},
      {"capabilities", caps},
      {"room_scope", scope}});
}

util::json::Value BotApiService::BotSessionToJson(const BotSession& session) const {
  return util::json::Value(util::json::Value::Object{
      {"session_id", session.session_id},
      {"bot_id", session.bot_id},
      {"room_id", session.room_id},
      {"joined_at", session.joined_at},
      {"last_seen_at", session.last_seen_at},
      {"state", session.state}});
}

util::json::Value BotApiService::BotCommandToJson(const BotCommand& command) const {
  return util::json::Value(util::json::Value::Object{
      {"command_id", command.command_id},
      {"room_id", command.room_id},
      {"bot_id", command.bot_id},
      {"name", command.name},
      {"args", command.args},
      {"issued_by", command.issued_by},
      {"issued_at", command.issued_at}});
}

core::Result<BotRecord> BotApiService::AuthenticateToken(const std::string& token) const {
  auto it = tokens_.find(token);
  if (it == tokens_.end()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Invalid or expired bot token"};
  }
  
  const auto& bot_id = it->second;
  auto bot_it = bots_.find(bot_id);
  if (bot_it == bots_.end()) {
    return core::Error{core::ErrorCode::kNotFound, "Bot not found"};
  }
  
  const auto& bot = bot_it->second;
  if (!bot.enabled) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Bot is disabled"};
  }
  
  return bot;
}

core::Status BotApiService::CheckCapability(const BotRecord& bot, const std::string& capability) const {
  for (const auto& cap : bot.capabilities) {
    if (cap == capability) {
      return std::monostate{};
    }
  }
  return core::Error{core::ErrorCode::kInvalidArgument, "Bot lacks required capability: " + capability};
}

core::Status BotApiService::CheckRoomScope(const BotRecord& bot, const std::string& room_id) const {
  if (bot.room_scope.empty()) {
    return std::monostate{};  // No scope restriction
  }
  
  for (const auto& room : bot.room_scope) {
    if (room == room_id) {
      return std::monostate{};
    }
  }
  return core::Error{core::ErrorCode::kInvalidArgument, "Bot not authorized for room: " + room_id};
}

core::Result<util::json::Value> BotApiService::HandleRegisterBot(const util::json::Value& payload) {
  const auto* display_name = payload.Find("display_name");
  if (display_name == nullptr || !display_name->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "display_name is required and must be a string"};
  }
  
  BotRecord bot;
  bot.bot_id = GenerateBotId();
  bot.display_name = display_name->AsString();
  bot.enabled = true;
  bot.created_at = GetCurrentTimestamp();
  bot.updated_at = bot.created_at;
  
  // Parse capabilities
  const auto* caps = payload.Find("capabilities");
  if (caps != nullptr && caps->IsArray()) {
    for (const auto& cap : caps->AsArray()) {
      if (cap.IsString()) {
        bot.capabilities.push_back(cap.AsString());
      }
    }
  }
  
  // Parse room scope
  const auto* scope = payload.Find("room_scope");
  if (scope != nullptr && scope->IsArray()) {
    for (const auto& room : scope->AsArray()) {
      if (room.IsString()) {
        bot.room_scope.push_back(room.AsString());
      }
    }
  }
  
  // Generate token
  BotToken token;
  token.token_id = GenerateTokenId();
  token.bearer = GenerateBearerToken();
  
  // Store bot and token
  bots_[bot.bot_id] = bot;
  tokens_[token.bearer] = bot.bot_id;
  
  
  return util::json::Value(util::json::Value::Object{
      {"bot", BotRecordToJson(bot)},
      {"token", util::json::Value(util::json::Value::Object{{"token_id", token.token_id}, {"bearer", token.bearer}})}});
}

core::Result<util::json::Value> BotApiService::HandleGetBot(const util::json::Value& payload) {
  const auto* bot_id = payload.Find("bot_id");
  if (bot_id == nullptr || !bot_id->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "bot_id is required and must be a string"};
  }
  
  auto it = bots_.find(bot_id->AsString());
  if (it == bots_.end()) {
    return core::Error{core::ErrorCode::kNotFound, "Bot not found: " + bot_id->AsString()};
  }
  
  return BotRecordToJson(it->second);
}

core::Result<util::json::Value> BotApiService::HandleListBots(const util::json::Value& payload) {
  util::json::Value::Array result;
  
  const auto* enabled_filter = payload.Find("enabled");
  const auto* room_filter = payload.Find("room_id");
  const auto* cap_filter = payload.Find("capability");
  
  for (const auto& [bot_id, bot] : bots_) {
    // Apply filters
    if (enabled_filter != nullptr && enabled_filter->IsBool() && bot.enabled != enabled_filter->AsBool()) {
      continue;
    }
    
    if (room_filter != nullptr && room_filter->IsString()) {
      bool in_scope = bot.room_scope.empty();
      for (const auto& room : bot.room_scope) {
        if (room == room_filter->AsString()) {
          in_scope = true;
          break;
        }
      }
      if (!in_scope) continue;
    }
    
    if (cap_filter != nullptr && cap_filter->IsString()) {
      bool has_cap = false;
      for (const auto& cap : bot.capabilities) {
        if (cap == cap_filter->AsString()) {
          has_cap = true;
          break;
        }
      }
      if (!has_cap) continue;
    }
    
    result.emplace_back(BotRecordToJson(bot));
  }
  
  return util::json::Value(result);
}

core::Result<util::json::Value> BotApiService::HandleJoinRoom(const util::json::Value& payload) {
  const auto* token = payload.Find("token");
  const auto* room_id = payload.Find("room_id");
  
  if (token == nullptr || !token->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "token is required and must be a string"};
  }
  if (room_id == nullptr || !room_id->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "room_id is required and must be a string"};
  }
  
  // Authenticate
  auto bot_result = AuthenticateToken(token->AsString());
  if (!bot_result.ok()) {
    return bot_result.error();
  }
  const auto& bot = bot_result.value();
  
  // Check capability
  auto cap_check = CheckCapability(bot, kCapRoomsJoin);
  if (!cap_check.ok()) {
    return cap_check.error();
  }
  
  // Check room scope
  auto scope_check = CheckRoomScope(bot, room_id->AsString());
  if (!scope_check.ok()) {
    return scope_check.error();
  }
  
  // Create session
  BotSession session;
  session.session_id = GenerateSessionId();
  session.bot_id = bot.bot_id;
  session.room_id = room_id->AsString();
  session.joined_at = GetCurrentTimestamp();
  session.last_seen_at = session.joined_at;
  session.state = "active";
  
  sessions_[session.session_id] = session;
  
  // Initialize event cursor
  std::string cursor_key = bot.bot_id + ":" + room_id->AsString();
  cursors_[cursor_key] = BotEventCursor{bot.bot_id, room_id->AsString(), 0, GetCurrentTimestamp()};
  
  
  return util::json::Value(util::json::Value::Object{
      {"session", BotSessionToJson(session)},
      {"participant", util::json::Value(util::json::Value::Object{
          {"participant_id", bot.bot_id},
          {"display_name", bot.display_name},
          {"role", "bot"}})}});
}

core::Result<util::json::Value> BotApiService::HandleLeaveRoom(const util::json::Value& payload) {
  const auto* token = payload.Find("token");
  const auto* room_id = payload.Find("room_id");
  
  if (token == nullptr || !token->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "token is required and must be a string"};
  }
  if (room_id == nullptr || !room_id->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "room_id is required and must be a string"};
  }
  
  // Authenticate
  auto bot_result = AuthenticateToken(token->AsString());
  if (!bot_result.ok()) {
    return bot_result.error();
  }
  const auto& bot = bot_result.value();
  
  // Find and remove session
  bool found = false;
  for (auto it = sessions_.begin(); it != sessions_.end();) {
    if (it->second.bot_id == bot.bot_id && it->second.room_id == room_id->AsString()) {
      it = sessions_.erase(it);
      found = true;
    } else {
      ++it;
    }
  }
  
  
  return util::json::Value(found);
}

core::Result<util::json::Value> BotApiService::HandlePostMessage(const util::json::Value& payload) {
  const auto* token = payload.Find("token");
  const auto* room_id = payload.Find("room_id");
  const auto* text = payload.Find("text");
  
  if (token == nullptr || !token->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "token is required and must be a string"};
  }
  if (room_id == nullptr || !room_id->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "room_id is required and must be a string"};
  }
  if (text == nullptr || !text->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "text is required and must be a string"};
  }
  
  // Authenticate
  auto bot_result = AuthenticateToken(token->AsString());
  if (!bot_result.ok()) {
    return bot_result.error();
  }
  const auto& bot = bot_result.value();
  
  // Check capability
  auto cap_check = CheckCapability(bot, kCapMessagesWrite);
  if (!cap_check.ok()) {
    return cap_check.error();
  }
  
  // Check room scope
  auto scope_check = CheckRoomScope(bot, room_id->AsString());
  if (!scope_check.ok()) {
    return scope_check.error();
  }
  
  // Generate message ID
  std::string message_id = "msg-" + std::to_string(next_sequence_);
  
  // Emit message event
  runtime::EventEnvelope event;
  event.topic = "room.message";
  event.emitted_at = GetCurrentTimestamp();
  event.payload = util::json::Value::Object{
      {"message_id", message_id},
      {"room_id", room_id->AsString()},
      {"sender_id", bot.bot_id},
      {"sender_name", bot.display_name},
      {"text", text->AsString()},
      {"sender_type", "bot"}};
  
  event_bus_.Publish(event);
  
  
  return util::json::Value(util::json::Value::Object{
      {"message_id", message_id},
      {"accepted", true},
      {"emitted_event_sequence", static_cast<double>(next_sequence_ - 1)}});
}

core::Result<util::json::Value> BotApiService::HandlePollEvents(const util::json::Value& payload) {
  const auto* token = payload.Find("token");
  const auto* room_id = payload.Find("room_id");
  
  if (token == nullptr || !token->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "token is required and must be a string"};
  }
  if (room_id == nullptr || !room_id->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "room_id is required and must be a string"};
  }
  
  // Authenticate
  auto bot_result = AuthenticateToken(token->AsString());
  if (!bot_result.ok()) {
    return bot_result.error();
  }
  const auto& bot = bot_result.value();
  
  // Check capability
  auto cap_check = CheckCapability(bot, kCapEventsRead);
  if (!cap_check.ok()) {
    return cap_check.error();
  }
  
  // Check room scope
  auto scope_check = CheckRoomScope(bot, room_id->AsString());
  if (!scope_check.ok()) {
    return scope_check.error();
  }
  
  // Get cursor position
  std::size_t after_sequence = 0;
  const auto* after_seq = payload.Find("after_sequence");
  if (after_seq != nullptr && after_seq->IsNumber()) {
    after_sequence = static_cast<std::size_t>(after_seq->AsNumber());
  }
  
  // Get limit
  std::size_t limit = 100;
  const auto* limit_val = payload.Find("limit");
  if (limit_val != nullptr && limit_val->IsNumber()) {
    limit = static_cast<std::size_t>(limit_val->AsNumber());
    if (limit > 1000) limit = 1000;  // Cap at 1000
  }
  
  // Collect events
  util::json::Value::Array result;
  std::size_t count = 0;
  std::size_t next_seq = after_sequence;
  
  for (const auto& entry : events_) {
    if (entry.sequence <= after_sequence) {
      continue;
    }
    if (count >= limit) {
      break;
    }
    
    // Filter by room if event has room_id
    const auto* event_room = entry.event.payload.Find("room_id");
    if (event_room != nullptr && event_room->IsString() && event_room->AsString() != room_id->AsString()) {
      continue;
    }
    
    result.emplace_back(util::json::Value::Object{
        {"sequence", static_cast<double>(entry.sequence)},
        {"topic", entry.event.topic},
        {"timestamp", entry.event.emitted_at},
        {"payload", entry.event.payload}});
    
    next_seq = entry.sequence;
    ++count;
  }
  
  // Update cursor
  std::string cursor_key = bot.bot_id + ":" + room_id->AsString();
  if (cursors_.find(cursor_key) != cursors_.end()) {
    cursors_[cursor_key].last_sequence = next_seq;
    cursors_[cursor_key].updated_at = GetCurrentTimestamp();
  }
  
  return util::json::Value(util::json::Value::Object{
      {"events", result},
      {"next_sequence", static_cast<double>(next_seq)}});
}

core::Result<util::json::Value> BotApiService::HandleHandleCommand(const util::json::Value& payload) {
  const auto* token = payload.Find("token");
  const auto* room_id = payload.Find("room_id");
  const auto* name = payload.Find("name");
  
  if (token == nullptr || !token->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "token is required and must be a string"};
  }
  if (room_id == nullptr || !room_id->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "room_id is required and must be a string"};
  }
  if (name == nullptr || !name->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "name is required and must be a string"};
  }
  
  // Authenticate
  auto bot_result = AuthenticateToken(token->AsString());
  if (!bot_result.ok()) {
    return bot_result.error();
  }
  const auto& bot = bot_result.value();
  
  // Check capability
  auto cap_check = CheckCapability(bot, kCapCommandsHandle);
  if (!cap_check.ok()) {
    return cap_check.error();
  }
  
  // Check room scope
  auto scope_check = CheckRoomScope(bot, room_id->AsString());
  if (!scope_check.ok()) {
    return scope_check.error();
  }
  
  // Create command
  BotCommand command;
  command.command_id = GenerateCommandId();
  command.room_id = room_id->AsString();
  command.bot_id = bot.bot_id;
  command.name = name->AsString();
  command.issued_at = GetCurrentTimestamp();
  
  const auto* args = payload.Find("args");
  if (args != nullptr && args->IsString()) {
    command.args = args->AsString();
  }
  
  const auto* issued_by = payload.Find("issued_by");
  if (issued_by != nullptr && issued_by->IsString()) {
    command.issued_by = issued_by->AsString();
  }
  
  commands_.push_back(command);
  
  
  return util::json::Value(util::json::Value::Object{
      {"command_id", command.command_id},
      {"accepted", true},
      {"result", BotCommandToJson(command)}});
}

core::Result<util::json::Value> BotApiService::HandleModerateParticipant(const util::json::Value& payload) {
  const auto* token = payload.Find("token");
  const auto* room_id = payload.Find("room_id");
  const auto* participant_id = payload.Find("participant_id");
  const auto* action = payload.Find("action");
  
  if (token == nullptr || !token->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "token is required and must be a string"};
  }
  if (room_id == nullptr || !room_id->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "room_id is required and must be a string"};
  }
  if (participant_id == nullptr || !participant_id->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "participant_id is required and must be a string"};
  }
  if (action == nullptr || !action->IsString()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "action is required and must be a string"};
  }
  
  // Authenticate
  auto bot_result = AuthenticateToken(token->AsString());
  if (!bot_result.ok()) {
    return bot_result.error();
  }
  const auto& bot = bot_result.value();
  
  // Check capability based on action
  const std::string action_str = action->AsString();
  if (action_str == "kick") {
    auto cap_check = CheckCapability(bot, kCapParticipantsKick);
    if (!cap_check.ok()) {
      return cap_check.error();
    }
  } else if (action_str == "mute") {
    auto cap_check = CheckCapability(bot, kCapParticipantsMute);
    if (!cap_check.ok()) {
      return cap_check.error();
    }
  } else {
    return core::Error{core::ErrorCode::kInvalidArgument, "Unsupported moderation action: " + action_str};
  }
  
  // Check room scope
  auto scope_check = CheckRoomScope(bot, room_id->AsString());
  if (!scope_check.ok()) {
    return scope_check.error();
  }
  
  // Emit moderation event
  runtime::EventEnvelope event;
  event.topic = "room.moderation";
  event.emitted_at = GetCurrentTimestamp();
  event.payload = util::json::Value::Object{
      {"room_id", room_id->AsString()},
      {"participant_id", participant_id->AsString()},
      {"action", action_str},
      {"moderator_id", bot.bot_id},
      {"moderator_type", "bot"}};
  
  const auto* reason = payload.Find("reason");
  if (reason != nullptr && reason->IsString()) {
    event.payload.AsObject()["reason"] = reason->AsString();
  }
  
  event_bus_.Publish(event);
  
  
  return util::json::Value(util::json::Value::Object{
      {"accepted", true},
      {"action", action_str},
      {"participant_id", participant_id->AsString()}});
}

core::Result<util::json::Value> BotApiService::HandleStatus(const util::json::Value& /*payload*/) {
  std::size_t active_sessions = 0;
  for (const auto& [_, session] : sessions_) {
    if (session.state == "active") {
      ++active_sessions;
    }
  }
  
  std::unordered_map<std::string, std::size_t> room_counts;
  for (const auto& [_, session] : sessions_) {
    room_counts[session.room_id]++;
  }
  
  return util::json::Value(util::json::Value::Object{
      {"service_name", "botapi"},
      {"status", "active"},
      {"registered_bots", static_cast<double>(bots_.size())},
      {"active_sessions", static_cast<double>(active_sessions)},
      {"tracked_rooms", static_cast<double>(room_counts.size())},
      {"total_events", static_cast<double>(events_.size())},
      {"total_commands", static_cast<double>(commands_.size())}});
}

core::Result<ipc::MessageEnvelope> BotApiService::Handle(const ipc::MessageEnvelope& request) {
  if (request.topic != kServiceTopic || request.type != kRequestType) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Unexpected bot API service message"};
  }
  
  const auto* rpc = request.payload.Find("rpc");
  if (rpc == nullptr || !rpc->IsString()) {
    return core::Error{core::ErrorCode::kParseError, "Bot API request must include a string `rpc` field"};
  }
  
  const std::string rpc_name = rpc->AsString();
  core::Result<util::json::Value> result = core::Error{core::ErrorCode::kNotFound, "Unknown RPC: " + rpc_name};
  
  if (rpc_name == "RegisterBot") {
    result = HandleRegisterBot(request.payload);
  } else if (rpc_name == "GetBot") {
    result = HandleGetBot(request.payload);
  } else if (rpc_name == "ListBots") {
    result = HandleListBots(request.payload);
  } else if (rpc_name == "JoinRoom") {
    result = HandleJoinRoom(request.payload);
  } else if (rpc_name == "LeaveRoom") {
    result = HandleLeaveRoom(request.payload);
  } else if (rpc_name == "PostMessage") {
    result = HandlePostMessage(request.payload);
  } else if (rpc_name == "PollEvents") {
    result = HandlePollEvents(request.payload);
  } else if (rpc_name == "HandleCommand") {
    result = HandleHandleCommand(request.payload);
  } else if (rpc_name == "ModerateParticipant") {
    result = HandleModerateParticipant(request.payload);
  } else if (rpc_name == "Status") {
    result = HandleStatus(request.payload);
  }
  
  if (!result.ok()) {
    return result.error();
  }
  
  ipc::MessageEnvelope reply;
  reply.topic = kServiceTopic;
  reply.type = kReplyType;
  reply.payload = util::json::Value::Object{{"rpc", rpc_name}, {"result", result.value()}};
  
  return reply;
}

core::Status BotApiService::Bind(ipc::NngRequestReplyTransport& transport, std::string url) {
  return transport.Bind(std::move(url), [this](const ipc::MessageEnvelope& request) {
    auto result = Handle(request);
    if (!result.ok()) {
      ipc::MessageEnvelope error_reply;
      error_reply.topic = kServiceTopic;
      error_reply.type = kReplyType;
      error_reply.payload = util::json::Value::Object{
          {"error", true},
          {"code", static_cast<double>(result.error().code)},
          {"message", result.error().message}};
      return error_reply;
    }
    return result.value();
  });
}

}  // namespace daffy::services
