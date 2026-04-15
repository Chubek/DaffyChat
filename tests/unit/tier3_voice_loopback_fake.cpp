#include <cassert>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

#include "daffy/voice/voice_loopback_harness.hpp"

namespace {

bool WaitUntil(const std::function<bool()>& predicate, const int attempts = 100, const int sleep_ms = 10) {
  for (int attempt = 0; attempt < attempts; ++attempt) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  }
  return false;
}

daffy::voice::VoiceSessionPlan BuildPlan() {
  daffy::voice::VoiceSessionPlan plan;
  plan.input_device = {0, "Fake Mic", "test", 1, 0, 48000.0, 0.01, 0.0, true, false};
  plan.output_device = {1, "Fake Speaker", "test", 0, 1, 48000.0, 0.0, 0.01, false, true};
  plan.capture_plan.device_format = {48000, 1};
  plan.capture_plan.pipeline_format = {48000, 1};
  plan.capture_plan.device_frames_per_buffer = 480;
  plan.playback_plan = plan.capture_plan;
  plan.input_stream = {0, 1, 48000.0, 480, 0.01};
  plan.output_stream = {1, 1, 48000.0, 480, 0.01};
  return plan;
}

struct FakeLiveState {
  daffy::voice::LiveVoiceSessionStateSnapshot snapshot{};
  daffy::voice::LiveVoiceSessionTelemetry telemetry{};
  daffy::voice::VoiceSessionPlan plan{BuildPlan()};
  daffy::voice::AudioDeviceInventory devices{};
  int start_calls{0};
  int stop_calls{0};
  int negotiation_calls{0};
  std::string last_target_peer_id;
};

struct FakeEchoState {
  daffy::voice::VoicePeerSessionStateSnapshot snapshot{};
  daffy::voice::VoicePeerSessionTelemetry telemetry{};
  daffy::voice::DeviceAudioBlock playback_block{};
  bool block_available{true};
  int pump_calls{0};
  int echoed_blocks{0};
};

class FakeLiveVoiceSession final : public daffy::voice::LiveVoiceSessionFacade {
 public:
  FakeLiveVoiceSession(const daffy::voice::LiveVoiceSessionConfig& config, std::shared_ptr<FakeLiveState> state)
      : state_(std::move(state)) {
    state_->snapshot.peer.room = config.peer_config.room;
    state_->snapshot.peer.peer_id = config.peer_config.peer_id;
  }

  daffy::core::Status Start() override {
    ++state_->start_calls;
    state_->snapshot.running = true;
    Emit();
    return daffy::core::OkStatus();
  }

  daffy::core::Status Stop() override {
    ++state_->stop_calls;
    state_->snapshot.running = false;
    Emit();
    return daffy::core::OkStatus();
  }

  daffy::core::Status StartNegotiation(std::string target_peer_id) override {
    ++state_->negotiation_calls;
    state_->last_target_peer_id = std::move(target_peer_id);
    state_->snapshot.peer.target_peer_id = state_->last_target_peer_id;
    Emit();
    return daffy::core::OkStatus();
  }

  daffy::core::Status HandleSignalingMessage(const daffy::signaling::Message&) override {
    return daffy::core::OkStatus();
  }

  daffy::core::Status UpdateTransportConfig(const daffy::voice::LibDatachannelPeerConfig&) override {
    return daffy::core::OkStatus();
  }

  void SetSignalingMessageCallback(daffy::voice::VoicePeerSession::SignalingMessageCallback callback) override {
    signaling_callback_ = std::move(callback);
  }

  void SetStateChangeCallback(daffy::voice::LiveVoiceSession::StateChangeCallback callback) override {
    state_callback_ = std::move(callback);
    Emit();
  }

  [[nodiscard]] bool IsRunning() const override { return state_->snapshot.running; }
  [[nodiscard]] const daffy::voice::VoiceSessionPlan& plan() const override { return state_->plan; }
  [[nodiscard]] const daffy::voice::AudioDeviceInventory& devices() const override { return state_->devices; }
  [[nodiscard]] daffy::voice::LiveVoiceSessionStateSnapshot state() const override { return state_->snapshot; }
  [[nodiscard]] daffy::voice::LiveVoiceSessionTelemetry telemetry() const override { return state_->telemetry; }

