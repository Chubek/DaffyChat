#include "daffy/voice/live_voice_session.hpp"

#include <chrono>
#include <mutex>
#include <thread>
#include <utility>

namespace daffy::voice {
namespace {

core::Error BuildLiveSessionError(const std::string& message) {
  return core::Error{core::ErrorCode::kStateError, message};
}

bool HasResolvedPlan(const VoiceSessionPlan& plan) {
  return plan.input_stream.device_index >= 0 && plan.output_stream.device_index >= 0;
}

}  // namespace

util::json::Value LiveVoiceSessionStateToJson(const LiveVoiceSessionStateSnapshot& state) {
  return util::json::Value::Object{{"stream_open", state.stream_open},
                                   {"stream_started", state.stream_started},
                                   {"running", state.running},
                                   {"last_error", state.last_error},
                                   {"peer", VoicePeerSessionStateToJson(state.peer)}};
}

util::json::Value LiveVoiceSessionTelemetryToJson(const LiveVoiceSessionTelemetry& telemetry) {
  return util::json::Value::Object{
      {"peer", VoicePeerSessionTelemetryToJson(telemetry.peer)},
      {"stream",
       util::json::Value::Object{{"capture_callbacks", static_cast<int>(telemetry.stream.capture_callbacks)},
                                 {"playback_callbacks", static_cast<int>(telemetry.stream.playback_callbacks)},
                                 {"captured_blocks", static_cast<int>(telemetry.stream.captured_blocks)},
                                 {"played_blocks", static_cast<int>(telemetry.stream.played_blocks)},
                                 {"dropped_capture_blocks", static_cast<int>(telemetry.stream.dropped_capture_blocks)},
                                 {"dropped_playback_blocks",
                                  static_cast<int>(telemetry.stream.dropped_playback_blocks)},
                                 {"playback_underruns", static_cast<int>(telemetry.stream.playback_underruns)}}},
      {"capture_blocks_dropped_while_disconnected",
       static_cast<int>(telemetry.capture_blocks_dropped_while_disconnected)},
      {"pump_iterations", static_cast<int>(telemetry.pump_iterations)}};
}

struct LiveVoiceSession::Impl {
  explicit Impl(LiveVoiceSessionConfig session_config,
                PortAudioRuntime runtime_handle,
                AudioDeviceInventory device_inventory,
                PortAudioStreamSession audio_stream,
                VoicePeerSession voice_session)
      : config(std::move(session_config)),
        runtime(std::move(runtime_handle)),
        inventory(std::move(device_inventory)),
        stream(std::move(audio_stream)),
        peer(std::move(voice_session)) {}

  void EmitState() {
    StateChangeCallback callback;
    LiveVoiceSessionStateSnapshot snapshot_copy;
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

  void SetRunningState(const bool running, const bool stream_started) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      state_snapshot.running = running;
      state_snapshot.stream_open = stream.IsOpen();
      state_snapshot.stream_started = stream_started;
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
      if (peer.IsReady()) {
        auto capture = peer.PumpCapture(stream);
        if (!capture.ok()) {
          SetLastError(capture.error().ToString());
          break;
        }
        did_work = did_work || capture.value() > 0;
      } else {
        DeviceAudioBlock dropped_block;
        while (stream.TryPopCapturedBlock(dropped_block)) {
          did_work = true;
          std::lock_guard<std::mutex> lock(mutex);
          ++capture_blocks_dropped_while_disconnected;
        }
      }

      auto playback = peer.PumpPlayback(stream);
      if (!playback.ok()) {
        SetLastError(playback.error().ToString());
        break;
      }
      did_work = did_work || playback.value() > 0;

      if (!did_work) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config.pump_interval_ms));
      }
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      state_snapshot.running = false;
      state_snapshot.stream_started = false;
    }
    EmitState();
  }

  LiveVoiceSessionConfig config;
  PortAudioRuntime runtime;
  AudioDeviceInventory inventory;
  PortAudioStreamSession stream;
  VoicePeerSession peer;
  mutable std::mutex mutex;
  std::thread pump_thread;
  StateChangeCallback state_callback;
  LiveVoiceSessionStateSnapshot state_snapshot{};
  std::uint64_t capture_blocks_dropped_while_disconnected{0};
  std::uint64_t pump_iterations{0};
  bool stop_requested{false};
};

LiveVoiceSession::LiveVoiceSession() = default;

