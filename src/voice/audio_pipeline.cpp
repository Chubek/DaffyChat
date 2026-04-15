#include "daffy/voice/audio_pipeline.hpp"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <memory>
#include <utility>

#include "daffy/voice/audio_processing.hpp"

namespace daffy::voice {

core::Result<StreamPlan> BuildCaptureStreamPlan(const AudioFormat& device_format,
                                                int device_frames_per_buffer,
                                                std::size_t pipeline_frame_samples,
                                                int pipeline_sample_rate) {
  if (device_format.sample_rate <= 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Device sample rate must be positive"};
  }
  if (device_format.channels <= 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Device channel count must be positive"};
  }
  if (device_frames_per_buffer <= 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Device frames per buffer must be positive"};
  }
  if (pipeline_frame_samples == 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Pipeline frame size must be non-zero"};
  }
  if (pipeline_sample_rate <= 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Pipeline sample rate must be positive"};
  }

  StreamPlan plan;
  plan.device_format = device_format;
  plan.pipeline_format = AudioFormat{pipeline_sample_rate, 1};
  plan.device_frames_per_buffer = device_frames_per_buffer;
  plan.pipeline_frame_samples = pipeline_frame_samples;
  plan.needs_resample = device_format.sample_rate != pipeline_sample_rate;
  plan.needs_downmix = device_format.channels != 1;
  return plan;
}

CaptureFrameAssembler::CaptureFrameAssembler(AudioFormat input_format,
                                             int target_sample_rate,
                                             std::size_t target_frame_samples,
                                             bool enable_noise_suppression)
    : input_format_(input_format),
      output_format_(AudioFormat{target_sample_rate, 1}),
      target_frame_samples_(target_frame_samples) {
  if (input_format_.sample_rate > 0 && output_format_.sample_rate > 0 &&
      input_format_.sample_rate != output_format_.sample_rate) {
    auto resampler = SampleRateConverter::Create(input_format_.sample_rate, output_format_.sample_rate, 1);
    if (resampler.ok()) {
      resampler_ = std::make_unique<SampleRateConverter>(std::move(resampler.value()));
    }
  }

  if (enable_noise_suppression) {
    auto suppressor = NoiseSuppressor::Create();
    if (suppressor.ok() && suppressor.value().frame_samples() == target_frame_samples_) {
      noise_suppressor_ = std::make_unique<NoiseSuppressor>(std::move(suppressor.value()));
    }
  }
}

CaptureFrameAssembler::CaptureFrameAssembler(CaptureFrameAssembler&& other) noexcept = default;

CaptureFrameAssembler& CaptureFrameAssembler::operator=(CaptureFrameAssembler&& other) noexcept = default;

CaptureFrameAssembler::~CaptureFrameAssembler() = default;

core::Result<std::vector<AudioFrame>> CaptureFrameAssembler::PushInterleaved(
    const std::vector<float>& interleaved_samples) {
  if (input_format_.channels <= 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Input channel count must be positive"};
  }
  if (interleaved_samples.size() % static_cast<std::size_t>(input_format_.channels) != 0) {
    return core::Error{core::ErrorCode::kInvalidArgument,
                       "Interleaved sample count must be divisible by the channel count"};
  }
  return PushInterleaved(interleaved_samples.data(), interleaved_samples.size() / input_format_.channels);
}

core::Result<std::vector<AudioFrame>> CaptureFrameAssembler::PushInterleaved(const float* interleaved_samples,
                                                                              std::size_t frame_count) {
  if (frame_count == 0) {
    return std::vector<AudioFrame>{};
  }
  if (interleaved_samples == nullptr) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Input samples cannot be null"};
  }
  if (input_format_.sample_rate <= 0 || input_format_.channels <= 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Input format must have a positive sample rate and channels"};
  }
  if (target_frame_samples_ == 0 || output_format_.sample_rate <= 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Output format must be configured with valid frame sizing"};
  }

  auto mono = DownmixToMono(interleaved_samples, frame_count);
  if (!mono.ok()) {
    return mono.error();
  }

  stats_.input_frames += frame_count;
  auto converted = ResampleToTarget(mono.value());
  if (!converted.ok()) {
    return converted.error();
  }
  stats_.resampled_samples += converted.value().size();
  stats_.output_samples += converted.value().size();
  pipeline_samples_.insert(pipeline_samples_.end(), converted.value().begin(), converted.value().end());

  return DrainReadyFrames();
}

