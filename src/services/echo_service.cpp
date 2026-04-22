#include "daffy/services/echo_service.hpp"

namespace daffy::services {

util::json::Value EchoRequestToJson(const EchoRequest& request) {
  return echo::EchoRequestToJson(echo::EchoRequest{request.message, request.sender});
}

core::Result<EchoRequest> ParseEchoRequest(const util::json::Value& value) {
  auto parsed = echo::ParseEchoRequest(value);
  if (!parsed.ok()) {
    return parsed.error();
  }
  return EchoRequest{parsed.value().message, parsed.value().sender};
}

util::json::Value EchoReplyToJson(const EchoReply& reply) {
  return echo::EchoReplyToJson(echo::EchoReply{reply.message, reply.sender, reply.service_name, reply.echoed});
}

ServiceMetadata EchoService::Metadata() { return echo::EchoGeneratedService::Metadata(); }

core::Result<ipc::MessageEnvelope> EchoService::Handle(const ipc::MessageEnvelope& request) const {
  return echo::EchoGeneratedService{}.Handle(request);
}

core::Status EchoService::Bind(ipc::NngRequestReplyTransport& transport, std::string url) const {
  return echo::EchoGeneratedService{}.Bind(transport, std::move(url));
}

}  // namespace daffy::services