LiveVoiceSession::LiveVoiceSession(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

LiveVoiceSession::LiveVoiceSession(LiveVoiceSession&& other) noexcept = default;

LiveVoiceSession& LiveVoiceSession::operator=(LiveVoiceSession&& other) noexcept = default;

LiveVoiceSession::~LiveVoiceSession() {
  if (impl_ != nullptr) {
    static_cast<void>(Stop());
  }
}

core::Result<LiveVoiceSession> LiveVoiceSession::Create(const LiveVoiceSessionConfig& config) {
  if (config.pump_interval_ms <= 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Live voice session pump interval must be positive"};
  }

  auto runtime = PortAudioRuntime::Load();
  if (!runtime.ok()) {
    return runtime.error();
  }

  auto inventory = runtime.value().EnumerateDevices();
  if (!inventory.ok()) {
    return inventory.error();
  }

  VoicePeerSessionConfig peer_config = config.peer_config;
  if (!HasResolvedPlan(peer_config.session_plan)) {
    auto plan = runtime.value().BuildSessionPlan(peer_config.runtime_config);
    if (!plan.ok()) {
      return plan.error();
    }
    peer_config.session_plan = plan.value();
  }

  auto stream = runtime.value().OpenSession(peer_config.session_plan);
  if (!stream.ok()) {
    return stream.error();
  }

  auto peer = VoicePeerSession::Create(peer_config);
  if (!peer.ok()) {
    return peer.error();
  }

  LiveVoiceSessionConfig resolved_config = config;
  resolved_config.peer_config = peer_config;
  auto impl = std::make_unique<Impl>(std::move(resolved_config),
                                     std::move(runtime.value()),
                                     std::move(inventory.value()),
                                     std::move(stream.value()),
                                     std::move(peer.value()));
  impl->state_snapshot.stream_open = impl->stream.IsOpen();
  impl->state_snapshot.stream_started = impl->stream.IsStarted();
  impl->state_snapshot.running = false;
  impl->state_snapshot.peer = impl->peer.state();

  impl->peer.SetStateChangeCallback([ptr = impl.get()](const VoicePeerSessionStateSnapshot& peer_state) {
    {
      std::lock_guard<std::mutex> lock(ptr->mutex);
      ptr->state_snapshot.peer = peer_state;
    }
    ptr->EmitState();
  });

  return LiveVoiceSession(std::move(impl));
}

core::Status LiveVoiceSession::Start() {
  if (impl_ == nullptr) {
    return BuildLiveSessionError("Live voice session is not initialized");
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->state_snapshot.running) {
      return core::OkStatus();
    }
    impl_->stop_requested = false;
  }

  auto started = impl_->stream.Start();
  if (!started.ok()) {
    return started;
  }
  impl_->SetRunningState(true, true);
  impl_->pump_thread = std::thread([ptr = impl_.get()]() { ptr->PumpLoop(); });
  return core::OkStatus();
}

core::Status LiveVoiceSession::Stop() {
  if (impl_ == nullptr) {
    return BuildLiveSessionError("Live voice session is not initialized");
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->stop_requested = true;
  }
  if (impl_->pump_thread.joinable()) {
    impl_->pump_thread.join();
  }

  auto stopped = impl_->stream.Stop();
  if (!stopped.ok()) {
    impl_->SetLastError(stopped.error().ToString());
    return stopped;
  }
  impl_->SetRunningState(false, false);
  return core::OkStatus();
}

core::Status LiveVoiceSession::StartNegotiation(std::string target_peer_id) {
  if (impl_ == nullptr) {
    return BuildLiveSessionError("Live voice session is not initialized");
  }
  return impl_->peer.StartNegotiation(std::move(target_peer_id));
}

core::Status LiveVoiceSession::HandleSignalingMessage(const signaling::Message& message) {
  if (impl_ == nullptr) {
    return BuildLiveSessionError("Live voice session is not initialized");
  }
  return impl_->peer.HandleSignalingMessage(message);
}

core::Status LiveVoiceSession::UpdateTransportConfig(const LibDatachannelPeerConfig& transport_config) {
  if (impl_ == nullptr) {
    return BuildLiveSessionError("Live voice session is not initialized");
  }
  return impl_->peer.UpdateTransportConfig(transport_config);
}

void LiveVoiceSession::SetSignalingMessageCallback(VoicePeerSession::SignalingMessageCallback callback) {
  if (impl_ == nullptr) {
    return;
  }
  impl_->peer.SetSignalingMessageCallback(std::move(callback));
}

void LiveVoiceSession::SetStateChangeCallback(StateChangeCallback callback) {
  if (impl_ == nullptr) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state_callback = std::move(callback);
  }
  impl_->EmitState();
}

bool LiveVoiceSession::IsRunning() const {
  if (impl_ == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->state_snapshot.running;
}

const VoiceSessionPlan& LiveVoiceSession::plan() const {
  static const VoiceSessionPlan kEmptyPlan{};
  return impl_ == nullptr ? kEmptyPlan : impl_->peer.plan();
}

const AudioDeviceInventory& LiveVoiceSession::devices() const {
  static const AudioDeviceInventory kEmptyInventory{};
  return impl_ == nullptr ? kEmptyInventory : impl_->inventory;
}

LiveVoiceSessionStateSnapshot LiveVoiceSession::state() const {
  if (impl_ == nullptr) {
    return {};
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->state_snapshot;
}

LiveVoiceSessionTelemetry LiveVoiceSession::telemetry() const {
  LiveVoiceSessionTelemetry telemetry;
  if (impl_ == nullptr) {
    return telemetry;
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  telemetry.peer = impl_->peer.telemetry();
  telemetry.stream = impl_->stream.stats();
  telemetry.capture_blocks_dropped_while_disconnected = impl_->capture_blocks_dropped_while_disconnected;
  telemetry.pump_iterations = impl_->pump_iterations;
  return telemetry;
}

}  // namespace daffy::voice
