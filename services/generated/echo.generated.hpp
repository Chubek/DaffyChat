#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <cstdint>

#include "daffy/core/error.hpp"
#include "daffy/util/json.hpp"

namespace echo {

struct EchoRequest {
    /** Request payload accepted by the sample service. */
    std::string message;
    std::string sender;
};

daffy::util::json::Value EchoRequestToJson(const EchoRequest& value);
daffy::core::Result<EchoRequest> ParseEchoRequest(const daffy::util::json::Value& value);

struct EchoReply {
    /** Reply payload returned by the sample service. */
    std::string message;
    std::string sender;
    std::string service_name;
    bool echoed;
};

daffy::util::json::Value EchoReplyToJson(const EchoReply& value);
daffy::core::Result<EchoReply> ParseEchoReply(const daffy::util::json::Value& value);

EchoReply Echo(std::string message, std::string sender);

} // namespace echo
