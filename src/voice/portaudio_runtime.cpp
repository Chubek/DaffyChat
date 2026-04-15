#include "daffy/voice/portaudio_runtime.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <dlfcn.h>

#include "daffy/voice/detail/portaudio_internal.hpp"
#include "daffy/voice/portaudio_streams.hpp"
#include "portaudio.h"

namespace daffy::voice {
namespace {

std::string ToLower(std::string_view text) {
  std::string lowered;
  lowered.reserve(text.size());
  for (char character : text) {
    lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(character))));
  }
  return lowered;
}

bool SupportsDirection(const AudioDeviceDescriptor& device, AudioDeviceDirection direction) {
  return direction == AudioDeviceDirection::kInput ? device.max_input_channels > 0 : device.max_output_channels > 0;
}

std::vector<double> CandidateSampleRates(double preferred_rate, double default_rate) {
  std::vector<double> candidates;
  auto append = [&](double value) {
    if (value <= 0.0) {
      return;
    }
    for (double existing : candidates) {
      if (std::fabs(existing - value) < 0.5) {
        return;
      }
    }
    candidates.push_back(value);
  };

  append(preferred_rate);
  append(default_rate);
  append(static_cast<double>(kPipelineSampleRate));
  return candidates;
}

int ResolveChannelCount(AudioDeviceDirection direction, const AudioDeviceDescriptor& device, int preferred_channels) {
  const int max_channels = direction == AudioDeviceDirection::kInput ? device.max_input_channels : device.max_output_channels;
  if (max_channels <= 0) {
    return 0;
  }
  return std::max(1, std::min(preferred_channels <= 0 ? 1 : preferred_channels, max_channels));
}

PortAudioStreamRequest BuildRequest(AudioDeviceDirection direction,
                                    const AudioDeviceDescriptor& device,
                                    int preferred_channels,
                                    int frames_per_buffer,
                                    double sample_rate) {
  PortAudioStreamRequest request;
  request.device_index = device.index;
  request.channel_count = ResolveChannelCount(direction, device, preferred_channels);
  request.sample_rate = sample_rate;
  request.frames_per_buffer = static_cast<unsigned long>(std::max(1, frames_per_buffer));
  request.suggested_latency_seconds =
      direction == AudioDeviceDirection::kInput ? device.default_low_input_latency : device.default_low_output_latency;
  return request;
}

}  // namespace

core::Result<AudioDeviceDescriptor> SelectDevice(const AudioDeviceInventory& inventory,
                                                 AudioDeviceDirection direction,
                                                 std::string_view preference) {
  const std::string lowered_preference = ToLower(preference);
  const bool wants_default = lowered_preference.empty() || lowered_preference == "default";

  auto supports_direction = [&](const AudioDeviceDescriptor& device) {
    return SupportsDirection(device, direction);
  };

  if (!wants_default) {
    for (const auto& device : inventory.devices) {
      if (supports_direction(device) && ToLower(device.name) == lowered_preference) {
        return device;
      }
    }
    for (const auto& device : inventory.devices) {
      if (supports_direction(device) && ToLower(device.name).find(lowered_preference) != std::string::npos) {
        return device;
      }
    }
  }

  for (const auto& device : inventory.devices) {
    if (!supports_direction(device)) {
      continue;
    }
    if ((direction == AudioDeviceDirection::kInput && device.is_default_input) ||
        (direction == AudioDeviceDirection::kOutput && device.is_default_output)) {
      return device;
    }
  }

  for (const auto& device : inventory.devices) {
    if (supports_direction(device)) {
      return device;
    }
  }

  return core::Error{core::ErrorCode::kNotFound,
                     direction == AudioDeviceDirection::kInput ? "No input audio device available"
                                                              : "No output audio device available"};
}

