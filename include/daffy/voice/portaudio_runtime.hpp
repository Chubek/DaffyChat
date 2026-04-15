#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/voice/audio_pipeline.hpp"

namespace daffy::voice {

class PortAudioStreamSession;

enum class AudioDeviceDirection {
  kInput,
  kOutput,
};

struct AudioDeviceDescriptor {
  int index{-1};
  std::string name;
  std::string host_api;
  int max_input_channels{0};
  int max_output_channels{0};
  double default_sample_rate{0.0};
  double default_low_input_latency{0.0};
  double default_low_output_latency{0.0};
  bool is_default_input{false};
  bool is_default_output{false};
};

struct AudioDeviceInventory {
  std::string library_path;
  std::string version_text;
  std::vector<AudioDeviceDescriptor> devices;
};

struct PortAudioStreamRequest {
  int device_index{-1};
  int channel_count{1};
  double sample_rate{static_cast<double>(kPipelineSampleRate)};
  unsigned long frames_per_buffer{static_cast<unsigned long>(kPipelineFrameSamples)};
  double suggested_latency_seconds{0.0};
};

struct VoiceSessionPlan {
  AudioDeviceDescriptor input_device{};
  AudioDeviceDescriptor output_device{};
  StreamPlan capture_plan{};
  StreamPlan playback_plan{};
  PortAudioStreamRequest input_stream{};
  PortAudioStreamRequest output_stream{};
};

using StreamSupportProbe = std::function<bool(AudioDeviceDirection direction, const PortAudioStreamRequest& request)>;

core::Result<AudioDeviceDescriptor> SelectDevice(const AudioDeviceInventory& inventory,
                                                 AudioDeviceDirection direction,
                                                 std::string_view preference);

core::Result<VoiceSessionPlan> BuildVoiceSessionPlan(const AudioDeviceInventory& inventory,
                                                     const VoiceRuntimeConfig& config,
                                                     const StreamSupportProbe& probe);

class PortAudioRuntime {
 public:
  struct Impl;

  PortAudioRuntime();
  PortAudioRuntime(PortAudioRuntime&& other) noexcept;
  PortAudioRuntime& operator=(PortAudioRuntime&& other) noexcept;
  PortAudioRuntime(const PortAudioRuntime&) = delete;
  PortAudioRuntime& operator=(const PortAudioRuntime&) = delete;
  ~PortAudioRuntime();

  static core::Result<PortAudioRuntime> Load();

  [[nodiscard]] bool IsLoaded() const;
  [[nodiscard]] const std::string& library_path() const;

  core::Result<AudioDeviceInventory> EnumerateDevices() const;
  core::Result<VoiceSessionPlan> BuildSessionPlan(const VoiceRuntimeConfig& config) const;
  core::Result<PortAudioStreamSession> OpenSession(const VoiceSessionPlan& plan) const;

 private:
  explicit PortAudioRuntime(std::shared_ptr<Impl> impl);

  std::shared_ptr<Impl> impl_;
};

}  // namespace daffy::voice
