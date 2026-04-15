#pragma once

#include <atomic>
#include <memory>

#include "daffy/core/error.hpp"
#include "daffy/voice/audio_pipeline.hpp"
#include "daffy/voice/portaudio_runtime.hpp"

namespace daffy::voice {

struct PortAudioCallbackBridgeStats {
  std::uint64_t capture_callbacks{0};
  std::uint64_t playback_callbacks{0};
  std::uint64_t captured_blocks{0};
  std::uint64_t played_blocks{0};
  std::uint64_t dropped_capture_blocks{0};
  std::uint64_t dropped_playback_blocks{0};
  std::uint64_t playback_underruns{0};
};

class PortAudioCallbackBridge {
 public:
  PortAudioCallbackBridge(AudioFormat input_format,
                          std::size_t input_frames_per_buffer,
                          AudioFormat output_format,
                          std::size_t output_frames_per_buffer);

  bool HandleCapture(const float* samples, std::size_t frame_count);
  void HandlePlayback(float* samples, std::size_t frame_count);

  bool TryPopCapturedBlock(DeviceAudioBlock& block);
  bool TryQueuePlaybackBlock(const DeviceAudioBlock& block);
  [[nodiscard]] PortAudioCallbackBridgeStats stats() const;

 private:
  static constexpr std::size_t kCaptureQueueCapacity = 16;
  static constexpr std::size_t kPlaybackQueueCapacity = 16;

  AudioFormat input_format_{};
  AudioFormat output_format_{};
  std::size_t input_frames_per_buffer_{kPipelineFrameSamples};
  std::size_t output_frames_per_buffer_{kPipelineFrameSamples};
  DeviceAudioBlockRingBuffer<kCaptureQueueCapacity> capture_blocks_{};
  DeviceAudioBlockRingBuffer<kPlaybackQueueCapacity> playback_blocks_{};
  DeviceAudioBlock playback_inflight_{};
  std::size_t playback_inflight_offset_{0};
  std::uint64_t next_capture_sequence_{1};
  std::atomic<std::uint64_t> capture_callbacks_{0};
  std::atomic<std::uint64_t> playback_callbacks_{0};
  std::atomic<std::uint64_t> captured_blocks_{0};
  std::atomic<std::uint64_t> played_blocks_{0};
  std::atomic<std::uint64_t> dropped_capture_blocks_{0};
  std::atomic<std::uint64_t> dropped_playback_blocks_{0};
  std::atomic<std::uint64_t> playback_underruns_{0};
};

class PortAudioStreamSession {
 public:
  struct Impl;

  PortAudioStreamSession();
  PortAudioStreamSession(PortAudioStreamSession&& other) noexcept;
  PortAudioStreamSession& operator=(PortAudioStreamSession&& other) noexcept;
  PortAudioStreamSession(const PortAudioStreamSession&) = delete;
  PortAudioStreamSession& operator=(const PortAudioStreamSession&) = delete;
  ~PortAudioStreamSession();

  [[nodiscard]] bool IsOpen() const;
  [[nodiscard]] bool IsStarted() const;
  [[nodiscard]] const VoiceSessionPlan& plan() const;

  core::Status Start();
  core::Status Stop();

  bool TryPopCapturedBlock(DeviceAudioBlock& block);
  bool TryQueuePlaybackBlock(const DeviceAudioBlock& block);
  [[nodiscard]] PortAudioCallbackBridgeStats stats() const;

 private:
  explicit PortAudioStreamSession(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;

  friend class PortAudioRuntime;
  friend core::Result<PortAudioStreamSession> OpenPortAudioStreamSession(
      const std::shared_ptr<PortAudioRuntime::Impl>& impl, const VoiceSessionPlan& plan);
};

}  // namespace daffy::voice