core::Result<VoiceSessionPlan> BuildVoiceSessionPlan(const AudioDeviceInventory& inventory,
                                                     const VoiceRuntimeConfig& config,
                                                     const StreamSupportProbe& probe) {
  auto input_device = SelectDevice(inventory, AudioDeviceDirection::kInput, config.preferred_input_device);
  if (!input_device.ok()) {
    return input_device.error();
  }

  auto output_device = SelectDevice(inventory, AudioDeviceDirection::kOutput, config.preferred_output_device);
  if (!output_device.ok()) {
    return output_device.error();
  }

  auto select_supported_request = [&](AudioDeviceDirection direction,
                                      const AudioDeviceDescriptor& device,
                                      int preferred_rate) -> core::Result<PortAudioStreamRequest> {
    const auto candidates = CandidateSampleRates(static_cast<double>(preferred_rate), device.default_sample_rate);
    for (double sample_rate : candidates) {
      auto request = BuildRequest(direction, device, config.preferred_channels, config.frames_per_buffer, sample_rate);
      if (request.channel_count <= 0) {
        return core::Error{core::ErrorCode::kInvalidArgument, "Selected device does not support any channels"};
      }
      if (!probe || probe(direction, request)) {
        return request;
      }
    }

    return core::Error{core::ErrorCode::kUnavailable,
                       std::string("No supported ") +
                           (direction == AudioDeviceDirection::kInput ? "input" : "output") +
                           " stream format for selected device"};
  };

  auto input_request = select_supported_request(AudioDeviceDirection::kInput,
                                                input_device.value(),
                                                config.preferred_capture_sample_rate);
  if (!input_request.ok()) {
    return input_request.error();
  }

  auto output_request = select_supported_request(AudioDeviceDirection::kOutput,
                                                 output_device.value(),
                                                 config.preferred_playback_sample_rate);
  if (!output_request.ok()) {
    return output_request.error();
  }

  auto capture_plan = BuildCaptureStreamPlan(
      {static_cast<int>(std::lround(input_request.value().sample_rate)), input_request.value().channel_count},
      static_cast<int>(input_request.value().frames_per_buffer));
  if (!capture_plan.ok()) {
    return capture_plan.error();
  }

  auto playback_plan = BuildCaptureStreamPlan(
      {static_cast<int>(std::lround(output_request.value().sample_rate)), output_request.value().channel_count},
      static_cast<int>(output_request.value().frames_per_buffer));
  if (!playback_plan.ok()) {
    return playback_plan.error();
  }

  VoiceSessionPlan plan;
  plan.input_device = input_device.value();
  plan.output_device = output_device.value();
  plan.capture_plan = capture_plan.value();
  plan.playback_plan = playback_plan.value();
  plan.input_stream = input_request.value();
  plan.output_stream = output_request.value();
  return plan;
}

