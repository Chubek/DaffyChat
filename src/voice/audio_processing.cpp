#include "daffy/voice/audio_processing.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <dlfcn.h>

#include "daffy/voice/audio_pipeline.hpp"
#include "rnnoise.h"
#include "samplerate.h"

namespace daffy::voice {
namespace {

template <typename Function>
Function ResolveSymbol(void* handle, const char* name) {
  return reinterpret_cast<Function>(dlsym(handle, name));
}

core::Error BuildDlError(const std::string& message) { return core::Error{core::ErrorCode::kUnavailable, message}; }

}  // namespace

struct SampleRateConverter::Impl {
  using NewFn = SRC_STATE* (*)(int, int, int*);
  using DeleteFn = SRC_STATE* (*)(SRC_STATE*);
  using ProcessFn = int (*)(SRC_STATE*, SRC_DATA*);
  using ResetFn = int (*)(SRC_STATE*);
  using StrErrorFn = const char* (*)(int);

  void* handle{nullptr};
  std::string library_path;
  NewFn src_new{nullptr};
  DeleteFn src_delete{nullptr};
  ProcessFn src_process{nullptr};
  ResetFn src_reset{nullptr};
  StrErrorFn src_strerror{nullptr};
  SRC_STATE* state{nullptr};
  int input_sample_rate{kPipelineSampleRate};
  int output_sample_rate{kPipelineSampleRate};
  int channels{1};
};

SampleRateConverter::SampleRateConverter() = default;

SampleRateConverter::SampleRateConverter(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

SampleRateConverter::SampleRateConverter(SampleRateConverter&& other) noexcept = default;

SampleRateConverter& SampleRateConverter::operator=(SampleRateConverter&& other) noexcept = default;

SampleRateConverter::~SampleRateConverter() {
  if (impl_ != nullptr) {
    if (impl_->state != nullptr && impl_->src_delete != nullptr) {
      impl_->src_delete(impl_->state);
      impl_->state = nullptr;
    }
    if (impl_->handle != nullptr) {
      dlclose(impl_->handle);
      impl_->handle = nullptr;
    }
  }
}

core::Result<SampleRateConverter> SampleRateConverter::Create(int input_sample_rate,
                                                              int output_sample_rate,
                                                              int channels) {
  if (input_sample_rate <= 0 || output_sample_rate <= 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Sample rates must be positive"};
  }
  if (channels <= 0) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Channel count must be positive"};
  }

  const std::vector<const char*> library_names = {
      "libsamplerate.so.0",
      "libsamplerate.so",
  };

  void* handle = nullptr;
  std::string library_path;
  for (const char* library_name : library_names) {
    dlerror();
    handle = dlopen(library_name, RTLD_NOW | RTLD_LOCAL);
    if (handle != nullptr) {
      library_path = library_name;
      break;
    }
  }

  if (handle == nullptr) {
    return BuildDlError("Unable to load libsamplerate runtime library");
  }

  auto impl = std::make_unique<Impl>();
  impl->handle = handle;
  impl->library_path = library_path;
  impl->src_new = ResolveSymbol<Impl::NewFn>(handle, "src_new");
  impl->src_delete = ResolveSymbol<Impl::DeleteFn>(handle, "src_delete");
  impl->src_process = ResolveSymbol<Impl::ProcessFn>(handle, "src_process");
  impl->src_reset = ResolveSymbol<Impl::ResetFn>(handle, "src_reset");
  impl->src_strerror = ResolveSymbol<Impl::StrErrorFn>(handle, "src_strerror");
  impl->input_sample_rate = input_sample_rate;
  impl->output_sample_rate = output_sample_rate;
  impl->channels = channels;

  if (impl->src_new == nullptr || impl->src_delete == nullptr || impl->src_process == nullptr ||
      impl->src_reset == nullptr || impl->src_strerror == nullptr) {
    dlclose(handle);
    return BuildDlError("libsamplerate runtime is missing required symbols");
  }

  int error = 0;
  impl->state = impl->src_new(SRC_SINC_FASTEST, channels, &error);
  if (impl->state == nullptr || error != 0) {
    const char* detail = impl->src_strerror != nullptr ? impl->src_strerror(error) : "unknown libsamplerate error";
    dlclose(handle);
    return core::Error{core::ErrorCode::kUnavailable,
                       std::string("Unable to create libsamplerate state: ") + detail};
  }

  return SampleRateConverter(std::move(impl));
}

bool SampleRateConverter::IsLoaded() const { return impl_ != nullptr; }

const std::string& SampleRateConverter::library_path() const {
  static const std::string kEmpty;
  return impl_ == nullptr ? kEmpty : impl_->library_path;
}

int SampleRateConverter::input_sample_rate() const {
  return impl_ == nullptr ? kPipelineSampleRate : impl_->input_sample_rate;
}

int SampleRateConverter::output_sample_rate() const {
  return impl_ == nullptr ? kPipelineSampleRate : impl_->output_sample_rate;
}

core::Result<std::vector<float>> SampleRateConverter::Process(const float* samples,
                                                              std::size_t frame_count,
                                                              bool end_of_input) {
  if (impl_ == nullptr || impl_->state == nullptr) {
    return core::Error{core::ErrorCode::kStateError, "libsamplerate converter is not loaded"};
  }
  if (frame_count == 0) {
    return std::vector<float>{};
  }
  if (samples == nullptr) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Resampler input cannot be null"};
  }

