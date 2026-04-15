#pragma once

#include <functional>
#include <memory>

#include "daffy/core/error.hpp"
#include "daffy/voice/live_voice_session.hpp"
#include "daffy/voice/voice_peer_session.hpp"

namespace daffy::voice {

class LiveVoiceSessionFacade {
 public:
  virtual ~LiveVoiceSessionFacade() = default;

  virtual core::Status Start() = 0;
  virtual core::Status Stop() = 0;
  virtual core::Status StartNegotiation(std::string target_peer_id) = 0;
  virtual core::Status HandleSignalingMessage(const signaling::Message& message) = 0;
  virtual core::Status UpdateTransportConfig(const LibDatachannelPeerConfig& transport_config) = 0;

  virtual void SetSignalingMessageCallback(VoicePeerSession::SignalingMessageCallback callback) = 0;
  virtual void SetStateChangeCallback(LiveVoiceSession::StateChangeCallback callback) = 0;

  [[nodiscard]] virtual bool IsRunning() const = 0;
  [[nodiscard]] virtual const VoiceSessionPlan& plan() const = 0;
  [[nodiscard]] virtual const AudioDeviceInventory& devices() const = 0;
  [[nodiscard]] virtual LiveVoiceSessionStateSnapshot state() const = 0;
  [[nodiscard]] virtual LiveVoiceSessionTelemetry telemetry() const = 0;
};

class VoicePeerSessionFacade {
 public:
  virtual ~VoicePeerSessionFacade() = default;

  virtual core::Status HandleSignalingMessage(const signaling::Message& message) = 0;
  virtual core::Result<std::size_t> SendCapturedBlock(const DeviceAudioBlock& block) = 0;
  virtual core::Result<std::size_t> PumpInboundAudio() = 0;
  virtual bool TryPopPlaybackBlock(DeviceAudioBlock& block) = 0;

  virtual void SetSignalingMessageCallback(VoicePeerSession::SignalingMessageCallback callback) = 0;
  virtual void SetStateChangeCallback(VoicePeerSession::StateChangeCallback callback) = 0;

  [[nodiscard]] virtual const VoiceSessionPlan& plan() const = 0;
  [[nodiscard]] virtual bool IsReady() const = 0;
  [[nodiscard]] virtual VoicePeerSessionStateSnapshot state() const = 0;
  [[nodiscard]] virtual VoicePeerSessionTelemetry telemetry() const = 0;
};

using LiveVoiceSessionFactory =
    std::function<core::Result<std::unique_ptr<LiveVoiceSessionFacade>>(const LiveVoiceSessionConfig& config)>;
using VoicePeerSessionFactory =
    std::function<core::Result<std::unique_ptr<VoicePeerSessionFacade>>(const VoicePeerSessionConfig& config)>;

core::Result<std::unique_ptr<LiveVoiceSessionFacade>> CreateDefaultLiveVoiceSessionFacade(
    const LiveVoiceSessionConfig& config);
core::Result<std::unique_ptr<VoicePeerSessionFacade>> CreateDefaultVoicePeerSessionFacade(
    const VoicePeerSessionConfig& config);

}  // namespace daffy::voice
