#include "daffy/signaling/native_signaling_client.hpp"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <chrono>
#include <thread>
#include <mutex>
#include <sstream>
#include <utility>

#include <rtc/rtc.hpp>
#include <cstring>

namespace daffy::signaling {
namespace {

core::Error BuildSignalingClientError(const std::string& message) {
  return core::Error{core::ErrorCode::kStateError, message};
}

Message BuildJoinMessage(const NativeSignalingClientConfig& config) {
  Message message;
  message.type = MessageType::kJoin;
  message.room = config.room;
  message.peer_id = config.peer_id;
  return message;
}

Message BuildLeaveMessage(const NativeSignalingClientConfig& config) {
  Message message;
  message.type = MessageType::kLeave;
  message.room = config.room;
  message.peer_id = config.peer_id;
  return message;
}

bool IsUnreservedUrlCharacter(const char character) {
  return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
         (character >= '0' && character <= '9') || character == '-' || character == '_' || character == '.' ||
         character == '~';
}

std::string UrlEncode(std::string_view input) {
  static constexpr char kHexDigits[] = "0123456789ABCDEF";
  std::ostringstream stream;
  for (const auto character : input) {
    if (IsUnreservedUrlCharacter(character)) {
      stream << character;
      continue;
    }
    const auto value = static_cast<unsigned char>(character);
    stream << '%' << kHexDigits[(value >> 4) & 0x0F] << kHexDigits[value & 0x0F];
  }
  return stream.str();
}

struct ParsedUrl {
  std::string scheme;
  std::string host;
  std::string port;
  std::string target;
};

core::Result<ParsedUrl> ParseUrl(const std::string& url) {
  const auto scheme_end = url.find("://");
  if (scheme_end == std::string::npos) {
    return core::Error{core::ErrorCode::kInvalidArgument, "URL is missing a scheme: " + url};
  }

  ParsedUrl parsed;
  parsed.scheme = url.substr(0, scheme_end);
  std::string remainder = url.substr(scheme_end + 3);
  const auto path_start = remainder.find('/');
  std::string authority = path_start == std::string::npos ? remainder : remainder.substr(0, path_start);
  parsed.target = path_start == std::string::npos ? "/" : remainder.substr(path_start);

  const auto port_separator = authority.rfind(':');
  if (port_separator != std::string::npos && authority.find(']') == std::string::npos) {
    parsed.host = authority.substr(0, port_separator);
    parsed.port = authority.substr(port_separator + 1);
  } else {
    parsed.host = authority;
    parsed.port = parsed.scheme == "https" || parsed.scheme == "wss" ? "443" : "80";
  }

  if (parsed.host.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "URL is missing a host: " + url};
  }
  if (parsed.target.empty()) {
    parsed.target = "/";
  }
  return parsed;
}

std::string ResolveHttpEndpointUrl(const std::string& websocket_url, const std::string& endpoint) {
  if (endpoint.empty()) {
    return {};
  }
  if (endpoint.rfind("http://", 0) == 0 || endpoint.rfind("https://", 0) == 0) {
    return endpoint;
  }

  auto parsed_base = ParseUrl(websocket_url);
  if (!parsed_base.ok()) {
    return {};
  }

  const auto& base = parsed_base.value();
  const std::string scheme = base.scheme == "wss" ? "https" : "http";
  const std::string normalized_endpoint = endpoint.front() == '/' ? endpoint : '/' + endpoint;
  return scheme + "://" + base.host + ':' + base.port + normalized_endpoint;
}

core::Result<std::string> HttpGet(const std::string& url) {
  auto parsed = ParseUrl(url);
  if (!parsed.ok()) {
    return parsed.error();
  }
  if (parsed.value().scheme != "http") {
    return core::Error{core::ErrorCode::kUnavailable, "Only http TURN credential endpoints are supported: " + url};
  }

  addrinfo hints {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* results = nullptr;
  if (getaddrinfo(parsed.value().host.c_str(), parsed.value().port.c_str(), &hints, &results) != 0) {
    return core::Error{core::ErrorCode::kUnavailable, "Failed to resolve TURN endpoint host: " + parsed.value().host};
  }

  int fd = -1;
  for (addrinfo* current = results; current != nullptr; current = current->ai_next) {
    fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
    if (fd < 0) {
      continue;
    }

    timeval timeout {};
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    if (connect(fd, current->ai_addr, current->ai_addrlen) == 0) {
      break;
    }
    close(fd);
    fd = -1;
  }
  freeaddrinfo(results);

  if (fd < 0) {
    return core::Error{core::ErrorCode::kUnavailable, "Failed to connect to TURN endpoint: " + url};
  }

  const std::string request = "GET " + parsed.value().target + " HTTP/1.1\r\nHost: " + parsed.value().host +
                              "\r\nConnection: close\r\n\r\n";
  std::size_t sent = 0;
  while (sent < request.size()) {
    const auto bytes = send(fd, request.data() + sent, request.size() - sent, 0);
    if (bytes <= 0) {
      close(fd);
      return core::Error{core::ErrorCode::kUnavailable, "Failed to write TURN endpoint request"};
    }
    sent += static_cast<std::size_t>(bytes);
  }

  std::string response;
  char buffer[1024];
  while (true) {
    const auto bytes = recv(fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0) {
      break;
    }
    response.append(buffer, buffer + bytes);
  }
  close(fd);

  const auto header_end = response.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return core::Error{core::ErrorCode::kParseError, "TURN endpoint returned a malformed HTTP response"};
  }

  const auto status_end = response.find("\r\n");
  if (status_end == std::string::npos || response.find(" 200 ", 0) == std::string::npos) {
    return core::Error{core::ErrorCode::kUnavailable, "TURN endpoint returned a non-success HTTP status"};
  }

  return response.substr(header_end + 4);
}

core::Status HydrateTurnCredentialsFromObject(const util::json::Value& object,
                                              NativeSignalingIceBootstrap& bootstrap) {
  const auto* uri = object.Find("uri");
  const auto* username = object.Find("username");
  const auto* password = object.Find("password");
  if (uri == nullptr || username == nullptr || password == nullptr || !uri->IsString() || !username->IsString() ||
      !password->IsString()) {
    return core::Error{core::ErrorCode::kParseError, "TURN payload is missing uri/username/password fields"};
  }

  bootstrap.turn_uri = uri->AsString();
  bootstrap.turn_username = username->AsString();
  bootstrap.turn_password = password->AsString();
  return core::OkStatus();
}

}  // namespace

