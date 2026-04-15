#include "daffy/voice/portaudio_streams.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>

#include "daffy/voice/detail/portaudio_internal.hpp"

namespace daffy::voice {

PortAudioCallbackBridge::PortAudioCallbackBridge(AudioFormat input_format,
                                                 std::size_t input_frames_per_buffer,
                                                 AudioFormat output_format,
                                                 std::size_t output_frames_per_buffer)
    : input_format_(input_format),
      output_format_(output_format),
      input_frames_per_buffer_(input_frames_per_buffer),
      output_frames_per_buffer_(output_frames_per_buffer) {}

bool PortAudioCallbackBridge::HandleCapture(const float* samples, std::size_t frame_count) {
  ++capture_callbacks_;
  if (input_format_.channels <= 0 || input_format_.channels > static_cast<int>(kMaxDeviceBlockChannels) ||
      frame_count == 0 || frame_count > kMaxDeviceBlockFrames) {
    ++dropped_capture_blocks_;
    return false;
  }

  DeviceAudioBlock block;
  block.sequence = next_capture_sequence_++;
  block.format = input_format_;
  block.frame_count = frame_count;

  const auto sample_count = frame_count * static_cast<std::size_t>(input_format_.channels);
  if (samples != nullptr) {
    std::copy_n(samples, static_cast<std::ptrdiff_t>(sample_count), block.samples.begin());
  } else {
    std::fill_n(block.samples.begin(), static_cast<std::ptrdiff_t>(sample_count), 0.0F);
  }

  if (!capture_blocks_.TryPush(block)) {
    ++dropped_capture_blocks_;
    return false;
  }

  ++captured_blocks_;
  return true;
}

void PortAudioCallbackBridge::HandlePlayback(float* samples, std::size_t frame_count) {
  ++playback_callbacks_;
  if (samples == nullptr || output_format_.channels <= 0 || output_format_.channels > static_cast<int>(kMaxDeviceBlockChannels) ||
      frame_count == 0 || frame_count > kMaxDeviceBlockFrames) {
    ++playback_underruns_;
    return;
  }

  const auto requested_samples = frame_count * static_cast<std::size_t>(output_format_.channels);
  std::fill_n(samples, static_cast<std::ptrdiff_t>(requested_samples), 0.0F);

  std::size_t frames_written = 0;
  while (frames_written < frame_count) {
    if (playback_inflight_offset_ >= playback_inflight_.frame_count) {
      playback_inflight_ = DeviceAudioBlock{};
      playback_inflight_offset_ = 0;
      if (!playback_blocks_.TryPop(playback_inflight_)) {
        ++playback_underruns_;
        break;
      }
      ++played_blocks_;
    }

    const std::size_t available_frames = playback_inflight_.frame_count - playback_inflight_offset_;
    const std::size_t needed_frames = frame_count - frames_written;
    const std::size_t frames_to_copy = std::min(available_frames, needed_frames);
    const auto source_offset = playback_inflight_offset_ * static_cast<std::size_t>(output_format_.channels);
    const auto destination_offset = frames_written * static_cast<std::size_t>(output_format_.channels);
    const auto samples_to_copy = frames_to_copy * static_cast<std::size_t>(output_format_.channels);
    std::copy_n(playback_inflight_.samples.begin() + static_cast<std::ptrdiff_t>(source_offset),
                static_cast<std::ptrdiff_t>(samples_to_copy),
                samples + static_cast<std::ptrdiff_t>(destination_offset));

    playback_inflight_offset_ += frames_to_copy;
    frames_written += frames_to_copy;
  }
}

bool PortAudioCallbackBridge::TryPopCapturedBlock(DeviceAudioBlock& block) { return capture_blocks_.TryPop(block); }

bool PortAudioCallbackBridge::TryQueuePlaybackBlock(const DeviceAudioBlock& block) {
  if (block.format.sample_rate != output_format_.sample_rate || block.format.channels != output_format_.channels ||
      block.frame_count == 0 || block.frame_count > kMaxDeviceBlockFrames) {
    ++dropped_playback_blocks_;
    return false;
  }
  if (!playback_blocks_.TryPush(block)) {
    ++dropped_playback_blocks_;
    return false;
  }
  return true;
}

PortAudioCallbackBridgeStats PortAudioCallbackBridge::stats() const {
  return PortAudioCallbackBridgeStats{
      capture_callbacks_.load(std::memory_order_acquire),
      playback_callbacks_.load(std::memory_order_acquire),
      captured_blocks_.load(std::memory_order_acquire),
      played_blocks_.load(std::memory_order_acquire),
      dropped_capture_blocks_.load(std::memory_order_acquire),
      dropped_playback_blocks_.load(std::memory_order_acquire),
      playback_underruns_.load(std::memory_order_acquire),
  };
}

struct PortAudioStreamSession::Impl {
  std::shared_ptr<PortAudioRuntime::Impl> runtime;
  VoiceSessionPlan plan;
  PortAudioCallbackBridge bridge;
  PaStream* input_stream{nullptr};
  PaStream* output_stream{nullptr};
  bool initialized{false};
  bool started{false};