  const double ratio = static_cast<double>(impl_->output_sample_rate) / static_cast<double>(impl_->input_sample_rate);
  const auto estimated_output_frames =
      std::max<std::size_t>(1, static_cast<std::size_t>(std::ceil(static_cast<double>(frame_count) * ratio)) + 64U);

  std::vector<float> output(estimated_output_frames * static_cast<std::size_t>(impl_->channels), 0.0F);

  SRC_DATA data{};
  data.data_in = samples;
  data.input_frames = static_cast<long>(frame_count);
  data.data_out = output.data();
  data.output_frames = static_cast<long>(estimated_output_frames);
  data.end_of_input = end_of_input ? 1 : 0;
  data.src_ratio = ratio;

  const int error = impl_->src_process(impl_->state, &data);
  if (error != 0) {
    return core::Error{core::ErrorCode::kUnavailable,
                       std::string("libsamplerate conversion failed: ") + impl_->src_strerror(error)};
  }

  output.resize(static_cast<std::size_t>(data.output_frames_gen) * static_cast<std::size_t>(impl_->channels));
  return output;
}

core::Result<std::vector<float>> SampleRateConverter::Process(const std::vector<float>& samples, bool end_of_input) {
  if (samples.empty()) {
    return std::vector<float>{};
  }
  if (impl_ == nullptr) {
    return core::Error{core::ErrorCode::kStateError, "libsamplerate converter is not loaded"};
  }
  if (samples.size() % static_cast<std::size_t>(impl_->channels) != 0) {
    return core::Error{core::ErrorCode::kInvalidArgument,
                       "Resampler input sample count must be divisible by the channel count"};
  }
  return Process(samples.data(), samples.size() / static_cast<std::size_t>(impl_->channels), end_of_input);
}

core::Status SampleRateConverter::Reset() {
  if (impl_ == nullptr || impl_->state == nullptr) {
    return core::Error{core::ErrorCode::kStateError, "libsamplerate converter is not loaded"};
  }
  const int error = impl_->src_reset(impl_->state);
  if (error != 0) {
    return core::Error{core::ErrorCode::kUnavailable,
                       std::string("libsamplerate reset failed: ") + impl_->src_strerror(error)};
  }
  return core::OkStatus();
}

struct NoiseSuppressor::Impl {
  using CreateFn = DenoiseState* (*)(RNNModel*);
  using DestroyFn = void (*)(DenoiseState*);
  using ProcessFrameFn = float (*)(DenoiseState*, float*, const float*);
  using GetFrameSizeFn = int (*)();

  void* handle{nullptr};
  std::string library_path;
  CreateFn create{nullptr};
  DestroyFn destroy{nullptr};
  ProcessFrameFn process_frame{nullptr};
  GetFrameSizeFn get_frame_size{nullptr};
  DenoiseState* state{nullptr};
  std::size_t frame_size{kPipelineFrameSamples};
};