core::Status CaptureFrameAssembler::Reset() {
  pipeline_samples_.clear();
  next_sequence_ = 1;
  if (resampler_ != nullptr) {
    auto reset = resampler_->Reset();
    if (!reset.ok()) {
      return reset;
    }
  }
  return core::OkStatus();
}

std::size_t CaptureFrameAssembler::pending_pipeline_samples() const { return pipeline_samples_.size(); }

const CaptureTransformStats& CaptureFrameAssembler::stats() const { return stats_; }

AudioFormat CaptureFrameAssembler::input_format() const { return input_format_; }

AudioFormat CaptureFrameAssembler::output_format() const { return output_format_; }

core::Result<std::vector<float>> CaptureFrameAssembler::DownmixToMono(const float* interleaved_samples,
                                                                      std::size_t frame_count) const {
  std::vector<float> mono(frame_count, 0.0F);
  for (std::size_t frame = 0; frame < frame_count; ++frame) {
    float mixed = 0.0F;
    for (int channel = 0; channel < input_format_.channels; ++channel) {
      mixed += interleaved_samples[frame * static_cast<std::size_t>(input_format_.channels) + channel];
    }
    mono[frame] = mixed / static_cast<float>(input_format_.channels);
  }
  return mono;
}

core::Result<std::vector<float>> CaptureFrameAssembler::ResampleToTarget(const std::vector<float>& mono_samples) {
  if (input_format_.sample_rate == output_format_.sample_rate || resampler_ == nullptr) {
    return mono_samples;
  }
  return resampler_->Process(mono_samples);
}

core::Result<std::vector<AudioFrame>> CaptureFrameAssembler::DrainReadyFrames() {
  std::vector<AudioFrame> frames;
  while (pipeline_samples_.size() >= target_frame_samples_) {
    AudioFrame frame;
    frame.sequence = next_sequence_++;
    frame.format = output_format_;
    std::copy_n(pipeline_samples_.begin(), static_cast<std::ptrdiff_t>(target_frame_samples_), frame.samples.begin());
    pipeline_samples_.erase(pipeline_samples_.begin(),
                            pipeline_samples_.begin() + static_cast<std::ptrdiff_t>(target_frame_samples_));

    if (noise_suppressor_ != nullptr) {
      auto vad = noise_suppressor_->ProcessFrame(frame.samples.data(), target_frame_samples_);
      if (!vad.ok()) {
        return vad.error();
      }
      ++stats_.noise_suppressed_frames;
    }

    frames.push_back(frame);
  }

  stats_.emitted_frames += frames.size();
  return frames;
}

PlayoutBuffer::PlayoutBuffer(std::size_t target_depth_frames, std::size_t max_depth_frames)
    : target_depth_frames_(std::max<std::size_t>(1, target_depth_frames)),
      max_depth_frames_(std::max(target_depth_frames_, max_depth_frames)) {}

bool PlayoutBuffer::Push(const AudioFrame& frame) {
  if (last_sequence_seen_.has_value() && frame.sequence <= *last_sequence_seen_) {
    ++stats_.dropped_frames;
    return false;
  }

  last_sequence_seen_ = frame.sequence;
  if (frames_.size() >= max_depth_frames_) {
    frames_.pop_front();
    ++stats_.dropped_frames;
  }

  frames_.push_back(frame);
  ++stats_.pushed_frames;
  stats_.high_watermark_frames = std::max(stats_.high_watermark_frames, frames_.size());
  if (!primed_ && frames_.size() >= target_depth_frames_) {
    primed_ = true;
  }

  return true;
}

std::optional<AudioFrame> PlayoutBuffer::PopReadyFrame() {
  if (!primed_) {
    ++stats_.underruns;
    return std::nullopt;
  }
  if (frames_.empty()) {
    primed_ = false;
    ++stats_.underruns;
    return std::nullopt;
  }

  AudioFrame frame = frames_.front();
  frames_.pop_front();
  ++stats_.popped_frames;
  if (frames_.empty()) {
    primed_ = false;
  }
  return frame;
}

std::size_t PlayoutBuffer::depth() const { return frames_.size(); }

const PlayoutBufferStats& PlayoutBuffer::stats() const { return stats_; }

void PlayoutBuffer::Reset() {
  primed_ = false;
  last_sequence_seen_.reset();
  frames_.clear();
}

