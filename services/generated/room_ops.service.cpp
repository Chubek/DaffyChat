#include "room_ops.service.hpp"

namespace room_ops {

namespace {
constexpr char kTopic[] = "service.roomops";
constexpr char kRequestType[] = "request";
constexpr char kReplyType[] = "reply";
} // namespace

daffy::services::ServiceMetadata RoomopsGeneratedService::Metadata() {
    return daffy::services::ServiceMetadata{"roomops", "1.0.0", "Sample multi-RPC room operations service used to exercise generated dispatch.", "./room_ops.service.cpp", {"ipc"}, true};
}

daffy::core::Result<daffy::ipc::MessageEnvelope> RoomopsGeneratedService::Handle(const daffy::ipc::MessageEnvelope& request) const {
    if (request.topic != kTopic) {
        return daffy::core::Error{daffy::core::ErrorCode::kInvalidArgument, "Unexpected service topic"};
    }
    if (request.type != kRequestType) {
        return daffy::core::Error{daffy::core::ErrorCode::kInvalidArgument, "Unexpected service message type"};
    }
    const auto* rpc_name_value = request.payload.Find("rpc");
    if (rpc_name_value == nullptr || !rpc_name_value->IsString()) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Multi-RPC services require a string `rpc` field in the request payload"};
    }
    const auto rpc_name = rpc_name_value->AsString();
    if (rpc_name == "Join") {
        const auto* user_value = request.payload.Find("user");
        if (user_value == nullptr) {
            return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `user` in request payload"};
        }
        const auto user = (*user_value).AsString();
        const auto result = Join(user);
        return daffy::ipc::MessageEnvelope{kTopic, kReplyType, JoinReplyToJson(result)};
    }
    if (rpc_name == "Leave") {
        const auto* user_value = request.payload.Find("user");
        if (user_value == nullptr) {
            return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `user` in request payload"};
        }
        const auto user = (*user_value).AsString();
        const auto result = Leave(user);
        return daffy::ipc::MessageEnvelope{kTopic, kReplyType, LeaveReplyToJson(result)};
    }
    return daffy::core::Error{daffy::core::ErrorCode::kInvalidArgument, "Unknown RPC requested for this service"};
}

daffy::core::Status RoomopsGeneratedService::Bind(daffy::ipc::NngRequestReplyTransport& transport, std::string url) const {
    return transport.Bind(std::move(url), [this](const daffy::ipc::MessageEnvelope& request) { return Handle(request); });
}

} // namespace room_ops
