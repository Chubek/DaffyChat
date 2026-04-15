#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "daffy/core/error.hpp"
#include "daffy/signaling/messages.hpp"
#include "daffy/voice/portaudio_runtime.hpp"
#include "daffy/voice/voice_peer_session.hpp"
#include "daffy/util/json.hpp"

namespace daffy::voice {

struct LiveVoiceSessionConfig {
  VoicePeerSessionConfig peer_config{};
  int pump_interval_ms{10};
};

struct LiveVoiceSessionStateSnapshot {
  bool stream_open{false};
  bool stream_started{false};
  bool running{false};
  std::string last_error;
  VoicePeerSessionStateSnapshot peer{};
};

struct LiveVoiceSessionTelemetry {
  VoicePeerSessionTelemetry peer{};
  PortAudioCallbackBridgeStats stream{};
  std::uint64_t capture_blocks_dropped_while_disconnected{0};
  std::uint64_t pump_iterations{0};
};

util::json::Value LiveVoiceSessionStateToJson(const LiveVoiceSessionStateSnapshot& state);
util::json::Value LiveVoiceSessionTelemetryToJson(const LiveVoiceSessionTelemetry& telemetry);

class LiveVoiceSession {
 public:
  using StateChangeCallback = std::function<void(const LiveVoiceSessionStateSnapshot& state)>;

  struct Impl;

  LiveVoiceSession();
  LiveVoiceSession(LiveVoiceSession&& other) noexcept;
  LiveVoiceSession& operator=(LiveVoiceSession&& other) noexcept;
  LiveVoiceSession(const LiveVoiceSession&) = delete;
  LiveVoiceSession& operator=(const LiveVoiceSession&) = delete;
  ~LiveVoiceSession();

  static core::Result<LiveVoiceSession> Create(const LiveVoiceSessionConfig& config);

  core::Status Start();
  core::Status Stop();
  core::Status StartNegotiation(std::string target_peer_id = {});
  core::Status HandleSignalingMessage(const signaling::Message& message);
  core::Status UpdateTransportConfig(const LibDatachannelPeerConfig& transport_config);

  void SetSignalingMessageCallback(VoicePeerSession::SignalingMessageCallback callback);
  void SetStateChangeCallback(StateChangeCallback callback);

  [[nodiscard]] bool IsRunning() const;
  [[nodiscard]] const VoiceSessionPlan& plan() const;
  [[nodiscard]] const AudioDeviceInventory& devices() const;
  [[nodiscard]] LiveVoiceSessionStateSnapshot state() const;
  [[nodiscard]] LiveVoiceSessionTelemetry telemetry() const;

 private:
  explicit LiveVoiceSession(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace daffy::voice