util::json::Value NativeSignalingClientStateToJson(const NativeSignalingClientStateSnapshot& state) {
  return util::json::Value::Object{{"websocket_url", state.websocket_url},
                                   {"room", state.room},
                                   {"peer_id", state.peer_id},
                                   {"started", state.started},
                                   {"websocket_open", state.websocket_open},
                                   {"joined", state.joined},
                                   {"join_sent", state.join_sent},
                                   {"has_turn_credentials", state.has_turn_credentials},
                                   {"discovered_ice_servers", static_cast<int>(state.discovered_ice_servers)},
                                   {"turn_credentials_url", state.turn_credentials_url},
                                   {"last_turn_fetch_error", state.last_turn_fetch_error},
                                   {"last_error", state.last_error}};
}

util::json::Value NativeSignalingIceBootstrapToJson(const NativeSignalingIceBootstrap& bootstrap) {
  util::json::Value::Array stun_servers;
  for (const auto& server : bootstrap.stun_servers) {
    stun_servers.emplace_back(server);
  }
  return util::json::Value::Object{{"stun_servers", stun_servers},
                                   {"turn_credentials_endpoint", bootstrap.turn_credentials_endpoint},
                                   {"turn_credentials_url", bootstrap.turn_credentials_url},
                                   {"turn_uri", bootstrap.turn_uri},
                                   {"turn_username", bootstrap.turn_username},
                                   {"turn_password", bootstrap.turn_password},
                                   {"turn_from_join_ack", bootstrap.turn_from_join_ack},
                                   {"turn_from_endpoint", bootstrap.turn_from_endpoint}};
}

