#include "daffy/ipc/nng_transport.hpp"

namespace daffy::ipc {

util::json::Value MessageEnvelopeToJson(const MessageEnvelope& message) {
  return util::json::Value::Object{{"topic", message.topic}, {"type", message.type}, {"payload", message.payload}};
}

core::Status ValidateNngUrl(std::string_view url) {
  if (url.rfind("ipc://", 0) == 0 || url.rfind("inproc://", 0) == 0 || url.rfind("tcp://", 0) == 0) {
    return core::OkStatus();
  }
  return core::Error{core::ErrorCode::kInvalidArgument, "Unsupported NNG URL: " + std::string(url)};
}

core::Status InMemoryRequestReplyTransport::Bind(std::string url, Handler handler) {
  auto valid = ValidateNngUrl(url);
  if (!valid.ok()) {
    return valid;
  }
  const auto [it, inserted] = handlers_.emplace(std::move(url), std::move(handler));
  if (!inserted) {
    return core::Error{core::ErrorCode::kAlreadyExists, "NNG request/reply binding already exists"};
  }
  return core::OkStatus();
}

core::Result<MessageEnvelope> InMemoryRequestReplyTransport::Request(const std::string& url,
                                                                     const MessageEnvelope& request) const {
  auto valid = ValidateNngUrl(url);
  if (!valid.ok()) {
    return valid.error();
  }
  const auto it = handlers_.find(url);
  if (it == handlers_.end()) {
    return core::Error{core::ErrorCode::kNotFound, "No request/reply handler bound for URL: " + url};
  }
  return it->second(request);
}

core::Status InMemoryPubSubTransport::Subscribe(std::string url, Handler handler) {
  auto valid = ValidateNngUrl(url);
  if (!valid.ok()) {
    return valid;
  }
  subscribers_[std::move(url)].push_back(std::move(handler));
  return core::OkStatus();
}

std::size_t InMemoryPubSubTransport::Publish(const std::string& url, const MessageEnvelope& message) const {
  const auto it = subscribers_.find(url);
  if (it == subscribers_.end()) {
    return 0;
  }

  for (const auto& handler : it->second) {
    handler(message);
  }
  return it->second.size();
}

}  // namespace daffy::ipc