  Impl(std::shared_ptr<PortAudioRuntime::Impl> runtime_impl, const VoiceSessionPlan& session_plan)
      : runtime(std::move(runtime_impl)),
        plan(session_plan),
        bridge(session_plan.capture_plan.device_format,
               static_cast<std::size_t>(session_plan.input_stream.frames_per_buffer),
               session_plan.playback_plan.device_format,
               static_cast<std::size_t>(session_plan.output_stream.frames_per_buffer)) {}
};

namespace {

core::Error BuildPortAudioError(const PortAudioRuntime::Impl& impl, const std::string& prefix, PaError error) {
  return core::Error{core::ErrorCode::kUnavailable,
                     prefix + ": " + (impl.get_error_text != nullptr ? impl.get_error_text(error) : "unknown PortAudio error")};
}

int CaptureCallback(const void* input,
                    void* output,
                    unsigned long frame_count,
                    const PaStreamCallbackTimeInfo* time_info,
                    PaStreamCallbackFlags status_flags,
                    void* user_data) {
  static_cast<void>(output);
  static_cast<void>(time_info);
  static_cast<void>(status_flags);
  auto* session = static_cast<PortAudioStreamSession::Impl*>(user_data);
  if (session == nullptr) {
    return paAbort;
  }
  session->bridge.HandleCapture(static_cast<const float*>(input), static_cast<std::size_t>(frame_count));
  return paContinue;
}

int PlaybackCallback(const void* input,
                     void* output,
                     unsigned long frame_count,
                     const PaStreamCallbackTimeInfo* time_info,
                     PaStreamCallbackFlags status_flags,
                     void* user_data) {
  static_cast<void>(input);
  static_cast<void>(time_info);
  static_cast<void>(status_flags);
  auto* session = static_cast<PortAudioStreamSession::Impl*>(user_data);
  if (session == nullptr) {
    return paAbort;
  }
  session->bridge.HandlePlayback(static_cast<float*>(output), static_cast<std::size_t>(frame_count));
  return paContinue;
}

core::Result<PaStream*> OpenCaptureStream(const std::shared_ptr<PortAudioRuntime::Impl>& runtime,
                                          PortAudioStreamSession::Impl& session) {
  PaStreamParameters input{};
  input.device = session.plan.input_stream.device_index;
  input.channelCount = session.plan.input_stream.channel_count;
  input.sampleFormat = paFloat32;
  input.suggestedLatency = session.plan.input_stream.suggested_latency_seconds;
  input.hostApiSpecificStreamInfo = nullptr;

  PaStream* stream = nullptr;
  const auto error = runtime->open_stream(&stream,
                                          &input,
                                          nullptr,
                                          session.plan.input_stream.sample_rate,
                                          session.plan.input_stream.frames_per_buffer,
                                          paClipOff | paDitherOff,
                                          &CaptureCallback,
                                          &session);
  if (error != paNoError) {
    return BuildPortAudioError(*runtime, "PortAudio failed to open capture stream", error);
  }
  return stream;
}

core::Result<PaStream*> OpenPlaybackStream(const std::shared_ptr<PortAudioRuntime::Impl>& runtime,
                                           PortAudioStreamSession::Impl& session) {
  PaStreamParameters output{};
  output.device = session.plan.output_stream.device_index;
  output.channelCount = session.plan.output_stream.channel_count;
  output.sampleFormat = paFloat32;
  output.suggestedLatency = session.plan.output_stream.suggested_latency_seconds;
  output.hostApiSpecificStreamInfo = nullptr;

  PaStream* stream = nullptr;
  const auto error = runtime->open_stream(&stream,
                                          nullptr,
                                          &output,
                                          session.plan.output_stream.sample_rate,
                                          session.plan.output_stream.frames_per_buffer,
                                          paClipOff | paDitherOff,
                                          &PlaybackCallback,
                                          &session);
  if (error != paNoError) {
    return BuildPortAudioError(*runtime, "PortAudio failed to open playback stream", error);
  }
  return stream;
}

}  // namespace

PortAudioStreamSession::PortAudioStreamSession() = default;

PortAudioStreamSession::PortAudioStreamSession(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

PortAudioStreamSession::PortAudioStreamSession(PortAudioStreamSession&& other) noexcept = default;

PortAudioStreamSession& PortAudioStreamSession::operator=(PortAudioStreamSession&& other) noexcept = default;

PortAudioStreamSession::~PortAudioStreamSession() {
  if (impl_ == nullptr || impl_->runtime == nullptr) {
    return;
  }

  if (impl_->started) {
    if (impl_->input_stream != nullptr && impl_->runtime->abort_stream != nullptr) {
      impl_->runtime->abort_stream(impl_->input_stream);
    }
    if (impl_->output_stream != nullptr && impl_->runtime->abort_stream != nullptr) {
      impl_->runtime->abort_stream(impl_->output_stream);
    }
    impl_->started = false;
  }

  if (impl_->input_stream != nullptr && impl_->runtime->close_stream != nullptr) {
    impl_->runtime->close_stream(impl_->input_stream);
    impl_->input_stream = nullptr;
  }
  if (impl_->output_stream != nullptr && impl_->runtime->close_stream != nullptr) {
    impl_->runtime->close_stream(impl_->output_stream);
    impl_->output_stream = nullptr;
  }

  if (impl_->initialized && impl_->runtime->terminate != nullptr) {
    impl_->runtime->terminate();
    impl_->initialized = false;
  }
}

bool PortAudioStreamSession::IsOpen() const { return impl_ != nullptr && impl_->initialized; }

bool PortAudioStreamSession::IsStarted() const { return impl_ != nullptr && impl_->started; }

const VoiceSessionPlan& PortAudioStreamSession::plan() const { return impl_->plan; }

core::Status PortAudioStreamSession::Start() {
  if (impl_ == nullptr || impl_->runtime == nullptr || !impl_->initialized) {
    return core::Error{core::ErrorCode::kStateError, "PortAudio stream session is not open"};
  }
  if (impl_->started) {
    return core::OkStatus();
  }

  if (impl_->input_stream != nullptr) {
    const auto error = impl_->runtime->start_stream(impl_->input_stream);
    if (error != paNoError) {
      return BuildPortAudioError(*impl_->runtime, "PortAudio failed to start capture stream", error);
    }
  }
  if (impl_->output_stream != nullptr) {
    const auto error = impl_->runtime->start_stream(impl_->output_stream);
    if (error != paNoError) {
      if (impl_->input_stream != nullptr) {
        impl_->runtime->abort_stream(impl_->input_stream);
      }
      return BuildPortAudioError(*impl_->runtime, "PortAudio failed to start playback stream", error);
    }
  }

  impl_->started = true;
  return core::OkStatus();
}

core::Status PortAudioStreamSession::Stop() {
  if (impl_ == nullptr || impl_->runtime == nullptr || !impl_->initialized) {
    return core::Error{core::ErrorCode::kStateError, "PortAudio stream session is not open"};
  }
  if (!impl_->started) {
    return core::OkStatus();
  }

  core::Result<std::monostate> status = core::OkStatus();
  if (impl_->input_stream != nullptr) {
    const auto error = impl_->runtime->stop_stream(impl_->input_stream);
    if (error != paNoError && status.ok()) {
      status = BuildPortAudioError(*impl_->runtime, "PortAudio failed to stop capture stream", error);
    }
  }
  if (impl_->output_stream != nullptr) {
    const auto error = impl_->runtime->stop_stream(impl_->output_stream);
    if (error != paNoError && status.ok()) {
      status = BuildPortAudioError(*impl_->runtime, "PortAudio failed to stop playback stream", error);
    }
  }

  impl_->started = false;
  return status;
}

bool PortAudioStreamSession::TryPopCapturedBlock(DeviceAudioBlock& block) {
  return impl_ != nullptr && impl_->bridge.TryPopCapturedBlock(block);
}

bool PortAudioStreamSession::TryQueuePlaybackBlock(const DeviceAudioBlock& block) {
  return impl_ != nullptr && impl_->bridge.TryQueuePlaybackBlock(block);
}

PortAudioCallbackBridgeStats PortAudioStreamSession::stats() const {
  return impl_ == nullptr ? PortAudioCallbackBridgeStats{} : impl_->bridge.stats();
}

core::Result<PortAudioStreamSession> OpenPortAudioStreamSession(const std::shared_ptr<PortAudioRuntime::Impl>& impl,
                                                                const VoiceSessionPlan& plan) {
  if (impl == nullptr) {
    return core::Error{core::ErrorCode::kStateError, "PortAudio runtime is not loaded"};
  }
  if (plan.input_stream.frames_per_buffer > kMaxDeviceBlockFrames ||
      plan.output_stream.frames_per_buffer > kMaxDeviceBlockFrames) {
    return core::Error{core::ErrorCode::kInvalidArgument, "VoiceSessionPlan exceeds PortAudio callback buffer limits"};
  }
  if (plan.input_stream.channel_count > static_cast<int>(kMaxDeviceBlockChannels) ||
      plan.output_stream.channel_count > static_cast<int>(kMaxDeviceBlockChannels)) {
    return core::Error{core::ErrorCode::kInvalidArgument, "VoiceSessionPlan exceeds supported channel limits"};
  }

  auto session = std::make_unique<PortAudioStreamSession::Impl>(impl, plan);
  const auto init_error = impl->initialize();
  if (init_error != paNoError) {
    return BuildPortAudioError(*impl, "PortAudio initialization failed", init_error);
  }
  session->initialized = true;

  auto input_stream = OpenCaptureStream(impl, *session);
  if (!input_stream.ok()) {
    impl->terminate();
    return input_stream.error();
  }
  session->input_stream = input_stream.value();

  auto output_stream = OpenPlaybackStream(impl, *session);
  if (!output_stream.ok()) {
    if (session->input_stream != nullptr) {
      impl->close_stream(session->input_stream);
      session->input_stream = nullptr;
    }
    impl->terminate();
    return output_stream.error();
  }
  session->output_stream = output_stream.value();

  return PortAudioStreamSession(std::move(session));
}

}  // namespace daffy::voice