util::json::Value NativeSignalingClientTelemetryToJson(const NativeSignalingClientTelemetry& telemetry) {
  return util::json::Value::Object{{"outbound_messages", static_cast<int>(telemetry.outbound_messages)},
                                   {"inbound_messages", static_cast<int>(telemetry.inbound_messages)},
                                   {"parse_errors", static_cast<int>(telemetry.parse_errors)},
                                   {"transport_errors", static_cast<int>(telemetry.transport_errors)},
                                   {"turn_fetches", static_cast<int>(telemetry.turn_fetches)},
                                   {"turn_fetch_retry_attempts", static_cast<int>(telemetry.turn_fetch_retry_attempts)},
                                   {"turn_refreshes", static_cast<int>(telemetry.turn_refreshes)},
                                   {"turn_fetch_successes", static_cast<int>(telemetry.turn_fetch_successes)},
                                   {"turn_fetch_failures", static_cast<int>(telemetry.turn_fetch_failures)}};
}

struct NativeSignalingClient::Impl {
  explicit Impl(NativeSignalingClientConfig client_config) : config(std::move(client_config)) {
    state_snapshot.websocket_url = config.websocket_url;
    state_snapshot.room = config.room;
    state_snapshot.peer_id = config.peer_id;
  }

  void EmitState() {
    StateChangeCallback callback;
    NativeSignalingClientStateSnapshot snapshot_copy;
    {
      std::lock_guard<std::mutex> lock(mutex);
      callback = state_callback;
      snapshot_copy = state_snapshot;
    }
    if (callback) {
      callback(snapshot_copy);
    }
  }

