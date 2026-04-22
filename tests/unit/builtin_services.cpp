#include <cassert>
#include <string>
#include <unistd.h>

#include "daffy/ipc/nng_transport.hpp"
#include "daffy/services/health_service.hpp"
#include "daffy/services/room_state_service.hpp"

int main() {
  daffy::ipc::NngRequestReplyTransport transport;
  const std::string suffix = std::to_string(::getpid());
  const std::string health_url = "inproc://health-" + suffix;
  const std::string roomstate_url = "inproc://roomstate-" + suffix;

  daffy::services::HealthService health_service;
  assert(health_service.Bind(transport, health_url).ok());
  auto health_reply = transport.Request(health_url,
                                        daffy::ipc::MessageEnvelope{"service.health",
                                                                    "request",
                                                                    daffy::util::json::Value::Object{{"rpc", "Status"}}});
  assert(health_reply.ok());
  const auto* status = health_reply.value().payload.Find("status");
  const auto* service_name = health_reply.value().payload.Find("service_name");
  assert(status != nullptr && status->IsString() && status->AsString() == "ok");
  assert(service_name != nullptr && service_name->IsString() && service_name->AsString() == "health");

  daffy::services::RoomStateService room_state_service;
  assert(room_state_service.Bind(transport, roomstate_url).ok());

  auto create_reply = transport.Request(roomstate_url,
                                        daffy::ipc::MessageEnvelope{
                                            "service.roomstate",
                                            "request",
                                            daffy::util::json::Value::Object{{"rpc", "CreateRoom"}, {"display_name", "Launch Room"}},
                                        });
  assert(create_reply.ok());
  const auto* room = create_reply.value().payload.Find("room");
  assert(room != nullptr && room->IsObject());
  const auto* room_id = room->Find("id");
  assert(room_id != nullptr && room_id->IsString());

  auto add_participant_reply = transport.Request(roomstate_url,
                                                 daffy::ipc::MessageEnvelope{
                                                     "service.roomstate",
                                                     "request",
                                                     daffy::util::json::Value::Object{{"rpc", "AddParticipant"},
                                                                                      {"room_id", room_id->AsString()},
                                                                                      {"display_name", "Operator"},
                                                                                      {"role", "admin"}},
                                                 });
  assert(add_participant_reply.ok());
  const auto* participant = add_participant_reply.value().payload.Find("participant");
  assert(participant != nullptr && participant->IsObject());
  const auto* participant_id = participant->Find("id");
  assert(participant_id != nullptr && participant_id->IsString());

  auto attach_reply = transport.Request(roomstate_url,
                                        daffy::ipc::MessageEnvelope{
                                            "service.roomstate",
                                            "request",
                                            daffy::util::json::Value::Object{{"rpc", "AttachSession"},
                                                                             {"room_id", room_id->AsString()},
                                                                             {"participant_id", participant_id->AsString()},
                                                                             {"peer_id", "peer-1"}},
                                        });
  assert(attach_reply.ok());

  auto state_reply = transport.Request(roomstate_url,
                                       daffy::ipc::MessageEnvelope{
                                           "service.roomstate",
                                           "request",
                                           daffy::util::json::Value::Object{{"rpc", "SetRoomState"},
                                                                            {"room_id", room_id->AsString()},
                                                                            {"state", "closing"}},
                                       });
  assert(state_reply.ok());

  auto list_reply = transport.Request(roomstate_url,
                                      daffy::ipc::MessageEnvelope{
                                          "service.roomstate",
                                          "request",
                                          daffy::util::json::Value::Object{{"rpc", "ListRooms"}},
                                      });
  assert(list_reply.ok());
  const auto* rooms = list_reply.value().payload.Find("rooms");
  assert(rooms != nullptr && rooms->IsArray() && rooms->AsArray().size() == 1);

  auto events_reply = transport.Request(roomstate_url,
                                        daffy::ipc::MessageEnvelope{
                                            "service.roomstate",
                                            "request",
                                            daffy::util::json::Value::Object{{"rpc", "PollEvents"}},
                                        });
  assert(events_reply.ok());
  const auto* events = events_reply.value().payload.Find("events");
  assert(events != nullptr && events->IsArray() && events->AsArray().size() >= 4);

  return 0;
}
