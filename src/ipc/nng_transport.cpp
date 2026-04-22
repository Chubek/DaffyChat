#include "daffy/ipc/nng_transport.hpp"

#include <chrono>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <utility>

#include <nng/nng.h>

namespace daffy::ipc {

namespace {

core::Error MakeNngError(const std::string& context, const int code) {
  return core::Error{core::ErrorCode::kUnavailable, context + ": " + std::string(nng_strerror(static_cast<nng_err>(code)))};
}

core::Result<std::string> EncodeEnvelope(const MessageEnvelope& message) {
  return util::json::Serialize(MessageEnvelopeToJson(message));
}

void RemoveExistingIpcSocket(std::string_view url) {
  if (url.rfind("ipc://", 0) != 0) {
    return;
  }
  const std::string path(url.substr(6));
  if (!path.empty()) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
}

core::Result<MessageEnvelope> DecodeEnvelope(const char* data, const std::size_t size) {
  auto parsed = util::json::Parse(std::string_view(data, size));
  if (!parsed.ok()) {
    return parsed.error();
  }
  return ParseMessageEnvelope(parsed.value());
}

core::Result<MessageEnvelope> ReceiveEnvelope(const nng_socket socket) {
  nng_msg* msg = nullptr;
  const int rv = nng_recvmsg(socket, &msg, 0);
  if (rv != 0) {
    return MakeNngError("NNG receive failed", rv);
  }

  auto decoded = DecodeEnvelope(static_cast<const char*>(nng_msg_body(msg)), nng_msg_len(msg));
  nng_msg_free(msg);
  return decoded;
}

core::Status SendEnvelope(const nng_socket socket, const MessageEnvelope& message) {
  auto encoded = EncodeEnvelope(message);
  if (!encoded.ok()) {
    return encoded.error();
  }

  nng_msg* msg = nullptr;
  int rv = nng_msg_alloc(&msg, 0);
  if (rv != 0) {
    return MakeNngError("NNG message allocation failed", rv);
  }
  rv = nng_msg_append(msg, encoded.value().data(), encoded.value().size());
  if (rv != 0) {
    nng_msg_free(msg);
    return MakeNngError("NNG message append failed", rv);
  }
  rv = nng_sendmsg(socket, msg, 0);
  if (rv != 0) {
    nng_msg_free(msg);
    return MakeNngError("NNG send failed", rv);
  }
  return core::OkStatus();
}

}  // namespace

struct NngRequestReplyTransport::ServerBinding {
  std::string url;
  Handler handler;
  nng_socket socket{};
  std::thread worker;
  std::atomic<bool> running{false};
};

struct NngPubSubTransport::PublisherBinding {
  explicit PublisherBinding(std::string url_in) : url(std::move(url_in)) {}

  std::string url;
  nng_socket socket{};
};

struct NngPubSubTransport::SubscriberBinding {
  std::string url;
  Handler handler;
  nng_socket socket{};
  std::thread worker;
  std::atomic<bool> running{false};
};

util::json::Value MessageEnvelopeToJson(const MessageEnvelope& message) {
  return util::json::Value::Object{{"topic", message.topic}, {"type", message.type}, {"payload", message.payload}};
}

core::Result<MessageEnvelope> ParseMessageEnvelope(const util::json::Value& value) {
  if (!value.IsObject()) {
    return core::Error{core::ErrorCode::kParseError, "Message envelope must be a JSON object"};
  }

  const auto* topic = value.Find("topic");
  const auto* type = value.Find("type");
  const auto* payload = value.Find("payload");
  if (topic == nullptr || type == nullptr || payload == nullptr || !topic->IsString() || !type->IsString()) {
    return core::Error{core::ErrorCode::kParseError, "Message envelope requires string fields `topic` and `type`"};
  }

  return MessageEnvelope{topic->AsString(), type->AsString(), *payload};
}

core::Status ValidateNngUrl(std::string_view url) {
  if (url.rfind("ipc://", 0) == 0 || url.rfind("inproc://", 0) == 0 || url.rfind("tcp://", 0) == 0) {
    return core::OkStatus();
  }
  return core::Error{core::ErrorCode::kInvalidArgument, "Unsupported NNG URL: " + std::string(url)};
}

NngRequestReplyTransport::NngRequestReplyTransport() {
  static_cast<void>(nng_init(nullptr));
}

NngRequestReplyTransport::~NngRequestReplyTransport() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& binding : bindings_) {
    binding->running = false;
    nng_socket_close(binding->socket);
  }
  for (const auto& binding : bindings_) {
    if (binding->worker.joinable()) {
      binding->worker.join();
    }
  }
}

