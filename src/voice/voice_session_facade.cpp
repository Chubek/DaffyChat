#include "daffy/voice/voice_session_facade.hpp"

#include <utility>

namespace daffy::voice {
namespace {

class LiveVoiceSessionFacadeAdapter final : public LiveVoiceSessionFacade {
 public:
  explicit LiveVoiceSessionFacadeAdapter(LiveVoiceSession session) : session_(std::move(session)) {}

  core::Status Start() override { return session_.Start(); }
  core::Status Stop() override { return session_.Stop(); }
  core::Status StartNegotiation(std::string target_peer_id) override {
    return session_.StartNegotiation(std::move(target_peer_id));
  }
  core::Status HandleSignalingMessage(const signaling::Message& message) override {
    return session_.HandleSignalingMessage(message);
  }
  core::Status UpdateTransportConfig(const LibDatachannelPeerConfig& transport_config) override {
    return session_.UpdateTransportConfig(transport_config);
  }

  void SetSignalingMessageCallback(VoicePeerSession::SignalingMessageCallback callback) override {
    session_.SetSignalingMessageCallback(std::move(callback));
  }

  void SetStateChangeCallback(LiveVoiceSession::StateChangeCallback callback) override {
    session_.SetStateChangeCallback(std::move(callback));
  }

  [[nodiscard]] bool IsRunning() const override { return session_.IsRunning(); }
  [[nodiscard]] const VoiceSessionPlan& plan() const override { return session_.plan(); }
  [[nodiscard]] const AudioDeviceInventory& devices() const override { return session_.devices(); }
  [[nodiscard]] LiveVoiceSessionStateSnapshot state() const override { return session_.state(); }
  [[nodiscard]] LiveVoiceSessionTelemetry telemetry() const override { return session_.telemetry(); }

 private:
  LiveVoiceSession session_;
};

class VoicePeerSessionFacadeAdapter final : public VoicePeerSessionFacade {
 public:
  explicit VoicePeerSessionFacadeAdapter(VoicePeerSession session) : session_(std::move(session)) {}

  core::Status HandleSignalingMessage(const signaling::Message& message) override {
    return session_.HandleSignalingMessage(message);
  }
  core::Result<std::size_t> SendCapturedBlock(const DeviceAudioBlock& block) override {
    return session_.SendCapturedBlock(block);
  }
  core::Result<std::size_t> PumpInboundAudio() override { return session_.PumpInboundAudio(); }
  bool TryPopPlaybackBlock(DeviceAudioBlock& block) override { return session_.TryPopPlaybackBlock(block); }

  void SetSignalingMessageCallback(VoicePeerSession::SignalingMessageCallback callback) override {
    session_.SetSignalingMessageCallback(std::move(callback));
  }

  void SetStateChangeCallback(VoicePeerSession::StateChangeCallback callback) override {
    session_.SetStateChangeCallback(std::move(callback));
  }

  [[nodiscard]] const VoiceSessionPlan& plan() const override { return session_.plan(); }
  [[nodiscard]] bool IsReady() const override { return session_.IsReady(); }
  [[nodiscard]] VoicePeerSessionStateSnapshot state() const override { return session_.state(); }
  [[nodiscard]] VoicePeerSessionTelemetry telemetry() const override { return session_.telemetry(); }

 private:
  VoicePeerSession session_;
};

}  // namespace

core::Result<std::unique_ptr<LiveVoiceSessionFacade>> CreateDefaultLiveVoiceSessionFacade(
    const LiveVoiceSessionConfig& config) {
  auto session = LiveVoiceSession::Create(config);
  if (!session.ok()) {
    return session.error();
  }
  std::unique_ptr<LiveVoiceSessionFacade> facade =
      std::make_unique<LiveVoiceSessionFacadeAdapter>(std::move(session.value()));
  return facade;
}

core::Result<std::unique_ptr<VoicePeerSessionFacade>> CreateDefaultVoicePeerSessionFacade(
    const VoicePeerSessionConfig& config) {
  auto session = VoicePeerSession::Create(config);
  if (!session.ok()) {
    return session.error();
  }
  std::unique_ptr<VoicePeerSessionFacade> facade =
      std::make_unique<VoicePeerSessionFacadeAdapter>(std::move(session.value()));
  return facade;
}

}  // namespace daffy::voice
