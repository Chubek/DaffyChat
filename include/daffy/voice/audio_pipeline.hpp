#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "daffy/core/error.hpp"

namespace daffy::voice {

inline constexpr int kPipelineSampleRate = 48000;
inline constexpr std::size_t kPipelineFrameSamples = 480;
inline constexpr std::size_t kMaxDeviceBlockFrames = 2048;
inline constexpr std::size_t kMaxDeviceBlockChannels = 8;
inline constexpr std::size_t kMaxDeviceBlockSamples = kMaxDeviceBlockFrames * kMaxDeviceBlockChannels;

struct AudioFormat {
  int sample_rate{kPipelineSampleRate};
  int channels{1};
};

struct StreamPlan {
  AudioFormat device_format{};
  AudioFormat pipeline_format{};
  int device_frames_per_buffer{static_cast<int>(kPipelineFrameSamples)};
  std::size_t pipeline_frame_samples{kPipelineFrameSamples};
  bool needs_resample{false};
  bool needs_downmix{false};
};

struct VoiceRuntimeConfig {
  std::string preferred_input_device{"default"};
  std::string preferred_output_device{"default"};
  int preferred_capture_sample_rate{kPipelineSampleRate};
  int preferred_playback_sample_rate{kPipelineSampleRate};
  int preferred_channels{1};
  int frames_per_buffer{static_cast<int>(kPipelineFrameSamples)};
  int playout_buffer_frames{3};
  int max_playout_buffer_frames{8};
  bool enable_noise_suppression{true};
  bool enable_metrics{true};
};

struct AudioFrame {
  std::uint64_t sequence{0};
  AudioFormat format{};
  std::array<float, kPipelineFrameSamples> samples{};
};

struct DeviceAudioBlock {
  std::uint64_t sequence{0};
  AudioFormat format{};
  std::size_t frame_count{0};
  std::array<float, kMaxDeviceBlockSamples> samples{};
};

struct CaptureTransformStats {
  std::uint64_t input_frames{0};
  std::uint64_t output_samples{0};
  std::uint64_t emitted_frames{0};
  std::uint64_t resampled_samples{0};
  std::uint64_t noise_suppressed_frames{0};
};

struct PlayoutBufferStats {
  std::uint64_t pushed_frames{0};
  std::uint64_t popped_frames{0};
  std::uint64_t dropped_frames{0};
  std::uint64_t underruns{0};
  std::size_t high_watermark_frames{0};
};

struct PlaybackRenderStats {
  std::uint64_t input_frames{0};
  std::uint64_t output_blocks{0};
  std::uint64_t output_samples{0};
  std::uint64_t resampled_samples{0};
};

core::Result<StreamPlan> BuildCaptureStreamPlan(const AudioFormat& device_format,
                                                int device_frames_per_buffer,
                                                std::size_t pipeline_frame_samples = kPipelineFrameSamples,
                                                int pipeline_sample_rate = kPipelineSampleRate);

class CaptureFrameAssembler {
 public:
  explicit CaptureFrameAssembler(AudioFormat input_format,
                                 int target_sample_rate = kPipelineSampleRate,
                                 std::size_t target_frame_samples = kPipelineFrameSamples,
                                 bool enable_noise_suppression = false);

  CaptureFrameAssembler(CaptureFrameAssembler&& other) noexcept;
  CaptureFrameAssembler& operator=(CaptureFrameAssembler&& other) noexcept;
  CaptureFrameAssembler(const CaptureFrameAssembler&) = delete;
  CaptureFrameAssembler& operator=(const CaptureFrameAssembler&) = delete;
  ~CaptureFrameAssembler();

  core::Result<std::vector<AudioFrame>> PushInterleaved(const std::vector<float>& interleaved_samples);
  core::Result<std::vector<AudioFrame>> PushInterleaved(const float* interleaved_samples, std::size_t frame_count);
  core::Status Reset();

  [[nodiscard]] std::size_t pending_pipeline_samples() const;
  [[nodiscard]] const CaptureTransformStats& stats() const;
  [[nodiscard]] AudioFormat input_format() const;
  [[nodiscard]] AudioFormat output_format() const;

 private:
  core::Result<std::vector<float>> DownmixToMono(const float* interleaved_samples, std::size_t frame_count) const;
  core::Result<std::vector<float>> ResampleToTarget(const std::vector<float>& mono_samples);
  core::Result<std::vector<AudioFrame>> DrainReadyFrames();