NoiseSuppressor::NoiseSuppressor() = default;

NoiseSuppressor::NoiseSuppressor(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

NoiseSuppressor::NoiseSuppressor(NoiseSuppressor&& other) noexcept = default;

NoiseSuppressor& NoiseSuppressor::operator=(NoiseSuppressor&& other) noexcept = default;

NoiseSuppressor::~NoiseSuppressor() {
  if (impl_ != nullptr) {
    if (impl_->state != nullptr && impl_->destroy != nullptr) {
      impl_->destroy(impl_->state);
      impl_->state = nullptr;
    }
    if (impl_->handle != nullptr) {
      dlclose(impl_->handle);
      impl_->handle = nullptr;
    }
  }
}

core::Result<NoiseSuppressor> NoiseSuppressor::Create() {
  const std::vector<const char*> library_names = {
      "librnnoise.so.0",
      "librnnoise.so",
  };

  void* handle = nullptr;
  std::string library_path;
  for (const char* library_name : library_names) {
    dlerror();
    handle = dlopen(library_name, RTLD_NOW | RTLD_LOCAL);
    if (handle != nullptr) {
      library_path = library_name;
      break;
    }
  }

  if (handle == nullptr) {
    return BuildDlError("Unable to load rnnoise runtime library");
  }

  auto impl = std::make_unique<Impl>();
  impl->handle = handle;
  impl->library_path = library_path;
  impl->create = ResolveSymbol<Impl::CreateFn>(handle, "rnnoise_create");
  impl->destroy = ResolveSymbol<Impl::DestroyFn>(handle, "rnnoise_destroy");
  impl->process_frame = ResolveSymbol<Impl::ProcessFrameFn>(handle, "rnnoise_process_frame");
  impl->get_frame_size = ResolveSymbol<Impl::GetFrameSizeFn>(handle, "rnnoise_get_frame_size");

  if (impl->create == nullptr || impl->destroy == nullptr || impl->process_frame == nullptr ||
      impl->get_frame_size == nullptr) {
    dlclose(handle);
    return BuildDlError("rnnoise runtime is missing required symbols");
  }

  const int frame_size = impl->get_frame_size();
  if (frame_size <= 0) {
    dlclose(handle);
    return core::Error{core::ErrorCode::kUnavailable, "rnnoise reported an invalid frame size"};
  }
  impl->frame_size = static_cast<std::size_t>(frame_size);
  impl->state = impl->create(nullptr);
  if (impl->state == nullptr) {
    dlclose(handle);
    return core::Error{core::ErrorCode::kUnavailable, "Unable to create rnnoise state"};
  }

  return NoiseSuppressor(std::move(impl));
}

bool NoiseSuppressor::IsLoaded() const { return impl_ != nullptr; }

const std::string& NoiseSuppressor::library_path() const {
  static const std::string kEmpty;
  return impl_ == nullptr ? kEmpty : impl_->library_path;
}

std::size_t NoiseSuppressor::frame_samples() const {
  return impl_ == nullptr ? kPipelineFrameSamples : impl_->frame_size;
}

core::Result<float> NoiseSuppressor::ProcessFrame(float* samples, std::size_t sample_count) {
  if (impl_ == nullptr || impl_->state == nullptr) {
    return core::Error{core::ErrorCode::kStateError, "rnnoise suppressor is not loaded"};
  }
  if (samples == nullptr) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Noise suppressor input cannot be null"};
  }
  if (sample_count != impl_->frame_size) {
    return core::Error{core::ErrorCode::kInvalidArgument, "rnnoise input frame size does not match runtime"};
  }

  std::array<float, kPipelineFrameSamples> processed{};
  if (sample_count > processed.size()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "rnnoise frame size exceeds pipeline frame size"};
  }

  const float vad_probability = impl_->process_frame(impl_->state, processed.data(), samples);
  std::copy_n(processed.begin(), static_cast<std::ptrdiff_t>(sample_count), samples);
  return vad_probability;
}

}  // namespace daffy::voice
