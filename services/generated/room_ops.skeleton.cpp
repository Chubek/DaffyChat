#include "room_ops.generated.hpp"

namespace room_ops {

daffy::util::json::Value JoinReplyToJson(const JoinReply& value) {
    return daffy::util::json::Value::Object{{"user", daffy::util::json::Value(value.user)}, {"action", daffy::util::json::Value(value.action)}, {"service_name", daffy::util::json::Value(value.service_name)}};
}

daffy::core::Result<JoinReply> ParseJoinReply(const daffy::util::json::Value& value) {
    if (!value.IsObject()) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "JoinReply must be a JSON object"};
    }
    JoinReply parsed{};
    const auto* user_value = value.Find("user");
    if (user_value == nullptr) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `user` in JoinReply"};
    }
    parsed.user = (*user_value).AsString();
    const auto* action_value = value.Find("action");
    if (action_value == nullptr) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `action` in JoinReply"};
    }
    parsed.action = (*action_value).AsString();
    const auto* service_name_value = value.Find("service_name");
    if (service_name_value == nullptr) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `service_name` in JoinReply"};
    }
    parsed.service_name = (*service_name_value).AsString();
    return parsed;
}

daffy::util::json::Value LeaveReplyToJson(const LeaveReply& value) {
    return daffy::util::json::Value::Object{{"user", daffy::util::json::Value(value.user)}, {"action", daffy::util::json::Value(value.action)}, {"service_name", daffy::util::json::Value(value.service_name)}};
}

daffy::core::Result<LeaveReply> ParseLeaveReply(const daffy::util::json::Value& value) {
    if (!value.IsObject()) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "LeaveReply must be a JSON object"};
    }
    LeaveReply parsed{};
    const auto* user_value = value.Find("user");
    if (user_value == nullptr) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `user` in LeaveReply"};
    }
    parsed.user = (*user_value).AsString();
    const auto* action_value = value.Find("action");
    if (action_value == nullptr) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `action` in LeaveReply"};
    }
    parsed.action = (*action_value).AsString();
    const auto* service_name_value = value.Find("service_name");
    if (service_name_value == nullptr) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `service_name` in LeaveReply"};
    }
    parsed.service_name = (*service_name_value).AsString();
    return parsed;
}

JoinReply Join(std::string user) {
    JoinReply result{};
    result.user = user;
    result.action = "action";
    result.service_name = "roomops";
    return result;
}

LeaveReply Leave(std::string user) {
    LeaveReply result{};
    result.user = user;
    result.action = "action";
    result.service_name = "roomops";
    return result;
}

} // namespace room_ops