PlaybackFrameAssembler::PlaybackFrameAssembler(AudioFormat output_format,
                                               std::size_t output_frames_per_buffer,
                                               int pipeline_sample_rate)
    : output_format_(output_format),
      pipeline_sample_rate_(pipeline_sample_rate),
      output_frames_per_buffer_(output_frames_per_buffer) {
  if (pipeline_sample_rate_ > 0 && output_format_.sample_rate > 0 && pipeline_sample_rate_ != output_format_.sample_rate) {
    auto resampler = SampleRateConverter::Create(pipeline_sample_rate_, output_format_.sample_rate, 1);
    if (resampler.ok()) {
      resampler_ = std::make_unique<SampleRateConverter>(std::move(resampler.value()));
    }
  }
}

PlaybackFrameAssembler::PlaybackFrameAssembler(PlaybackFrameAssembler&& other) noexcept = default;

PlaybackFrameAssembler& PlaybackFrameAssembler::operator=(PlaybackFrameAssembler&& other) noexcept = default;

PlaybackFrameAssembler::~PlaybackFrameAssembler() = default;

core::Result<std::vector<DeviceAudioBlock>> PlaybackFrameAssembler::PushFrame(const AudioFrame& frame) {
  if (frame.format.channels != 1) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Playback pipeline expects mono audio frames"};
  }
  if (frame.format.sample_rate != pipeline_sample_rate_) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Playback frame sample rate does not match pipeline"};
  }
  if (output_format_.channels <= 0 || output_format_.channels > static_cast<int>(kMaxDeviceBlockChannels)) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Output device channel count exceeds supported limits"};
  }
  if (output_frames_per_buffer_ == 0 || output_frames_per_buffer_ > kMaxDeviceBlockFrames) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Output device frame size exceeds supported limits"};
  }

  auto converted = ResampleToDevice(frame);
  if (!converted.ok()) {
    return converted.error();
  }

  ++stats_.input_frames;
  stats_.resampled_samples += converted.value().size();
  pending_mono_samples_.insert(pending_mono_samples_.end(), converted.value().begin(), converted.value().end());
  return DrainReadyBlocks(false);
}

std::vector<DeviceAudioBlock> PlaybackFrameAssembler::Flush() { return DrainReadyBlocks(true); }

core::Status PlaybackFrameAssembler::Reset() {
  pending_mono_samples_.clear();
  next_sequence_ = 1;
  if (resampler_ != nullptr) {
    auto reset = resampler_->Reset();
    if (!reset.ok()) {
      return reset;
    }
  }
  return core::OkStatus();
}

std::size_t PlaybackFrameAssembler::pending_device_frames() const { return pending_mono_samples_.size(); }

const PlaybackRenderStats& PlaybackFrameAssembler::stats() const { return stats_; }

AudioFormat PlaybackFrameAssembler::output_format() const { return output_format_; }

core::Result<std::vector<float>> PlaybackFrameAssembler::ResampleToDevice(const AudioFrame& frame) {
  std::vector<float> mono(frame.samples.begin(), frame.samples.end());
  if (pipeline_sample_rate_ == output_format_.sample_rate || resampler_ == nullptr) {
    return mono;
  }
  return resampler_->Process(mono);
}

std::vector<DeviceAudioBlock> PlaybackFrameAssembler::DrainReadyBlocks(bool flush_partial) {
  std::vector<DeviceAudioBlock> blocks;
  while (pending_mono_samples_.size() >= output_frames_per_buffer_ ||
         (flush_partial && !pending_mono_samples_.empty())) {
    DeviceAudioBlock block;
    block.sequence = next_sequence_++;
    block.format = output_format_;
    block.frame_count = output_frames_per_buffer_;

    const std::size_t available_frames = std::min<std::size_t>(output_frames_per_buffer_, pending_mono_samples_.size());
    for (std::size_t frame = 0; frame < available_frames; ++frame) {
      const float sample = pending_mono_samples_[frame];
      for (int channel = 0; channel < output_format_.channels; ++channel) {
        block.samples[frame * static_cast<std::size_t>(output_format_.channels) + static_cast<std::size_t>(channel)] =
            sample;
      }
    }

    pending_mono_samples_.erase(pending_mono_samples_.begin(),
                                pending_mono_samples_.begin() + static_cast<std::ptrdiff_t>(available_frames));
    stats_.output_samples += available_frames;
    ++stats_.output_blocks;
    blocks.push_back(block);

    if (!flush_partial && available_frames < output_frames_per_buffer_) {
      break;
    }
  }

  return blocks;
}

}  // namespace daffy::voice
