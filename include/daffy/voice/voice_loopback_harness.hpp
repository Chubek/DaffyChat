#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "daffy/core/error.hpp"
#include "daffy/voice/live_voice_session.hpp"
#include "daffy/voice/voice_peer_session.hpp"
#include "daffy/voice/voice_session_facade.hpp"

namespace daffy::voice {

struct VoiceLoopbackHarnessConfig {
  LiveVoiceSessionConfig live_config{};
  std::string echo_peer_id{"peer-echo"};
  int pump_interval_ms{10};
  LiveVoiceSessionFactory live_factory{};
  VoicePeerSessionFactory echo_factory{};
};

struct VoiceLoopbackHarnessStateSnapshot {
  bool running{false};
  bool negotiation_started{false};
  std::string last_error;
  LiveVoiceSessionStateSnapshot live{};
  VoicePeerSessionStateSnapshot echo{};
};

struct VoiceLoopbackHarnessTelemetry {
  LiveVoiceSessionTelemetry live{};
  VoicePeerSessionTelemetry echo{};
  std::uint64_t echo_blocks_received{0};
  std::uint64_t echo_blocks_returned{0};
  std::uint64_t pump_iterations{0};
};

class VoiceLoopbackHarness {
 public:
  using StateChangeCallback = std::function<void(const VoiceLoopbackHarnessStateSnapshot& state)>;

  struct Impl;

  VoiceLoopbackHarness();
  VoiceLoopbackHarness(VoiceLoopbackHarness&& other) noexcept;
  VoiceLoopbackHarness& operator=(VoiceLoopbackHarness&& other) noexcept;
  VoiceLoopbackHarness(const VoiceLoopbackHarness&) = delete;
  VoiceLoopbackHarness& operator=(const VoiceLoopbackHarness&) = delete;
  ~VoiceLoopbackHarness();

  static core::Result<VoiceLoopbackHarness> Create(const VoiceLoopbackHarnessConfig& config);

  core::Status Start();
  core::Status Stop();

  void SetStateChangeCallback(StateChangeCallback callback);

  [[nodiscard]] bool IsRunning() const;
  [[nodiscard]] VoiceLoopbackHarnessStateSnapshot state() const;
  [[nodiscard]] VoiceLoopbackHarnessTelemetry telemetry() const;
  [[nodiscard]] const VoiceSessionPlan& plan() const;
  [[nodiscard]] const AudioDeviceInventory& devices() const;

 private:
  explicit VoiceLoopbackHarness(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace daffy::voice
