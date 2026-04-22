#pragma once

#include <string>

#include "daffy/core/error.hpp"
#include "daffy/ipc/nng_transport.hpp"
#include "daffy/services/service_metadata.hpp"
#include "services/generated/room_ops.service.hpp"

namespace daffy::services {

struct JoinRoomReply {
  std::string user;
  std::string action;
  std::string service_name;
};

struct LeaveRoomReply {
  std::string user;
  std::string action;
  std::string service_name;
};

util::json::Value JoinRoomReplyToJson(const JoinRoomReply& reply);
core::Result<JoinRoomReply> ParseJoinRoomReply(const util::json::Value& value);
util::json::Value LeaveRoomReplyToJson(const LeaveRoomReply& reply);
core::Result<LeaveRoomReply> ParseLeaveRoomReply(const util::json::Value& value);

class RoomOpsService {
 public:
  static ServiceMetadata Metadata();

  core::Result<ipc::MessageEnvelope> Handle(const ipc::MessageEnvelope& request) const;
  core::Status Bind(ipc::NngRequestReplyTransport& transport, std::string url) const;
};

}  // namespace daffy::services