 private:
  void Emit() {
    if (state_callback_) {
      state_callback_(state_->snapshot);
    }
  }

  std::shared_ptr<FakeLiveState> state_;
  daffy::voice::VoicePeerSession::SignalingMessageCallback signaling_callback_;
  daffy::voice::LiveVoiceSession::StateChangeCallback state_callback_;
};

class FakeEchoPeerSession final : public daffy::voice::VoicePeerSessionFacade {
 public:
  explicit FakeEchoPeerSession(std::shared_ptr<FakeEchoState> state) : state_(std::move(state)) {
    state_->playback_block.sequence = 7;
    state_->playback_block.format = {48000, 1};
    state_->playback_block.frame_count = 480;
    state_->playback_block.samples[0] = 0.25F;
  }

  daffy::core::Status HandleSignalingMessage(const daffy::signaling::Message&) override {
    return daffy::core::OkStatus();
  }

  daffy::core::Result<std::size_t> SendCapturedBlock(const daffy::voice::DeviceAudioBlock&) override {
    ++state_->echoed_blocks;
    return std::size_t{1};
  }

  daffy::core::Result<std::size_t> PumpInboundAudio() override {
    ++state_->pump_calls;
    return static_cast<std::size_t>(state_->block_available ? 1 : 0);
  }

  bool TryPopPlaybackBlock(daffy::voice::DeviceAudioBlock& block) override {
    if (!state_->block_available) {
      return false;
    }
    block = state_->playback_block;
    state_->block_available = false;
    return true;
  }

  void SetSignalingMessageCallback(daffy::voice::VoicePeerSession::SignalingMessageCallback callback) override {
    signaling_callback_ = std::move(callback);
  }

  void SetStateChangeCallback(daffy::voice::VoicePeerSession::StateChangeCallback callback) override {
    state_callback_ = std::move(callback);
    if (state_callback_) {
      state_callback_(state_->snapshot);
    }
  }

  [[nodiscard]] const daffy::voice::VoiceSessionPlan& plan() const override {
    static const daffy::voice::VoiceSessionPlan kPlan = BuildPlan();
    return kPlan;
  }
  [[nodiscard]] bool IsReady() const override { return true; }
  [[nodiscard]] daffy::voice::VoicePeerSessionStateSnapshot state() const override { return state_->snapshot; }
  [[nodiscard]] daffy::voice::VoicePeerSessionTelemetry telemetry() const override { return state_->telemetry; }

 private:
  std::shared_ptr<FakeEchoState> state_;
  daffy::voice::VoicePeerSession::SignalingMessageCallback signaling_callback_;
  daffy::voice::VoicePeerSession::StateChangeCallback state_callback_;
};

}  // namespace

int main() {
  auto live_state = std::make_shared<FakeLiveState>();
  auto echo_state = std::make_shared<FakeEchoState>();

  daffy::voice::VoiceLoopbackHarnessConfig config;
  config.live_config.peer_config.room = "local-audio";
  config.live_config.peer_config.peer_id = "peer-local";
  config.live_factory = [live_state](const daffy::voice::LiveVoiceSessionConfig& live_config) {
    return daffy::core::Result<std::unique_ptr<daffy::voice::LiveVoiceSessionFacade>>(
        std::make_unique<FakeLiveVoiceSession>(live_config, live_state));
  };
  config.echo_factory = [echo_state](const daffy::voice::VoicePeerSessionConfig&) {
    return daffy::core::Result<std::unique_ptr<daffy::voice::VoicePeerSessionFacade>>(
        std::make_unique<FakeEchoPeerSession>(echo_state));
  };

  auto harness = daffy::voice::VoiceLoopbackHarness::Create(config);
  assert(harness.ok());
  assert(harness.value().Start().ok());

  assert(WaitUntil([&harness]() { return harness.value().telemetry().echo_blocks_returned >= 1; }));
  assert(harness.value().Stop().ok());

  assert(live_state->start_calls >= 1);
  assert(live_state->stop_calls >= 1);
  assert(live_state->negotiation_calls >= 1);
  assert(live_state->last_target_peer_id == "peer-echo");
  assert(echo_state->pump_calls >= 1);
  assert(echo_state->echoed_blocks >= 1);
  return 0;
}