core::Status NngRequestReplyTransport::Bind(std::string url, Handler handler) {
  auto valid = ValidateNngUrl(url);
  if (!valid.ok()) {
    return valid;
  }

  auto binding = std::make_shared<ServerBinding>();
  binding->url = std::move(url);
  binding->handler = std::move(handler);

  int rv = nng_rep0_open(&binding->socket);
  if (rv != 0) {
    return MakeNngError("Failed to open NNG rep socket", rv);
  }

  rv = nng_socket_set_ms(binding->socket, NNG_OPT_RECVTIMEO, 100);
  if (rv != 0) {
    nng_socket_close(binding->socket);
    return MakeNngError("Failed to set rep recv timeout", rv);
  }

  RemoveExistingIpcSocket(binding->url);
  rv = nng_listen(binding->socket, binding->url.c_str(), nullptr, 0);
  if (rv != 0) {
    nng_socket_close(binding->socket);
    return MakeNngError("Failed to bind NNG rep socket", rv);
  }

  binding->running = true;
  binding->worker = std::thread([binding]() {
    while (binding->running.load()) {
      auto request = ReceiveEnvelope(binding->socket);
      if (!request.ok()) {
        if (request.error().message.find("Timed out") != std::string::npos ||
            request.error().message.find("Object closed") != std::string::npos) {
          continue;
        }
        break;
      }

      MessageEnvelope reply_message;
      if (request.ok()) {
        auto reply = binding->handler(request.value());
        if (reply.ok()) {
          reply_message = reply.value();
        } else {
          reply_message = MessageEnvelope{"service.error",
                                          "error",
                                          util::json::Value::Object{{"message", reply.error().message}}};
        }
      } else {
        reply_message = MessageEnvelope{"service.error",
                                        "error",
                                        util::json::Value::Object{{"message", request.error().message}}};
      }

      if (!SendEnvelope(binding->socket, reply_message).ok()) {
        continue;
      }
    }
  });

  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& existing : bindings_) {
    if (existing->url == binding->url) {
      binding->running = false;
      nng_socket_close(binding->socket);
      if (binding->worker.joinable()) {
        binding->worker.join();
      }
      return core::Error{core::ErrorCode::kAlreadyExists, "NNG request/reply binding already exists"};
    }
  }
  bindings_.push_back(binding);

  // Let the background REP worker enter its receive loop before the first client dials.
  std::this_thread::sleep_for(std::chrono::milliseconds(75));
  return core::OkStatus();
}

core::Result<MessageEnvelope> NngRequestReplyTransport::Request(const std::string& url,
                                                                     const MessageEnvelope& request) const {
  auto valid = ValidateNngUrl(url);
  if (!valid.ok()) {
    return valid.error();
  }

  nng_socket socket{};
  int rv = nng_req0_open(&socket);
  if (rv != 0) {
    return MakeNngError("Failed to open NNG req socket", rv);
  }

  rv = nng_socket_set_ms(socket, NNG_OPT_RECVTIMEO, 1000);
  if (rv != 0) {
    nng_socket_close(socket);
    return MakeNngError("Failed to set req recv timeout", rv);
  }
  rv = nng_socket_set_ms(socket, NNG_OPT_SENDTIMEO, 1000);
  if (rv != 0) {
    nng_socket_close(socket);
    return MakeNngError("Failed to set req send timeout", rv);
  }
  rv = nng_dial(socket, url.c_str(), nullptr, 0);
  if (rv != 0) {
    nng_socket_close(socket);
    return MakeNngError("Failed to dial NNG req socket", rv);
  }

  auto send_status = SendEnvelope(socket, request);
  if (!send_status.ok()) {
    nng_socket_close(socket);
    return send_status.error();
  }

  auto reply = ReceiveEnvelope(socket);
  if (!reply.ok()) {
    nng_socket_close(socket);
    return reply.error();
  }
  nng_socket_close(socket);
  return reply;
}

