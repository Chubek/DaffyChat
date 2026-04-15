#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/signaling/messages.hpp"
#include "daffy/util/json.hpp"

namespace daffy::signaling {

struct NativeSignalingClientConfig {
  using TurnCredentialsFetcher = std::function<core::Result<std::string>(const std::string& url)>;

  std::string websocket_url;
  std::string room;
  std::string peer_id;
  int connection_timeout_ms{5000};
  int ping_interval_ms{15000};
  int max_outstanding_pings{2};
  int turn_fetch_max_attempts{3};
  int turn_fetch_retry_delay_ms{100};
  TurnCredentialsFetcher turn_credentials_fetcher{};
};

struct NativeSignalingClientStateSnapshot {
  std::string websocket_url;
  std::string room;
  std::string peer_id;
  bool started{false};
  bool websocket_open{false};
  bool joined{false};
  bool join_sent{false};
  bool has_turn_credentials{false};
  std::size_t discovered_ice_servers{0};
  std::string turn_credentials_url;
  std::string last_turn_fetch_error;
  std::string last_error;
};

struct NativeSignalingIceBootstrap {
  std::vector<std::string> stun_servers;
  std::string turn_credentials_endpoint;
  std::string turn_credentials_url;
  std::string turn_uri;
  std::string turn_username;
  std::string turn_password;
  bool turn_from_join_ack{false};
  bool turn_from_endpoint{false};
};

struct NativeSignalingClientTelemetry {
  std::uint64_t outbound_messages{0};
  std::uint64_t inbound_messages{0};
  std::uint64_t parse_errors{0};
  std::uint64_t transport_errors{0};
  std::uint64_t turn_fetches{0};
  std::uint64_t turn_fetch_retry_attempts{0};
  std::uint64_t turn_refreshes{0};
  std::uint64_t turn_fetch_successes{0};
  std::uint64_t turn_fetch_failures{0};
};

util::json::Value NativeSignalingClientStateToJson(const NativeSignalingClientStateSnapshot& state);
util::json::Value NativeSignalingIceBootstrapToJson(const NativeSignalingIceBootstrap& bootstrap);
util::json::Value NativeSignalingClientTelemetryToJson(const NativeSignalingClientTelemetry& telemetry);

class NativeSignalingClient {
 public:
  using MessageCallback = std::function<void(const Message& message)>;
  using StateChangeCallback = std::function<void(const NativeSignalingClientStateSnapshot& state)>;

  struct Impl;

  NativeSignalingClient();
  NativeSignalingClient(NativeSignalingClient&& other) noexcept;
  NativeSignalingClient& operator=(NativeSignalingClient&& other) noexcept;
  NativeSignalingClient(const NativeSignalingClient&) = delete;
  NativeSignalingClient& operator=(const NativeSignalingClient&) = delete;
  ~NativeSignalingClient();

  static core::Result<NativeSignalingClient> Create(const NativeSignalingClientConfig& config);

  core::Status Start();
  core::Status Stop();
  core::Status Send(const Message& message);

  void SetMessageCallback(MessageCallback callback);
  void SetStateChangeCallback(StateChangeCallback callback);

  [[nodiscard]] bool IsStarted() const;
  [[nodiscard]] NativeSignalingClientStateSnapshot state() const;
  [[nodiscard]] NativeSignalingClientTelemetry telemetry() const;
  [[nodiscard]] NativeSignalingIceBootstrap bootstrap() const;

 private:
  explicit NativeSignalingClient(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace daffy::signaling