  AudioFormat input_format_{};
  AudioFormat output_format_{};
  std::size_t target_frame_samples_{kPipelineFrameSamples};
  std::uint64_t next_sequence_{1};
  std::vector<float> pipeline_samples_{};
  CaptureTransformStats stats_{};
  std::unique_ptr<class SampleRateConverter> resampler_{};
  std::unique_ptr<class NoiseSuppressor> noise_suppressor_{};
};

template <typename Value, std::size_t Capacity>
class TypedSpscRingBuffer {
  static_assert(Capacity >= 2, "TypedSpscRingBuffer capacity must be at least 2");

 public:
  bool TryPush(const Value& value) {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto next = Increment(head);
    if (next == tail_.load(std::memory_order_acquire)) {
      return false;
    }

    values_[head] = value;
    head_.store(next, std::memory_order_release);
    return true;
  }

  bool TryPop(Value& value) {
    const auto tail = tail_.load(std::memory_order_relaxed);
    if (tail == head_.load(std::memory_order_acquire)) {
      return false;
    }

    value = values_[tail];
    tail_.store(Increment(tail), std::memory_order_release);
    return true;
  }

  [[nodiscard]] std::size_t ApproxSize() const {
    const auto head = head_.load(std::memory_order_acquire);
    const auto tail = tail_.load(std::memory_order_acquire);
    if (head >= tail) {
      return head - tail;
    }
    return Capacity - tail + head;
  }

  [[nodiscard]] constexpr std::size_t EffectiveCapacity() const { return Capacity - 1; }

 private:
  static constexpr std::size_t Increment(std::size_t index) { return (index + 1) % Capacity; }

  std::array<Value, Capacity> values_{};
  std::atomic<std::size_t> head_{0};
  std::atomic<std::size_t> tail_{0};
};

template <std::size_t Capacity>
using AudioFrameRingBuffer = TypedSpscRingBuffer<AudioFrame, Capacity>;

template <std::size_t Capacity>
using DeviceAudioBlockRingBuffer = TypedSpscRingBuffer<DeviceAudioBlock, Capacity>;

class PlayoutBuffer {
 public:
  explicit PlayoutBuffer(std::size_t target_depth_frames = 3, std::size_t max_depth_frames = 8);

  bool Push(const AudioFrame& frame);
  std::optional<AudioFrame> PopReadyFrame();
  void Reset();

  [[nodiscard]] std::size_t depth() const;
  [[nodiscard]] const PlayoutBufferStats& stats() const;

 private:
  std::size_t target_depth_frames_{3};
  std::size_t max_depth_frames_{8};
  bool primed_{false};
  std::optional<std::uint64_t> last_sequence_seen_{};
  std::deque<AudioFrame> frames_{};
  PlayoutBufferStats stats_{};
};

class PlaybackFrameAssembler {
 public:
  explicit PlaybackFrameAssembler(AudioFormat output_format,
                                  std::size_t output_frames_per_buffer,
                                  int pipeline_sample_rate = kPipelineSampleRate);

  PlaybackFrameAssembler(PlaybackFrameAssembler&& other) noexcept;
  PlaybackFrameAssembler& operator=(PlaybackFrameAssembler&& other) noexcept;
  PlaybackFrameAssembler(const PlaybackFrameAssembler&) = delete;
  PlaybackFrameAssembler& operator=(const PlaybackFrameAssembler&) = delete;
  ~PlaybackFrameAssembler();

  core::Result<std::vector<DeviceAudioBlock>> PushFrame(const AudioFrame& frame);
  std::vector<DeviceAudioBlock> Flush();
  core::Status Reset();

  [[nodiscard]] std::size_t pending_device_frames() const;
  [[nodiscard]] const PlaybackRenderStats& stats() const;
  [[nodiscard]] AudioFormat output_format() const;

 private:
  core::Result<std::vector<float>> ResampleToDevice(const AudioFrame& frame);
  std::vector<DeviceAudioBlock> DrainReadyBlocks(bool flush_partial);

  AudioFormat output_format_{};
  int pipeline_sample_rate_{kPipelineSampleRate};
  std::size_t output_frames_per_buffer_{kPipelineFrameSamples};
  std::uint64_t next_sequence_{1};
  std::vector<float> pending_mono_samples_{};
  PlaybackRenderStats stats_{};
  std::unique_ptr<class SampleRateConverter> resampler_{};
};

}  // namespace daffy::voice
