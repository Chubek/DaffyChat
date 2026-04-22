#include <cassert>
#include <string>
#include <unistd.h>

#include "daffy/ipc/nng_transport.hpp"
#include "daffy/services/bot_api_service.hpp"

int main() {
  daffy::ipc::NngRequestReplyTransport transport;
  const std::string url = "inproc://botapi-" + std::to_string(::getpid());

  daffy::services::BotApiService service;
  assert(service.Bind(transport, url).ok());

  // Test metadata
  auto metadata = daffy::services::BotApiService::Metadata();
  assert(metadata.name == "botapi");
  assert(metadata.version == "1.0.0");
  assert(metadata.enabled);

  // Test RegisterBot
  auto register_bot = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{
          "service.botapi",
          "request",
          daffy::util::json::Value::Object{
              {"rpc", "RegisterBot"},
              {"display_name", "test-bot"},
              {"capabilities", daffy::util::json::Value::Array{"rooms.join", "messages.write"}},
              {"room_scope", daffy::util::json::Value::Array{"room-1"}}}});
  assert(register_bot.ok());
  
  const auto* result = register_bot.value().payload.Find("result");
  assert(result != nullptr);
  const auto* bot = result->Find("bot");
  assert(bot != nullptr);
  assert(bot->Find("display_name")->AsString() == "test-bot");
  assert(bot->Find("enabled")->AsBool());
  
  const auto* token = result->Find("token");
  assert(token != nullptr);
  const std::string bearer = token->Find("bearer")->AsString();
  assert(!bearer.empty());
  assert(bearer.find("dcb_") == 0);
  
  const std::string bot_id = bot->Find("bot_id")->AsString();

  // Test GetBot
  auto get_bot = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{
          "service.botapi",
          "request",
          daffy::util::json::Value::Object{
              {"rpc", "GetBot"},
              {"bot_id", bot_id}}});
  assert(get_bot.ok());
  const auto* get_result = get_bot.value().payload.Find("result");
  assert(get_result != nullptr);
  assert(get_result->Find("bot_id")->AsString() == bot_id);

  // Test ListBots
  auto list_bots = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{
          "service.botapi",
          "request",
          daffy::util::json::Value::Object{{"rpc", "ListBots"}}});
  assert(list_bots.ok());
  const auto* list_result = list_bots.value().payload.Find("result");
  assert(list_result != nullptr);
  assert(list_result->IsArray());
  assert(list_result->AsArray().size() >= 1);

  // Test JoinRoom
  auto join_room = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{
          "service.botapi",
          "request",
          daffy::util::json::Value::Object{
              {"rpc", "JoinRoom"},
              {"token", bearer},
              {"room_id", "room-1"}}});
  assert(join_room.ok());
  const auto* join_result = join_room.value().payload.Find("result");
  assert(join_result != nullptr);
  const auto* session = join_result->Find("session");
  assert(session != nullptr);
  assert(session->Find("room_id")->AsString() == "room-1");
  assert(session->Find("state")->AsString() == "active");

  // Test PostMessage
  auto post_message = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{
          "service.botapi",
          "request",
          daffy::util::json::Value::Object{
              {"rpc", "PostMessage"},
              {"token", bearer},
              {"room_id", "room-1"},
              {"text", "Hello from bot!"}}});
  assert(post_message.ok());
  const auto* msg_result = post_message.value().payload.Find("result");
  assert(msg_result != nullptr);
  assert(msg_result->Find("accepted")->AsBool());
  // Test PollEvents
  auto poll_events = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{
          "service.botapi",
          "request",
          daffy::util::json::Value::Object{
              {"rpc", "PollEvents"},
              {"token", bearer},
              {"room_id", "room-1"},
              {"after_sequence", 0.0},
              {"limit", 10.0}}});
  
  if (!poll_events.ok()) {
    // If request failed, check if it's an error response
    return 1;
  }
  
  const auto* poll_result = poll_events.value().payload.Find("result");
  if (poll_result != nullptr) {
    assert(poll_result->Find("events")->IsArray());
  }
  // Test Status
  auto status = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{
          "service.botapi",
          "request",
          daffy::util::json::Value::Object{{"rpc", "Status"}}});
  assert(status.ok());
  const auto* status_result = status.value().payload.Find("result");
  assert(status_result != nullptr);
  assert(status_result->Find("service_name")->AsString() == "botapi");
  assert(status_result->Find("status")->AsString() == "active");
  assert(status_result->Find("registered_bots")->AsNumber() >= 1.0);

  // Test LeaveRoom
  auto leave_room = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{
          "service.botapi",
          "request",
          daffy::util::json::Value::Object{
              {"rpc", "LeaveRoom"},
              {"token", bearer},
              {"room_id", "room-1"}}});
  assert(leave_room.ok());
  const auto* leave_result = leave_room.value().payload.Find("result");
  assert(leave_result != nullptr);
  assert(leave_result->IsBool());
  assert(leave_result->AsBool());

  return 0;
}
