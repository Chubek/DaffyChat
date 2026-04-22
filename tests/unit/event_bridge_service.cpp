#include <cassert>
#include <string>
#include <unistd.h>

#include "daffy/ipc/nng_transport.hpp"
#include "daffy/services/event_bridge_service.hpp"

int main() {
  daffy::ipc::NngRequestReplyTransport transport;
  const std::string url = "inproc://eventbridge-" + std::to_string(::getpid());

  daffy::services::EventBridgeService service;
  assert(service.Bind(transport, url).ok());

  auto create_room = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{"service.eventbridge",
                                  "request",
                                  daffy::util::json::Value::Object{{"rpc", "CreateRoom"}, {"display_name", "Bridge Room"}}});
  assert(create_room.ok());
  const auto* room = create_room.value().payload.Find("room");
  assert(room != nullptr && room->IsObject());
  const auto* room_id = room->Find("id");
  assert(room_id != nullptr && room_id->IsString());

  auto register_webhook = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{"service.eventbridge",
                                  "request",
                                  daffy::util::json::Value::Object{{"rpc", "RegisterWebhook"},
                                                                   {"topic", "room.lifecycle"},
                                                                   {"url", "https://example.test/room-events"}}});
  assert(register_webhook.ok());

  auto add_participant = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{"service.eventbridge",
                                  "request",
                                  daffy::util::json::Value::Object{{"rpc", "AddParticipant"},
                                                                   {"room_id", room_id->AsString()},
                                                                   {"display_name", "alice"},
                                                                   {"role", "member"}}});
  assert(add_participant.ok());
  const auto* participant = add_participant.value().payload.Find("participant");
  assert(participant != nullptr && participant->IsObject());
  const auto* participant_id = participant->Find("id");
  assert(participant_id != nullptr && participant_id->IsString());

  auto attach = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{"service.eventbridge",
                                  "request",
                                  daffy::util::json::Value::Object{{"rpc", "AttachSession"},
                                                                   {"room_id", room_id->AsString()},
                                                                   {"participant_id", participant_id->AsString()},
                                                                   {"peer_id", "peer-42"}}});
  assert(attach.ok());

  auto poll = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{"service.eventbridge",
                                  "request",
                                  daffy::util::json::Value::Object{{"rpc", "PollEvents"}, {"after_sequence", 0.0}}});
  assert(poll.ok());
  const auto* events = poll.value().payload.Find("events");
  assert(events != nullptr && events->IsArray() && events->AsArray().size() >= 3);

  auto dispatch = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{"service.eventbridge",
                                  "request",
                                  daffy::util::json::Value::Object{{"rpc", "DispatchWebhooks"}}});
  assert(dispatch.ok());
  const auto* deliveries = dispatch.value().payload.Find("deliveries");
  assert(deliveries != nullptr && deliveries->IsArray() && !deliveries->AsArray().empty());

  auto status = transport.Request(
      url,
      daffy::ipc::MessageEnvelope{"service.eventbridge",
                                  "request",
                                  daffy::util::json::Value::Object{{"rpc", "Status"}}});
  assert(status.ok());
  const auto* service_name = status.value().payload.Find("service_name");
  assert(service_name != nullptr && service_name->IsString() && service_name->AsString() == "eventbridge");

  return 0;
}