namespace {

template <typename Function>
Function ResolveSymbol(void* handle, const char* name) {
  return reinterpret_cast<Function>(dlsym(handle, name));
}

core::Result<std::shared_ptr<PortAudioRuntime::Impl>> LoadPortAudioImpl() {
  const std::vector<const char*> library_names = {
      "libportaudio.so.2",
      "libportaudio.so",
  };

  void* handle = nullptr;
  std::string library_path;
  for (const char* library_name : library_names) {
    handle = dlopen(library_name, RTLD_NOW | RTLD_LOCAL);
    if (handle != nullptr) {
      library_path = library_name;
      break;
    }
  }

  if (handle == nullptr) {
    return core::Error{core::ErrorCode::kUnavailable, "Unable to load PortAudio runtime library"};
  }

  auto impl = std::make_shared<PortAudioRuntime::Impl>();
  impl->handle = handle;
  impl->library_path = library_path;
  impl->get_version_info = ResolveSymbol<PortAudioRuntime::Impl::GetVersionInfoFn>(handle, "Pa_GetVersionInfo");
  impl->initialize = ResolveSymbol<PortAudioRuntime::Impl::InitializeFn>(handle, "Pa_Initialize");
  impl->terminate = ResolveSymbol<PortAudioRuntime::Impl::TerminateFn>(handle, "Pa_Terminate");
  impl->get_device_count = ResolveSymbol<PortAudioRuntime::Impl::GetDeviceCountFn>(handle, "Pa_GetDeviceCount");
  impl->get_default_input_device =
      ResolveSymbol<PortAudioRuntime::Impl::GetDefaultDeviceFn>(handle, "Pa_GetDefaultInputDevice");
  impl->get_default_output_device =
      ResolveSymbol<PortAudioRuntime::Impl::GetDefaultDeviceFn>(handle, "Pa_GetDefaultOutputDevice");
  impl->get_device_info = ResolveSymbol<PortAudioRuntime::Impl::GetDeviceInfoFn>(handle, "Pa_GetDeviceInfo");
  impl->get_host_api_info = ResolveSymbol<PortAudioRuntime::Impl::GetHostApiInfoFn>(handle, "Pa_GetHostApiInfo");
  impl->is_format_supported =
      ResolveSymbol<PortAudioRuntime::Impl::IsFormatSupportedFn>(handle, "Pa_IsFormatSupported");
  impl->open_stream = ResolveSymbol<PortAudioRuntime::Impl::OpenStreamFn>(handle, "Pa_OpenStream");
  impl->start_stream = ResolveSymbol<PortAudioRuntime::Impl::StartStreamFn>(handle, "Pa_StartStream");
  impl->stop_stream = ResolveSymbol<PortAudioRuntime::Impl::StopStreamFn>(handle, "Pa_StopStream");
  impl->abort_stream = ResolveSymbol<PortAudioRuntime::Impl::AbortStreamFn>(handle, "Pa_AbortStream");
  impl->close_stream = ResolveSymbol<PortAudioRuntime::Impl::CloseStreamFn>(handle, "Pa_CloseStream");
  impl->is_stream_active = ResolveSymbol<PortAudioRuntime::Impl::IsStreamActiveFn>(handle, "Pa_IsStreamActive");
  impl->get_error_text = ResolveSymbol<PortAudioRuntime::Impl::GetErrorTextFn>(handle, "Pa_GetErrorText");

  if (impl->get_version_info == nullptr || impl->initialize == nullptr || impl->terminate == nullptr ||
      impl->get_device_count == nullptr || impl->get_default_input_device == nullptr ||
      impl->get_default_output_device == nullptr || impl->get_device_info == nullptr ||
      impl->get_host_api_info == nullptr || impl->is_format_supported == nullptr || impl->open_stream == nullptr ||
      impl->start_stream == nullptr || impl->stop_stream == nullptr || impl->abort_stream == nullptr ||
      impl->close_stream == nullptr || impl->is_stream_active == nullptr || impl->get_error_text == nullptr) {
    dlclose(handle);
    return core::Error{core::ErrorCode::kUnavailable, "PortAudio runtime is missing required symbols"};
  }

  return impl;
}

class ScopedPortAudioInitialization {
 public:
  explicit ScopedPortAudioInitialization(const PortAudioRuntime::Impl& impl) : impl_(impl) {
    const auto error = impl_.initialize();
    if (error != paNoError) {
      error_ = core::Error{core::ErrorCode::kUnavailable,
                           std::string("PortAudio initialization failed: ") + impl_.get_error_text(error)};
      return;
    }
    initialized_ = true;
  }

  ~ScopedPortAudioInitialization() {
    if (initialized_) {
      impl_.terminate();
    }
  }

  [[nodiscard]] bool ok() const { return initialized_; }
  [[nodiscard]] const core::Error& error() const { return error_; }

 private:
  const PortAudioRuntime::Impl& impl_;
  bool initialized_{false};
  core::Error error_{core::ErrorCode::kStateError, "PortAudio not initialized"};
};

}  // namespace

PortAudioRuntime::Impl::~Impl() {
  if (handle != nullptr) {
    dlclose(handle);
    handle = nullptr;
  }
}

PortAudioRuntime::PortAudioRuntime() = default;

PortAudioRuntime::PortAudioRuntime(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}

PortAudioRuntime::PortAudioRuntime(PortAudioRuntime&& other) noexcept = default;

PortAudioRuntime& PortAudioRuntime::operator=(PortAudioRuntime&& other) noexcept = default;

PortAudioRuntime::~PortAudioRuntime() = default;

core::Result<PortAudioRuntime> PortAudioRuntime::Load() {
  auto impl = LoadPortAudioImpl();
  if (!impl.ok()) {
    return impl.error();
  }
  return PortAudioRuntime(impl.value());
}

bool PortAudioRuntime::IsLoaded() const { return impl_ != nullptr; }

