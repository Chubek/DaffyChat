#include <cassert>
#include <string>
#include <unistd.h>

#include "daffy/ipc/nng_transport.hpp"
#include "daffy/util/json.hpp"
#include "services/generated/room_ops.generated.hpp"
#include "services/generated/room_ops.service.hpp"

int main() {
  const auto metadata = room_ops::RoomopsGeneratedService::Metadata();
  assert(metadata.name == "roomops");
  assert(metadata.version == "1.0.0");

  daffy::ipc::NngRequestReplyTransport transport;
  const room_ops::RoomopsGeneratedService service;
  const std::string service_url = "ipc:///tmp/daffychat-room-ops-service-" + std::to_string(getpid()) + ".ipc";
  auto bind_status = service.Bind(transport, service_url);
  assert(bind_status.ok());

  auto join_reply = transport.Request(service_url,
                                      daffy::ipc::MessageEnvelope{
                                          "service.roomops",
                                          "request",
                                          daffy::util::json::Value::Object{{"rpc", "Join"}, {"user", "alice"}},
                                      });
  assert(join_reply.ok());
  assert(join_reply.value().topic == "service.roomops");
  assert(join_reply.value().type == "reply");
  auto parsed_join = room_ops::ParseJoinReply(join_reply.value().payload);
  assert(parsed_join.ok());
  assert(parsed_join.value().user == "alice");
  assert(parsed_join.value().service_name == "roomops");

  auto leave_reply = transport.Request(service_url,
                                       daffy::ipc::MessageEnvelope{
                                           "service.roomops",
                                           "request",
                                           daffy::util::json::Value::Object{{"rpc", "Leave"}, {"user", "bob"}},
                                       });
  assert(leave_reply.ok());
  auto parsed_leave = room_ops::ParseLeaveReply(leave_reply.value().payload);
  assert(parsed_leave.ok());
  assert(parsed_leave.value().user == "bob");
  assert(parsed_leave.value().service_name == "roomops");

  auto missing_rpc = service.Handle(daffy::ipc::MessageEnvelope{
      "service.roomops",
      "request",
      daffy::util::json::Value::Object{{"user", "charlie"}},
  });
  assert(!missing_rpc.ok());

  auto unknown_rpc = service.Handle(daffy::ipc::MessageEnvelope{
      "service.roomops",
      "request",
      daffy::util::json::Value::Object{{"rpc", "Rename"}, {"user", "charlie"}},
  });
  assert(!unknown_rpc.ok());

  return 0;
}
