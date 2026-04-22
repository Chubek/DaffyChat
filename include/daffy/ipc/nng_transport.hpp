#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
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
core::Result<MessageEnvelope> ParseMessageEnvelope(const util::json::Value& value);
core::Status ValidateNngUrl(std::string_view url);

class NngRequestReplyTransport {
 public:
  using Handler = std::function<core::Result<MessageEnvelope>(const MessageEnvelope&)>;

  NngRequestReplyTransport();
  ~NngRequestReplyTransport();

  NngRequestReplyTransport(const NngRequestReplyTransport&) = delete;
  NngRequestReplyTransport& operator=(const NngRequestReplyTransport&) = delete;
  NngRequestReplyTransport(NngRequestReplyTransport&&) = delete;
  NngRequestReplyTransport& operator=(NngRequestReplyTransport&&) = delete;

  core::Status Bind(std::string url, Handler handler);
  core::Result<MessageEnvelope> Request(const std::string& url, const MessageEnvelope& request) const;

 private:
  struct ServerBinding;

  std::mutex mutex_;
  std::vector<std::shared_ptr<ServerBinding>> bindings_;
};

class NngPubSubTransport {
 public:
  using Handler = std::function<void(const MessageEnvelope&)>;

  NngPubSubTransport();
  ~NngPubSubTransport();

  NngPubSubTransport(const NngPubSubTransport&) = delete;
  NngPubSubTransport& operator=(const NngPubSubTransport&) = delete;
  NngPubSubTransport(NngPubSubTransport&&) = delete;
  NngPubSubTransport& operator=(NngPubSubTransport&&) = delete;

  core::Status Subscribe(std::string url, Handler handler);
  std::size_t Publish(const std::string& url, const MessageEnvelope& message) const;

 private:
  struct SubscriberBinding;
  struct PublisherBinding;

  std::shared_ptr<PublisherBinding> EnsurePublisher(const std::string& url) const;

  mutable std::mutex mutex_;
  mutable std::unordered_map<std::string, std::shared_ptr<PublisherBinding>> publishers_;
  std::vector<std::shared_ptr<SubscriberBinding>> subscribers_;
};

using InMemoryRequestReplyTransport = NngRequestReplyTransport;
using InMemoryPubSubTransport = NngPubSubTransport;

}  // namespace daffy::ipc