  void SetLastError(std::string error) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      state_snapshot.last_error = std::move(error);
      ++telemetry_snapshot.transport_errors;
    }
    EmitState();
  }

  core::Result<std::string> FetchTurnCredentialsWithRetry(const std::string& url, const bool refresh) {
    const auto fetcher = config.turn_credentials_fetcher ? config.turn_credentials_fetcher : HttpGet;
    const int attempts = std::max(1, config.turn_fetch_max_attempts);
    core::Error last_error{core::ErrorCode::kUnavailable, "TURN endpoint fetch did not run"};

    {
      std::lock_guard<std::mutex> lock(mutex);
      ++telemetry_snapshot.turn_fetches;
      if (refresh) {
        ++telemetry_snapshot.turn_refreshes;
      }
      state_snapshot.last_turn_fetch_error.clear();
    }

    for (int attempt = 1; attempt <= attempts; ++attempt) {
      auto response = fetcher(url);
      if (response.ok()) {
        std::lock_guard<std::mutex> lock(mutex);
        ++telemetry_snapshot.turn_fetch_successes;
        state_snapshot.last_turn_fetch_error.clear();
        return response;
      }

      last_error = response.error();
      {
        std::lock_guard<std::mutex> lock(mutex);
        state_snapshot.last_turn_fetch_error = last_error.ToString();
        if (attempt < attempts) {
          ++telemetry_snapshot.turn_fetch_retry_attempts;
        }
      }

      if (attempt < attempts && config.turn_fetch_retry_delay_ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config.turn_fetch_retry_delay_ms));
      }
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      ++telemetry_snapshot.turn_fetch_failures;
    }
    return last_error;
  }

  core::Status SendInternal(const Message& message) {
    std::shared_ptr<rtc::WebSocket> socket_copy;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (!state_snapshot.started || socket == nullptr) {
        return BuildSignalingClientError("Native signaling client is not started");
      }
      if (!state_snapshot.websocket_open) {
        return BuildSignalingClientError("Native signaling websocket is not open");
      }
      socket_copy = socket;
    }

    if (message.type == MessageType::kJoin || message.type == MessageType::kLeave ||
        message.type == MessageType::kOffer || message.type == MessageType::kAnswer ||
        message.type == MessageType::kIceCandidate) {
      socket_copy->send(SerializeMessage(message));
    } else {
      return core::Error{core::ErrorCode::kInvalidArgument, "Cannot send server-originated signaling message type"};
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      ++telemetry_snapshot.outbound_messages;
      if (message.type == MessageType::kJoin) {
        state_snapshot.join_sent = true;
      }
      if (message.type == MessageType::kLeave) {
        state_snapshot.joined = false;
      }
    }
    EmitState();
    return core::OkStatus();
  }

  void OnOpen() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      state_snapshot.websocket_open = true;
      state_snapshot.last_error.clear();
    }
    EmitState();

    const auto joined = SendInternal(BuildJoinMessage(config));
    if (!joined.ok()) {
      SetLastError(joined.error().ToString());
    }
  }

  void OnClosed() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      state_snapshot.websocket_open = false;
      state_snapshot.joined = false;
    }
    EmitState();
  }

  void OnError(std::string error) { SetLastError(std::move(error)); }

  void OnMessage(const std::string& payload) {
    auto parsed = ParseMessage(payload);
    if (!parsed.ok()) {
      {
        std::lock_guard<std::mutex> lock(mutex);
        ++telemetry_snapshot.parse_errors;
        state_snapshot.last_error = parsed.error().ToString();
      }
      EmitState();
      return;
    }

    Message message = parsed.value();
    MessageCallback callback;
    {
      std::lock_guard<std::mutex> lock(mutex);
      ++telemetry_snapshot.inbound_messages;
      if (message.type == MessageType::kJoinAck) {
        state_snapshot.joined = true;
      }
      callback = message_callback;
    }

    if (message.type == MessageType::kJoinAck) {
      auto hydrated = HydrateBootstrap(message);
      if (!hydrated.ok()) {
        SetLastError(hydrated.error().ToString());
      }
    }
    EmitState();

    if (callback) {
      callback(message);
    }
  }

  core::Status HydrateBootstrap(const Message& message) {
    NativeSignalingIceBootstrap updated;
    NativeSignalingIceBootstrap previous_bootstrap;
    {
      std::lock_guard<std::mutex> lock(mutex);
      previous_bootstrap = bootstrap;
    }
    if (const auto* stun_servers = message.data.Find("stun_servers"); stun_servers != nullptr && stun_servers->IsArray()) {
      for (const auto& entry : stun_servers->AsArray()) {
        if (entry.IsString()) {
          updated.stun_servers.push_back(entry.AsString());
        }
      }
    }
    if (const auto* endpoints = message.data.Find("endpoints"); endpoints != nullptr && endpoints->IsObject()) {
      if (const auto* turn_endpoint = endpoints->Find("turn"); turn_endpoint != nullptr && turn_endpoint->IsString()) {
        updated.turn_credentials_endpoint = turn_endpoint->AsString();
        updated.turn_credentials_url = ResolveHttpEndpointUrl(config.websocket_url, updated.turn_credentials_endpoint);
      }
    }
    if (const auto* turn = message.data.Find("turn"); turn != nullptr && turn->IsObject()) {
      auto hydrated = HydrateTurnCredentialsFromObject(*turn, updated);
      if (!hydrated.ok()) {
        return hydrated;
      }
      updated.turn_from_join_ack = !updated.turn_uri.empty();
    } else if (!updated.turn_credentials_url.empty()) {
      std::string target = updated.turn_credentials_url;
      target += target.find('?') == std::string::npos ? '?' : '&';
      target += "room=" + UrlEncode(config.room) + "&peer_id=" + UrlEncode(config.peer_id);
      const bool refresh = !previous_bootstrap.turn_uri.empty();
      auto response = FetchTurnCredentialsWithRetry(target, refresh);
      if (!response.ok()) {
        if (!previous_bootstrap.turn_uri.empty()) {
          updated.turn_uri = previous_bootstrap.turn_uri;
          updated.turn_username = previous_bootstrap.turn_username;
          updated.turn_password = previous_bootstrap.turn_password;
          updated.turn_from_endpoint = previous_bootstrap.turn_from_endpoint;
          updated.turn_from_join_ack = previous_bootstrap.turn_from_join_ack;
        } else {
          return response.error();
        }
      } else {
        auto payload = util::json::Parse(response.value());
        if (!payload.ok() || !payload.value().IsObject()) {
          {
            std::lock_guard<std::mutex> lock(mutex);
            ++telemetry_snapshot.turn_fetch_failures;
            state_snapshot.last_turn_fetch_error = "TURN endpoint returned invalid JSON credentials";
          }
          if (previous_bootstrap.turn_uri.empty()) {
            return core::Error{core::ErrorCode::kParseError, "TURN endpoint returned invalid JSON credentials"};
          }
        } else {
          auto hydrated = HydrateTurnCredentialsFromObject(payload.value(), updated);
          if (!hydrated.ok()) {
            {
              std::lock_guard<std::mutex> lock(mutex);
              ++telemetry_snapshot.turn_fetch_failures;
              state_snapshot.last_turn_fetch_error = hydrated.error().ToString();
            }
            if (previous_bootstrap.turn_uri.empty()) {
              return hydrated;
            }
          } else {
            updated.turn_from_endpoint = !updated.turn_uri.empty();
          }
        }
      }
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      bootstrap = std::move(updated);
      state_snapshot.has_turn_credentials = !bootstrap.turn_uri.empty();
      state_snapshot.discovered_ice_servers =
          bootstrap.stun_servers.size() + (bootstrap.turn_uri.empty() ? std::size_t{0} : std::size_t{1});
      state_snapshot.turn_credentials_url = bootstrap.turn_credentials_url;
      if (!state_snapshot.has_turn_credentials) {
        state_snapshot.last_turn_fetch_error.clear();
      }
    }
    return core::OkStatus();
  }

  NativeSignalingClientConfig config;
  mutable std::mutex mutex;
  std::shared_ptr<rtc::WebSocket> socket;
  MessageCallback message_callback;
  StateChangeCallback state_callback;
  NativeSignalingClientStateSnapshot state_snapshot{};
  NativeSignalingIceBootstrap bootstrap{};
  NativeSignalingClientTelemetry telemetry_snapshot{};
};