const std::string& PortAudioRuntime::library_path() const {
  static const std::string kEmpty;
  return impl_ == nullptr ? kEmpty : impl_->library_path;
}

core::Result<AudioDeviceInventory> PortAudioRuntime::EnumerateDevices() const {
  if (impl_ == nullptr) {
    return core::Error{core::ErrorCode::kStateError, "PortAudio runtime is not loaded"};
  }

  ScopedPortAudioInitialization initialization(*impl_);
  if (!initialization.ok()) {
    return initialization.error();
  }

  AudioDeviceInventory inventory;
  inventory.library_path = impl_->library_path;
  const auto* version_info = impl_->get_version_info();
  if (version_info != nullptr && version_info->versionText != nullptr) {
    inventory.version_text = version_info->versionText;
  }

  const PaDeviceIndex default_input = impl_->get_default_input_device();
  const PaDeviceIndex default_output = impl_->get_default_output_device();
  const PaDeviceIndex device_count = impl_->get_device_count();
  if (device_count < 0) {
    return core::Error{core::ErrorCode::kUnavailable,
                       std::string("PortAudio failed to enumerate devices: ") + impl_->get_error_text(device_count)};
  }

  inventory.devices.reserve(static_cast<std::size_t>(device_count));
  for (PaDeviceIndex index = 0; index < device_count; ++index) {
    const auto* device_info = impl_->get_device_info(index);
    if (device_info == nullptr) {
      continue;
    }

    AudioDeviceDescriptor device;
    device.index = index;
    device.name = device_info->name != nullptr ? device_info->name : "unknown-device";
    device.max_input_channels = device_info->maxInputChannels;
    device.max_output_channels = device_info->maxOutputChannels;
    device.default_sample_rate = device_info->defaultSampleRate;
    device.default_low_input_latency = device_info->defaultLowInputLatency;
    device.default_low_output_latency = device_info->defaultLowOutputLatency;
    device.is_default_input = index == default_input;
    device.is_default_output = index == default_output;

    const auto* host_api = impl_->get_host_api_info(device_info->hostApi);
    if (host_api != nullptr && host_api->name != nullptr) {
      device.host_api = host_api->name;
    }

    inventory.devices.push_back(std::move(device));
  }

  return inventory;
}

core::Result<VoiceSessionPlan> PortAudioRuntime::BuildSessionPlan(const VoiceRuntimeConfig& config) const {
  if (impl_ == nullptr) {
    return core::Error{core::ErrorCode::kStateError, "PortAudio runtime is not loaded"};
  }

  auto inventory = EnumerateDevices();
  if (!inventory.ok()) {
    return inventory.error();
  }

  auto probe = [this](AudioDeviceDirection direction, const PortAudioStreamRequest& request) {
    ScopedPortAudioInitialization initialization(*impl_);
    if (!initialization.ok()) {
      return false;
    }

    PaStreamParameters input{};
    PaStreamParameters output{};
    const PaStreamParameters* input_ptr = nullptr;
    const PaStreamParameters* output_ptr = nullptr;

    if (direction == AudioDeviceDirection::kInput) {
      input.device = request.device_index;
      input.channelCount = request.channel_count;
      input.sampleFormat = paFloat32;
      input.suggestedLatency = request.suggested_latency_seconds;
      input.hostApiSpecificStreamInfo = nullptr;
      input_ptr = &input;
    } else {
      output.device = request.device_index;
      output.channelCount = request.channel_count;
      output.sampleFormat = paFloat32;
      output.suggestedLatency = request.suggested_latency_seconds;
      output.hostApiSpecificStreamInfo = nullptr;
      output_ptr = &output;
    }

    return impl_->is_format_supported(input_ptr, output_ptr, request.sample_rate) == paFormatIsSupported;
  };

  return BuildVoiceSessionPlan(inventory.value(), config, probe);
}

core::Result<PortAudioStreamSession> PortAudioRuntime::OpenSession(const VoiceSessionPlan& plan) const {
  if (impl_ == nullptr) {
    return core::Error{core::ErrorCode::kStateError, "PortAudio runtime is not loaded"};
  }
  return OpenPortAudioStreamSession(impl_, plan);
}

}  // namespace daffy::voice
