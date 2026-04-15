#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/util/json.hpp"

namespace daffy::ipc {

struct MessageEnvelope {
  std::string topic;
  std::string type;
  util::json::Value payload;
};

util::json::Value MessageEnvelopeToJson(const MessageEnvelope& message);
core::Status ValidateNngUrl(std::string_view url);

class InMemoryRequestReplyTransport {
 public:
  using Handler = std::function<core::Result<MessageEnvelope>(const MessageEnvelope&)>;

  core::Status Bind(std::string url, Handler handler);
  core::Result<MessageEnvelope> Request(const std::string& url, const MessageEnvelope& request) const;

 private:
  std::unordered_map<std::string, Handler> handlers_;
};

class InMemoryPubSubTransport {
 public:
  using Handler = std::function<void(const MessageEnvelope&)>;

  core::Status Subscribe(std::string url, Handler handler);
  std::size_t Publish(const std::string& url, const MessageEnvelope& message) const;

 private:
  std::unordered_map<std::string, std::vector<Handler>> subscribers_;
};

}  // namespace daffy::ipc
