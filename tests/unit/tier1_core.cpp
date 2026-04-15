#include <cassert>
#include <sstream>
#include <string>
#include <vector>

#include "daffy/config/app_config.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/ipc/nng_transport.hpp"
#include "daffy/rooms/room_registry.hpp"
#include "daffy/runtime/event_bus.hpp"
#include "daffy/services/service_registry.hpp"
#include "daffy/signaling/messages.hpp"
#include "daffy/util/json.hpp"
#include "daffy/web/webhook.hpp"

int main() {
  auto config = daffy::config::LoadAppConfigFromFile(daffy::config::ExampleConfigPath());
  assert(config.ok());
  assert(config.value().signaling.stun_servers.size() == 1);
  assert(config.value().frontend_bridge.voice_transport == "native-client-only");
  assert(config.value().voice.preferred_capture_sample_rate == 48000);
  assert(config.value().voice.enable_noise_suppression);

  std::ostringstream log_stream;
  auto logger = daffy::core::CreateOstreamLogger("tier1-test", daffy::core::LogLevel::kInfo, log_stream);
  daffy::runtime::InMemoryEventBus event_bus;
  std::vector<daffy::runtime::EventEnvelope> events;
  event_bus.Subscribe("room.lifecycle", [&](const daffy::runtime::EventEnvelope& event) {
    events.push_back(event);
  });

  daffy::rooms::RoomRegistry room_registry(logger, event_bus);
  auto room = room_registry.CreateRoom("alpha");
  assert(room.ok());
  auto participant = room_registry.AddParticipant(room.value().id, "alice", daffy::rooms::ParticipantRole::kMember);
  assert(participant.ok());
  auto session = room_registry.AttachSession(room.value().id, participant.value().id, "peer-a");
  assert(session.ok());
  assert(room_registry.List().size() == 1);
  assert(events.size() == 3);
  assert(log_stream.str().find("Created room") != std::string::npos);

  daffy::services::ServiceRegistry service_registry;
  daffy::services::ServiceMetadata metadata{"echo", "0.1.0", "Echo service", "./echo.da", {"ipc", "rest"}, true};
  auto registered = service_registry.Register(metadata);
  assert(registered.ok());
  auto duplicate = service_registry.Register(metadata);
  assert(!duplicate.ok());
  auto found_service = service_registry.Find("echo");
  assert(found_service.ok());
  assert(daffy::util::json::Serialize(daffy::services::ServiceMetadataToJson(found_service.value())).find("echo") !=
         std::string::npos);

  daffy::signaling::Message signaling_message;
  signaling_message.type = daffy::signaling::MessageType::kIceCandidate;
  signaling_message.candidate = "candidate:1 1 UDP 2122252543 192.0.2.1 54400 typ host";
  signaling_message.mid = "audio";
  auto parsed_message = daffy::signaling::ParseMessage(daffy::signaling::SerializeMessage(signaling_message));
  assert(parsed_message.ok());
  assert(parsed_message.value().candidate == signaling_message.candidate);

  daffy::ipc::InMemoryRequestReplyTransport request_reply;
  auto bind_status = request_reply.Bind("ipc:///tmp/daffychat-services.ipc", [](const daffy::ipc::MessageEnvelope& request) {
    return daffy::ipc::MessageEnvelope{"service.echo", "reply", request.payload};
  });
  assert(bind_status.ok());
  auto reply = request_reply.Request(
      "ipc:///tmp/daffychat-services.ipc",
      daffy::ipc::MessageEnvelope{"service.echo", "request", daffy::util::json::Value::Object{{"message", "ping"}}});
  assert(reply.ok());
  assert(reply.value().type == "reply");

  daffy::ipc::InMemoryPubSubTransport pubsub;
  std::size_t delivered_messages = 0;
  auto subscribe_status = pubsub.Subscribe("inproc://room-events", [&](const daffy::ipc::MessageEnvelope& message) {
    if (message.topic == "room.lifecycle") {
      ++delivered_messages;
    }
  });
  assert(subscribe_status.ok());
  const auto delivered = pubsub.Publish(
      "inproc://room-events",
      daffy::ipc::MessageEnvelope{"room.lifecycle", "event", daffy::util::json::Value::Object{{"room", "alpha"}}});
  assert(delivered == 1);
  assert(delivered_messages == 1);

  daffy::web::RecordingWebhookDispatcher webhooks;
  daffy::web::WebhookSubscription subscription{"hook-1", "room.lifecycle", "https://example.test/hook", true};
  auto delivery = webhooks.Dispatch(subscription, events.back());
  assert(delivery.ok());
  assert(webhooks.deliveries().size() == 1);
  assert(daffy::util::json::Serialize(daffy::web::WebhookDeliveryToJson(delivery.value())).find("example.test") !=
         std::string::npos);

  return 0;
}
