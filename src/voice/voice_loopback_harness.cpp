#include "daffy/voice/voice_loopback_harness.hpp"

#include <chrono>
#include <mutex>
#include <thread>
#include <utility>

namespace daffy::voice {
namespace {

core::Error BuildLoopbackHarnessError(const std::string& message) {
  return core::Error{core::ErrorCode::kStateError, message};
}

}  // namespace

struct VoiceLoopbackHarness::Impl {
  explicit Impl(VoiceLoopbackHarnessConfig harness_config,
                std::unique_ptr<LiveVoiceSessionFacade> live_session,
                std::unique_ptr<VoicePeerSessionFacade> echo_session)
      : config(std::move(harness_config)), live(std::move(live_session)), echo(std::move(echo_session)) {}

  void EmitState() {
    StateChangeCallback callback;
    VoiceLoopbackHarnessStateSnapshot snapshot_copy;
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
    }
    EmitState();
  }

  void PumpLoop() {
    while (true) {
      {
        std::lock_guard<std::mutex> lock(mutex);
        if (stop_requested) {
          break;
        }
        ++pump_iterations;
      }

      bool did_work = false;
      auto pumped = echo->PumpInboundAudio();
      if (!pumped.ok()) {
        SetLastError(pumped.error().ToString());
        break;
      }
      did_work = did_work || pumped.value() > 0;

      DeviceAudioBlock block;
      while (echo->TryPopPlaybackBlock(block)) {
        {
          std::lock_guard<std::mutex> lock(mutex);
          ++echo_blocks_received;
        }
        auto echoed = echo->SendCapturedBlock(block);
        if (!echoed.ok()) {
          SetLastError(echoed.error().ToString());
          return;
        }
        {
          std::lock_guard<std::mutex> lock(mutex);
          ++echo_blocks_returned;
        }
        did_work = true;
      }

      if (!did_work) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config.pump_interval_ms));
      }
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      state_snapshot.running = false;
    }
    EmitState();
  }

  VoiceLoopbackHarnessConfig config;
  std::unique_ptr<LiveVoiceSessionFacade> live;
  std::unique_ptr<VoicePeerSessionFacade> echo;
  mutable std::mutex mutex;
  std::thread pump_thread;
  StateChangeCallback state_callback;
  VoiceLoopbackHarnessStateSnapshot state_snapshot{};
  std::uint64_t echo_blocks_received{0};
  std::uint64_t echo_blocks_returned{0};
  std::uint64_t pump_iterations{0};
  bool stop_requested{false};
};

VoiceLoopbackHarness::VoiceLoopbackHarness() = default;

VoiceLoopbackHarness::VoiceLoopbackHarness(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

VoiceLoopbackHarness::VoiceLoopbackHarness(VoiceLoopbackHarness&& other) noexcept = default;

VoiceLoopbackHarness& VoiceLoopbackHarness::operator=(VoiceLoopbackHarness&& other) noexcept = default;

VoiceLoopbackHarness::~VoiceLoopbackHarness() {
  if (impl_ != nullptr) {
    static_cast<void>(Stop());
  }
}

core::Result<VoiceLoopbackHarness> VoiceLoopbackHarness::Create(const VoiceLoopbackHarnessConfig& config) {
  if (config.echo_peer_id.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Voice loopback harness requires an echo peer id"};
  }
  if (config.pump_interval_ms <= 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Voice loopback harness pump interval must be positive"};
  }
  if (config.live_config.peer_config.room.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Voice loopback harness requires a room id"};
  }
  if (config.live_config.peer_config.peer_id.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Voice loopback harness requires a local peer id"};
  }
  if (config.live_config.peer_config.peer_id == config.echo_peer_id) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Voice loopback harness peer ids must be unique"};
  }

  auto live_factory = config.live_factory;
  if (!live_factory) {
    live_factory = CreateDefaultLiveVoiceSessionFacade;
  }
  auto echo_factory = config.echo_factory;
  if (!echo_factory) {
    echo_factory = CreateDefaultVoicePeerSessionFacade;
  }

  auto live = live_factory(config.live_config);
  if (!live.ok()) {
    return live.error();
  }

  VoicePeerSessionConfig echo_config = config.live_config.peer_config;
  echo_config.session_plan = live.value()->plan();
  echo_config.peer_id = config.echo_peer_id;
  echo_config.target_peer_id = config.live_config.peer_config.peer_id;
  if (echo_config.transport_config.local_ssrc == config.live_config.peer_config.transport_config.local_ssrc) {
    ++echo_config.transport_config.local_ssrc;
  }

  auto echo = echo_factory(echo_config);
  if (!echo.ok()) {
    return echo.error();
  }

  auto impl = std::make_unique<Impl>(config, std::move(live.value()), std::move(echo.value()));
  impl->state_snapshot.live = impl->live->state();
  impl->state_snapshot.echo = impl->echo->state();

  impl->live->SetSignalingMessageCallback([ptr = impl.get()](const signaling::Message& message) {
    auto status = ptr->echo->HandleSignalingMessage(message);
    if (!status.ok()) {
      ptr->SetLastError(status.error().ToString());
    }
  });
  impl->echo->SetSignalingMessageCallback([ptr = impl.get()](const signaling::Message& message) {
    auto status = ptr->live->HandleSignalingMessage(message);
    if (!status.ok()) {
      ptr->SetLastError(status.error().ToString());
    }
  });
  impl->live->SetStateChangeCallback([ptr = impl.get()](const LiveVoiceSessionStateSnapshot& state) {
    {
      std::lock_guard<std::mutex> lock(ptr->mutex);
      ptr->state_snapshot.live = state;
      if (!state.last_error.empty()) {
        ptr->state_snapshot.last_error = state.last_error;
      }
    }
    ptr->EmitState();
  });
  impl->echo->SetStateChangeCallback([ptr = impl.get()](const VoicePeerSessionStateSnapshot& state) {
    {
      std::lock_guard<std::mutex> lock(ptr->mutex);
      ptr->state_snapshot.echo = state;
    }
    ptr->EmitState();
  });

  return VoiceLoopbackHarness(std::move(impl));
}