NativeSignalingClient::NativeSignalingClient() = default;

NativeSignalingClient::NativeSignalingClient(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

NativeSignalingClient::NativeSignalingClient(NativeSignalingClient&& other) noexcept = default;

NativeSignalingClient& NativeSignalingClient::operator=(NativeSignalingClient&& other) noexcept = default;

NativeSignalingClient::~NativeSignalingClient() {
  if (impl_ != nullptr) {
    static_cast<void>(Stop());
  }
}

core::Result<NativeSignalingClient> NativeSignalingClient::Create(const NativeSignalingClientConfig& config) {
  if (config.websocket_url.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Native signaling client requires a websocket URL"};
  }
  if (config.room.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Native signaling client requires a room id"};
  }
  if (config.peer_id.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Native signaling client requires a peer id"};
  }
  if (config.connection_timeout_ms < 0 || config.ping_interval_ms < 0 || config.max_outstanding_pings < 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Native signaling client timing values must be non-negative"};
  }
  if (config.turn_fetch_max_attempts <= 0 || config.turn_fetch_retry_delay_ms < 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "TURN fetch retry values must be positive"};
  }

  return NativeSignalingClient(std::make_unique<Impl>(config));
}

core::Status NativeSignalingClient::Start() {
  if (impl_ == nullptr) {
    return BuildSignalingClientError("Native signaling client is not initialized");
  }

  std::shared_ptr<rtc::WebSocket> socket;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->state_snapshot.started) {
      return core::OkStatus();
    }

    rtc::WebSocket::Configuration ws_config;
    if (impl_->config.connection_timeout_ms > 0) {
      ws_config.connectionTimeout = std::chrono::milliseconds(impl_->config.connection_timeout_ms);
    }
    if (impl_->config.ping_interval_ms > 0) {
      ws_config.pingInterval = std::chrono::milliseconds(impl_->config.ping_interval_ms);
    }
    if (impl_->config.max_outstanding_pings > 0) {
      ws_config.maxOutstandingPings = impl_->config.max_outstanding_pings;
    }

    socket = std::make_shared<rtc::WebSocket>(ws_config);
    impl_->socket = socket;
    impl_->state_snapshot.started = true;
    impl_->state_snapshot.websocket_open = false;
    impl_->state_snapshot.joined = false;
    impl_->state_snapshot.join_sent = false;
    impl_->state_snapshot.last_turn_fetch_error.clear();
    impl_->state_snapshot.last_error.clear();
  }

  socket->onOpen([ptr = impl_.get()]() { ptr->OnOpen(); });
  socket->onClosed([ptr = impl_.get()]() { ptr->OnClosed(); });
  socket->onError([ptr = impl_.get()](std::string error) { ptr->OnError(std::move(error)); });
  socket->onMessage(nullptr, [ptr = impl_.get()](std::string payload) { ptr->OnMessage(payload); });
  socket->open(impl_->config.websocket_url);
  impl_->EmitState();
  return core::OkStatus();
}

