#include <cassert>
#include <string>
#include <unistd.h>

#include "daffy/ipc/nng_transport.hpp"
#include "daffy/services/echo_service.hpp"
#include "daffy/services/service_registry.hpp"
#include "daffy/util/json.hpp"

int main() {
  daffy::services::ServiceRegistry registry;
  const auto metadata = daffy::services::EchoService::Metadata();
  auto register_status = registry.Register(metadata);
  assert(register_status.ok());

  daffy::ipc::NngRequestReplyTransport transport;
  const daffy::services::EchoService service;
  const std::string service_url = "ipc:///tmp/daffychat-echo-service-" + std::to_string(getpid()) + ".ipc";
  auto bind_status = service.Bind(transport, service_url);
  assert(bind_status.ok());

  auto reply = transport.Request(service_url,
                                 daffy::ipc::MessageEnvelope{
                                     "service.echo",
                                     "request",
                                     daffy::services::EchoRequestToJson({"ping from test", "alice"}),
                                 });
  assert(reply.ok());
  assert(reply.value().topic == "service.echo");
  assert(reply.value().type == "reply");

  const auto* service_name = reply.value().payload.Find("service_name");
  const auto* echoed = reply.value().payload.Find("echoed");
  const auto* sender = reply.value().payload.Find("sender");
  assert(service_name != nullptr && service_name->IsString());
  assert(echoed != nullptr && echoed->IsBool());
  assert(sender != nullptr && sender->IsString());
  assert(service_name->AsString() == "echo");
  assert(echoed->AsBool());
  assert(sender->AsString() == "alice");

  auto missing_sender = service.Handle(daffy::ipc::MessageEnvelope{
      "service.echo",
      "request",
      daffy::util::json::Value::Object{{"message", "ping"}},
  });
  assert(!missing_sender.ok());

  return 0;
}