NngPubSubTransport::NngPubSubTransport() {
  static_cast<void>(nng_init(nullptr));
}

NngPubSubTransport::~NngPubSubTransport() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto& subscriber : subscribers_) {
    subscriber->running = false;
    nng_socket_close(subscriber->socket);
  }
  for (const auto& subscriber : subscribers_) {
    if (subscriber->worker.joinable()) {
      subscriber->worker.join();
    }
  }
  for (const auto& [url, publisher] : publishers_) {
    static_cast<void>(url);
    nng_socket_close(publisher->socket);
  }
}

std::shared_ptr<NngPubSubTransport::PublisherBinding> NngPubSubTransport::EnsurePublisher(
    const std::string& url) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = publishers_.find(url);
  if (it != publishers_.end()) {
    return it->second;
  }

  auto publisher = std::make_shared<PublisherBinding>(url);
  int rv = nng_pub0_open(&publisher->socket);
  if (rv != 0) {
    return nullptr;
  }

  RemoveExistingIpcSocket(url);
  rv = nng_listen(publisher->socket, url.c_str(), nullptr, 0);
  if (rv != 0) {
    nng_socket_close(publisher->socket);
    return nullptr;
  }

  publishers_.emplace(url, publisher);
  return publisher;
}

core::Status NngPubSubTransport::Subscribe(std::string url, Handler handler) {
  auto valid = ValidateNngUrl(url);
  if (!valid.ok()) {
    return valid;
  }

  if (EnsurePublisher(url) == nullptr) {
    return core::Error{core::ErrorCode::kUnavailable, "Failed to create NNG pub socket for URL: " + url};
  }

  auto binding = std::make_shared<SubscriberBinding>();
  binding->url = std::move(url);
  binding->handler = std::move(handler);

  int rv = nng_sub0_open(&binding->socket);
  if (rv != 0) {
    return MakeNngError("Failed to open NNG sub socket", rv);
  }
  rv = nng_sub0_socket_subscribe(binding->socket, "", 0);
  if (rv != 0) {
    nng_socket_close(binding->socket);
    return MakeNngError("Failed to subscribe NNG sub socket", rv);
  }
  rv = nng_socket_set_ms(binding->socket, NNG_OPT_RECVTIMEO, 100);
  if (rv != 0) {
    nng_socket_close(binding->socket);
    return MakeNngError("Failed to set sub recv timeout", rv);
  }
  rv = nng_dial(binding->socket, binding->url.c_str(), nullptr, 0);
  if (rv != 0) {
    nng_socket_close(binding->socket);
    return MakeNngError("Failed to dial NNG sub socket", rv);
  }

  binding->running = true;
  binding->worker = std::thread([binding]() {
    while (binding->running.load()) {
      auto message = ReceiveEnvelope(binding->socket);
      if (!message.ok()) {
        if (message.error().message.find("Timed out") != std::string::npos ||
            message.error().message.find("Object closed") != std::string::npos) {
          continue;
        }
        break;
      }
      if (message.ok()) {
        binding->handler(message.value());
      }
    }
  });

  std::lock_guard<std::mutex> lock(mutex_);
  subscribers_.push_back(binding);
  return core::OkStatus();
}

std::size_t NngPubSubTransport::Publish(const std::string& url, const MessageEnvelope& message) const {
  if (EnsurePublisher(url) == nullptr) {
    return 0;
  }

  // Give subscriber dialers a moment to finish connecting in simple in-process tests.
  std::this_thread::sleep_for(std::chrono::milliseconds(75));

  std::lock_guard<std::mutex> lock(mutex_);
  const auto publisher_it = publishers_.find(url);
  if (publisher_it == publishers_.end()) {
    return 0;
  }
  if (!SendEnvelope(publisher_it->second->socket, message).ok()) {
    return 0;
  }
  std::size_t subscriber_count = 0;
  for (const auto& subscriber : subscribers_) {
    if (subscriber->url == url) {
      ++subscriber_count;
    }
  }
  return subscriber_count;
}

}  // namespace daffy::ipc