core::Status VoiceLoopbackHarness::Start() {
  if (impl_ == nullptr) {
    return BuildLoopbackHarnessError("Voice loopback harness is not initialized");
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->state_snapshot.running) {
      return core::OkStatus();
    }
    impl_->stop_requested = false;
  }

  auto started = impl_->live->Start();
  if (!started.ok()) {
    return started;
  }

  impl_->pump_thread = std::thread([ptr = impl_.get()]() { ptr->PumpLoop(); });
  auto negotiation = impl_->live->StartNegotiation(impl_->config.echo_peer_id);
  if (!negotiation.ok()) {
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      impl_->stop_requested = true;
    }
    if (impl_->pump_thread.joinable()) {
      impl_->pump_thread.join();
    }
    static_cast<void>(impl_->live->Stop());
    return negotiation;
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state_snapshot.running = true;
    impl_->state_snapshot.negotiation_started = true;
  }
  impl_->EmitState();
  return core::OkStatus();
}

core::Status VoiceLoopbackHarness::Stop() {
  if (impl_ == nullptr) {
    return BuildLoopbackHarnessError("Voice loopback harness is not initialized");
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->stop_requested = true;
  }
  if (impl_->pump_thread.joinable()) {
    impl_->pump_thread.join();
  }

  auto stopped = impl_->live->Stop();
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state_snapshot.running = false;
    impl_->state_snapshot.negotiation_started = false;
    impl_->state_snapshot.live = impl_->live->state();
    impl_->state_snapshot.echo = impl_->echo->state();
    if (!stopped.ok()) {
      impl_->state_snapshot.last_error = stopped.error().ToString();
    }
  }
  impl_->EmitState();
  return stopped;
}

void VoiceLoopbackHarness::SetStateChangeCallback(StateChangeCallback callback) {
  if (impl_ == nullptr) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state_callback = std::move(callback);
  }
  impl_->EmitState();
}

bool VoiceLoopbackHarness::IsRunning() const {
  if (impl_ == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->state_snapshot.running;
}

VoiceLoopbackHarnessStateSnapshot VoiceLoopbackHarness::state() const {
  if (impl_ == nullptr) {
    return {};
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->state_snapshot;
}

VoiceLoopbackHarnessTelemetry VoiceLoopbackHarness::telemetry() const {
  VoiceLoopbackHarnessTelemetry telemetry;
  if (impl_ == nullptr) {
    return telemetry;
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  telemetry.live = impl_->live->telemetry();
  telemetry.echo = impl_->echo->telemetry();
  telemetry.echo_blocks_received = impl_->echo_blocks_received;
  telemetry.echo_blocks_returned = impl_->echo_blocks_returned;
  telemetry.pump_iterations = impl_->pump_iterations;
  return telemetry;
}

const VoiceSessionPlan& VoiceLoopbackHarness::plan() const {
  static const VoiceSessionPlan kEmptyPlan{};
  return impl_ == nullptr ? kEmptyPlan : impl_->live->plan();
}

const AudioDeviceInventory& VoiceLoopbackHarness::devices() const {
  static const AudioDeviceInventory kEmptyInventory{};
  return impl_ == nullptr ? kEmptyInventory : impl_->live->devices();
}

}  // namespace daffy::voice