core::Status NativeSignalingClient::Stop() {
  if (impl_ == nullptr) {
    return BuildSignalingClientError("Native signaling client is not initialized");
  }

  std::shared_ptr<rtc::WebSocket> socket;
  bool should_leave = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (!impl_->state_snapshot.started) {
      return core::OkStatus();
    }
    socket = impl_->socket;
    should_leave = impl_->state_snapshot.websocket_open && impl_->state_snapshot.joined;
    impl_->state_snapshot.started = false;
    impl_->state_snapshot.joined = false;
    impl_->state_snapshot.join_sent = false;
    impl_->state_snapshot.websocket_open = false;
    impl_->state_snapshot.last_turn_fetch_error.clear();
    impl_->socket.reset();
  }

  if (should_leave && socket != nullptr) {
    socket->send(SerializeMessage(BuildLeaveMessage(impl_->config)));
  }
  if (socket != nullptr) {
    socket->close();
  }

  impl_->EmitState();
  return core::OkStatus();
}

core::Status NativeSignalingClient::Send(const Message& message) {
  if (impl_ == nullptr) {
    return BuildSignalingClientError("Native signaling client is not initialized");
  }
  return impl_->SendInternal(message);
}

void NativeSignalingClient::SetMessageCallback(MessageCallback callback) {
  if (impl_ == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->message_callback = std::move(callback);
}

void NativeSignalingClient::SetStateChangeCallback(StateChangeCallback callback) {
  if (impl_ == nullptr) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state_callback = std::move(callback);
  }
  impl_->EmitState();
}

bool NativeSignalingClient::IsStarted() const {
  if (impl_ == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->state_snapshot.started;
}

NativeSignalingClientStateSnapshot NativeSignalingClient::state() const {
  if (impl_ == nullptr) {
    return {};
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->state_snapshot;
}

NativeSignalingClientTelemetry NativeSignalingClient::telemetry() const {
  if (impl_ == nullptr) {
    return {};
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->telemetry_snapshot;
}

NativeSignalingIceBootstrap NativeSignalingClient::bootstrap() const {
  if (impl_ == nullptr) {
    return {};
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->bootstrap;
}

}  // namespace daffy::signaling
