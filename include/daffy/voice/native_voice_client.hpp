#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "daffy/core/error.hpp"
#include "daffy/signaling/signaling_client_facade.hpp"
#include "daffy/signaling/native_signaling_client.hpp"
#include "daffy/util/json.hpp"
#include "daffy/voice/live_voice_session.hpp"
#include "daffy/voice/voice_session_facade.hpp"

namespace daffy::voice {

struct NativeVoiceClientConfig {
  LiveVoiceSessionConfig live_config{};
  signaling::NativeSignalingClientConfig signaling_config{};
  bool auto_offer{true};
  LiveVoiceSessionFactory live_factory{};
  signaling::SignalingClientFactory signaling_factory{};
};

struct NativeVoiceClientStateSnapshot {
  bool running{false};
  bool negotiation_started{false};
  std::uint64_t negotiation_resets{0};
  std::uint64_t negotiation_reoffers{0};
  std::string last_negotiation_reset_reason;
  std::string last_reoffer_reason;
  std::string last_error;
  LiveVoiceSessionStateSnapshot live{};
  signaling::NativeSignalingClientStateSnapshot signaling{};
};

struct NativeVoiceClientTelemetry {
  LiveVoiceSessionTelemetry live{};
  signaling::NativeSignalingClientTelemetry signaling{};
  std::uint64_t negotiation_resets{0};
  std::uint64_t negotiation_reoffers{0};
  std::string last_negotiation_reset_reason;
  std::string last_reoffer_reason;
};

util::json::Value NativeVoiceClientStateToJson(const NativeVoiceClientStateSnapshot& state);
util::json::Value NativeVoiceClientTelemetryToJson(const NativeVoiceClientTelemetry& telemetry);

class NativeVoiceClient {
 public:
  using StateChangeCallback = std::function<void(const NativeVoiceClientStateSnapshot& state)>;

  struct Impl;

  NativeVoiceClient();
  NativeVoiceClient(NativeVoiceClient&& other) noexcept;
  NativeVoiceClient& operator=(NativeVoiceClient&& other) noexcept;
  NativeVoiceClient(const NativeVoiceClient&) = delete;
  NativeVoiceClient& operator=(const NativeVoiceClient&) = delete;
  ~NativeVoiceClient();

  static core::Result<NativeVoiceClient> Create(const NativeVoiceClientConfig& config);

  core::Status Start();
  core::Status Stop();

  void SetStateChangeCallback(StateChangeCallback callback);

  [[nodiscard]] bool IsRunning() const;
  [[nodiscard]] NativeVoiceClientStateSnapshot state() const;
  [[nodiscard]] NativeVoiceClientTelemetry telemetry() const;
  [[nodiscard]] const VoiceSessionPlan& plan() const;
  [[nodiscard]] const AudioDeviceInventory& devices() const;

 private:
  explicit NativeVoiceClient(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace daffy::voice
