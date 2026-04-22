#include "daffy/services/room_ops_service.hpp"

namespace daffy::services {

util::json::Value JoinRoomReplyToJson(const JoinRoomReply& reply) {
  return room_ops::JoinReplyToJson(room_ops::JoinReply{reply.user, reply.action, reply.service_name});
}

core::Result<JoinRoomReply> ParseJoinRoomReply(const util::json::Value& value) {
  auto parsed = room_ops::ParseJoinReply(value);
  if (!parsed.ok()) {
    return parsed.error();
  }
  return JoinRoomReply{parsed.value().user, parsed.value().action, parsed.value().service_name};
}

util::json::Value LeaveRoomReplyToJson(const LeaveRoomReply& reply) {
  return room_ops::LeaveReplyToJson(room_ops::LeaveReply{reply.user, reply.action, reply.service_name});
}

core::Result<LeaveRoomReply> ParseLeaveRoomReply(const util::json::Value& value) {
  auto parsed = room_ops::ParseLeaveReply(value);
  if (!parsed.ok()) {
    return parsed.error();
  }
  return LeaveRoomReply{parsed.value().user, parsed.value().action, parsed.value().service_name};
}

ServiceMetadata RoomOpsService::Metadata() { return room_ops::RoomopsGeneratedService::Metadata(); }

core::Result<ipc::MessageEnvelope> RoomOpsService::Handle(const ipc::MessageEnvelope& request) const {
  return room_ops::RoomopsGeneratedService{}.Handle(request);
}

core::Status RoomOpsService::Bind(ipc::NngRequestReplyTransport& transport, std::string url) const {
  return room_ops::RoomopsGeneratedService{}.Bind(transport, std::move(url));
}

}  // namespace daffy::services
