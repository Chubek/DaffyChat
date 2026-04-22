#include "echo.service.hpp"

namespace echo {

namespace {
constexpr char kTopic[] = "service.echo";
constexpr char kRequestType[] = "request";
constexpr char kReplyType[] = "reply";
} // namespace

daffy::services::ServiceMetadata EchoGeneratedService::Metadata() {
    return daffy::services::ServiceMetadata{"echo", "1.0.0", "Sample echo service used to exercise the DSSL -> generated C++ -> runtime path.", "./echo.service.cpp", {"ipc"}, true};
}

daffy::core::Result<daffy::ipc::MessageEnvelope> EchoGeneratedService::Handle(const daffy::ipc::MessageEnvelope& request) const {
    if (request.topic != kTopic) {
        return daffy::core::Error{daffy::core::ErrorCode::kInvalidArgument, "Unexpected service topic"};
    }
    if (request.type != kRequestType) {
        return daffy::core::Error{daffy::core::ErrorCode::kInvalidArgument, "Unexpected service message type"};
    }
    {
        const auto* message_value = request.payload.Find("message");
        if (message_value == nullptr) {
            return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `message` in request payload"};
        }
        const auto message = (*message_value).AsString();
        const auto* sender_value = request.payload.Find("sender");
        if (sender_value == nullptr) {
            return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `sender` in request payload"};
        }
        const auto sender = (*sender_value).AsString();
        const auto result = Echo(message, sender);
        return daffy::ipc::MessageEnvelope{kTopic, kReplyType, EchoReplyToJson(result)};
    }
}

daffy::core::Status EchoGeneratedService::Bind(daffy::ipc::NngRequestReplyTransport& transport, std::string url) const {
    return transport.Bind(std::move(url), [this](const daffy::ipc::MessageEnvelope& request) { return Handle(request); });
}

} // namespace echo
