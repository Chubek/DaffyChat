#include "echo.generated.hpp"

namespace echo {

daffy::util::json::Value EchoRequestToJson(const EchoRequest& value) {
    return daffy::util::json::Value::Object{{"message", daffy::util::json::Value(value.message)}, {"sender", daffy::util::json::Value(value.sender)}};
}

daffy::core::Result<EchoRequest> ParseEchoRequest(const daffy::util::json::Value& value) {
    if (!value.IsObject()) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "EchoRequest must be a JSON object"};
    }
    EchoRequest parsed{};
    const auto* message_value = value.Find("message");
    if (message_value == nullptr) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `message` in EchoRequest"};
    }
    parsed.message = (*message_value).AsString();
    const auto* sender_value = value.Find("sender");
    if (sender_value == nullptr) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `sender` in EchoRequest"};
    }
    parsed.sender = (*sender_value).AsString();
    return parsed;
}

daffy::util::json::Value EchoReplyToJson(const EchoReply& value) {
    return daffy::util::json::Value::Object{{"message", daffy::util::json::Value(value.message)}, {"sender", daffy::util::json::Value(value.sender)}, {"service_name", daffy::util::json::Value(value.service_name)}, {"echoed", daffy::util::json::Value(value.echoed)}};
}

daffy::core::Result<EchoReply> ParseEchoReply(const daffy::util::json::Value& value) {
    if (!value.IsObject()) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "EchoReply must be a JSON object"};
    }
    EchoReply parsed{};
    const auto* message_value = value.Find("message");
    if (message_value == nullptr) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `message` in EchoReply"};
    }
    parsed.message = (*message_value).AsString();
    const auto* sender_value = value.Find("sender");
    if (sender_value == nullptr) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `sender` in EchoReply"};
    }
    parsed.sender = (*sender_value).AsString();
    const auto* service_name_value = value.Find("service_name");
    if (service_name_value == nullptr) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `service_name` in EchoReply"};
    }
    parsed.service_name = (*service_name_value).AsString();
    const auto* echoed_value = value.Find("echoed");
    if (echoed_value == nullptr) {
        return daffy::core::Error{daffy::core::ErrorCode::kParseError, "Missing field `echoed` in EchoReply"};
    }
    parsed.echoed = (*echoed_value).AsBool();
    return parsed;
}

EchoReply Echo(std::string message, std::string sender) {
    EchoReply result{};
    result.message = message;
    result.sender = sender;
    result.service_name = "echo";
    result.echoed = true;
    return result;
}

} // namespace echo
