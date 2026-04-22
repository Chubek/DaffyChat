#pragma once

#include <string>

#include "daffy/core/error.hpp"
#include "daffy/ipc/nng_transport.hpp"
#include "daffy/services/service_metadata.hpp"
#include "services/generated/echo.service.hpp"

namespace daffy::services {

struct EchoRequest {
  std::string message;
  std::string sender;
};

struct EchoReply {
  std::string message;
  std::string sender;
  std::string service_name;
  bool echoed{true};
};

util::json::Value EchoRequestToJson(const EchoRequest& request);
core::Result<EchoRequest> ParseEchoRequest(const util::json::Value& value);
util::json::Value EchoReplyToJson(const EchoReply& reply);

class EchoService {
 public:
  static ServiceMetadata Metadata();

  core::Result<ipc::MessageEnvelope> Handle(const ipc::MessageEnvelope& request) const;
  core::Status Bind(ipc::NngRequestReplyTransport& transport, std::string url) const;
};

}  // namespace daffy::services
