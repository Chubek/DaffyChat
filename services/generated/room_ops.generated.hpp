#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <cstdint>

#include "daffy/core/error.hpp"
#include "daffy/util/json.hpp"

namespace room_ops {

struct JoinReply {
    /** Reply payload returned when a user joins a room. */
    std::string user;
    std::string action;
    std::string service_name;
};

daffy::util::json::Value JoinReplyToJson(const JoinReply& value);
daffy::core::Result<JoinReply> ParseJoinReply(const daffy::util::json::Value& value);

struct LeaveReply {
    /** Reply payload returned when a user leaves a room. */
    std::string user;
    std::string action;
    std::string service_name;
};

daffy::util::json::Value LeaveReplyToJson(const LeaveReply& value);
daffy::core::Result<LeaveReply> ParseLeaveReply(const daffy::util::json::Value& value);

JoinReply Join(std::string user);

LeaveReply Leave(std::string user);

} // namespace room_ops
