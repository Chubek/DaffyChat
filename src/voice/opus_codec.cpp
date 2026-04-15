#include "daffy/voice/opus_codec.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <dlfcn.h>

#include "opus.h"

namespace daffy::voice {
namespace {

template <typename Function>
Function ResolveSymbol(void* handle, const char* name) {
  return reinterpret_cast<Function>(dlsym(handle, name));
}

core::Error BuildOpusError(const std::string& message) {
  return core::Error{core::ErrorCode::kUnavailable, message};
}

}  // namespace

struct OpusEncoderWrapper::Impl {
  using EncoderCreateFn = OpusEncoder* (*)(opus_int32, int, int, int*);
  using EncoderDestroyFn = void (*)(OpusEncoder*);
  using EncodeFloatFn = opus_int32 (*)(OpusEncoder*, const float*, int, unsigned char*, opus_int32);
  using EncoderCtlFn = int (*)(OpusEncoder*, int, ...);
  using StrErrorFn = const char* (*)(int);

  void* handle{nullptr};
  std::string library_path;
  EncoderCreateFn encoder_create{nullptr};
  EncoderDestroyFn encoder_destroy{nullptr};
  EncodeFloatFn encode_float{nullptr};
  EncoderCtlFn encoder_ctl{nullptr};
  StrErrorFn strerror_fn{nullptr};
  OpusEncoder* encoder{nullptr};
  OpusCodecConfig config{};
};

OpusEncoderWrapper::OpusEncoderWrapper() = default;

OpusEncoderWrapper::OpusEncoderWrapper(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

OpusEncoderWrapper::OpusEncoderWrapper(OpusEncoderWrapper&& other) noexcept = default;

OpusEncoderWrapper& OpusEncoderWrapper::operator=(OpusEncoderWrapper&& other) noexcept = default;

OpusEncoderWrapper::~OpusEncoderWrapper() {
  if (impl_ != nullptr) {
    if (impl_->encoder != nullptr && impl_->encoder_destroy != nullptr) {
      impl_->encoder_destroy(impl_->encoder);
      impl_->encoder = nullptr;
    }
    if (impl_->handle != nullptr) {
      dlclose(impl_->handle);
      impl_->handle = nullptr;
    }
  }
}

core::Result<OpusEncoderWrapper> OpusEncoderWrapper::Create(const OpusCodecConfig& config) {
  const std::vector<const char*> library_names = {
      "libopus.so.0",
      "libopus.so",
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
    return BuildOpusError("Unable to load libopus runtime library");
  }

  auto impl = std::make_unique<Impl>();
  impl->handle = handle;
  impl->library_path = library_path;
  impl->encoder_create = ResolveSymbol<Impl::EncoderCreateFn>(handle, "opus_encoder_create");
  impl->encoder_destroy = ResolveSymbol<Impl::EncoderDestroyFn>(handle, "opus_encoder_destroy");
  impl->encode_float = ResolveSymbol<Impl::EncodeFloatFn>(handle, "opus_encode_float");
  impl->encoder_ctl = ResolveSymbol<Impl::EncoderCtlFn>(handle, "opus_encoder_ctl");
  impl->strerror_fn = ResolveSymbol<Impl::StrErrorFn>(handle, "opus_strerror");
  impl->config = config;

  if (impl->encoder_create == nullptr || impl->encoder_destroy == nullptr || impl->encode_float == nullptr ||
      impl->encoder_ctl == nullptr || impl->strerror_fn == nullptr) {
    dlclose(handle);
    return BuildOpusError("libopus runtime is missing required symbols");
  }

  int error = OPUS_OK;
  impl->encoder = impl->encoder_create(kPipelineSampleRate, 1, OPUS_APPLICATION_VOIP, &error);
  if (impl->encoder == nullptr || error != OPUS_OK) {
    const char* detail = impl->strerror_fn != nullptr ? impl->strerror_fn(error) : "unknown libopus error";
    dlclose(handle);
    return BuildOpusError(std::string("Unable to create Opus encoder: ") + detail);
  }

  if (impl->encoder_ctl(impl->encoder, OPUS_SET_BITRATE(config.bitrate_bps)) != OPUS_OK ||
      impl->encoder_ctl(impl->encoder, OPUS_SET_COMPLEXITY(config.complexity)) != OPUS_OK) {
    dlclose(handle);
    return BuildOpusError("Unable to configure Opus encoder");
  }

  return OpusEncoderWrapper(std::move(impl));
}

bool OpusEncoderWrapper::IsLoaded() const { return impl_ != nullptr; }

const std::string& OpusEncoderWrapper::library_path() const {
  static const std::string kEmpty;
  return impl_ == nullptr ? kEmpty : impl_->library_path;
}

core::Result<OpusPacket> OpusEncoderWrapper::Encode(const AudioFrame& frame) {
  if (impl_ == nullptr || impl_->encoder == nullptr) {
    return core::Error{core::ErrorCode::kStateError, "Opus encoder is not loaded"};
  }
  if (frame.format.sample_rate != kPipelineSampleRate || frame.format.channels != 1) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Opus encoder expects 48 kHz mono frames"};
  }

  std::vector<std::uint8_t> payload(static_cast<std::size_t>(impl_->config.max_packet_size), 0);
  const auto encoded = impl_->encode_float(impl_->encoder,
                                           frame.samples.data(),
                                           static_cast<int>(kPipelineFrameSamples),
                                           payload.data(),
                                           impl_->config.max_packet_size);
  if (encoded < 0) {
    return core::Error{core::ErrorCode::kUnavailable,
                       std::string("Opus encode failed: ") + impl_->strerror_fn(static_cast<int>(encoded))};
  }
  payload.resize(static_cast<std::size_t>(encoded));
  return OpusPacket{frame.sequence, 0, std::move(payload)};
}

core::Status OpusEncoderWrapper::Reset() {
  if (impl_ == nullptr || impl_->encoder == nullptr) {
    return core::Error{core::ErrorCode::kStateError, "Opus encoder is not loaded"};
  }
  if (impl_->encoder_ctl(impl_->encoder, OPUS_RESET_STATE) != OPUS_OK) {
    return BuildOpusError("Unable to reset Opus encoder state");
  }
  return core::OkStatus();
}

struct OpusDecoderWrapper::Impl {
  using DecoderCreateFn = OpusDecoder* (*)(opus_int32, int, int*);
  using DecoderDestroyFn = void (*)(OpusDecoder*);
  using DecodeFloatFn = int (*)(OpusDecoder*, const unsigned char*, opus_int32, float*, int, int);
  using DecoderCtlFn = int (*)(OpusDecoder*, int, ...);
  using StrErrorFn = const char* (*)(int);

  void* handle{nullptr};
  std::string library_path;
  DecoderCreateFn decoder_create{nullptr};
  DecoderDestroyFn decoder_destroy{nullptr};
  DecodeFloatFn decode_float{nullptr};
  DecoderCtlFn decoder_ctl{nullptr};
  StrErrorFn strerror_fn{nullptr};
  OpusDecoder* decoder{nullptr};
};

OpusDecoderWrapper::OpusDecoderWrapper() = default;

OpusDecoderWrapper::OpusDecoderWrapper(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

OpusDecoderWrapper::OpusDecoderWrapper(OpusDecoderWrapper&& other) noexcept = default;

OpusDecoderWrapper& OpusDecoderWrapper::operator=(OpusDecoderWrapper&& other) noexcept = default;

OpusDecoderWrapper::~OpusDecoderWrapper() {
  if (impl_ != nullptr) {
    if (impl_->decoder != nullptr && impl_->decoder_destroy != nullptr) {
      impl_->decoder_destroy(impl_->decoder);
      impl_->decoder = nullptr;
    }
    if (impl_->handle != nullptr) {
      dlclose(impl_->handle);
      impl_->handle = nullptr;
    }
  }
}

core::Result<OpusDecoderWrapper> OpusDecoderWrapper::Create() {
  const std::vector<const char*> library_names = {
      "libopus.so.0",
      "libopus.so",
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
    return BuildOpusError("Unable to load libopus runtime library");
  }

  auto impl = std::make_unique<Impl>();
  impl->handle = handle;
  impl->library_path = library_path;
  impl->decoder_create = ResolveSymbol<Impl::DecoderCreateFn>(handle, "opus_decoder_create");
  impl->decoder_destroy = ResolveSymbol<Impl::DecoderDestroyFn>(handle, "opus_decoder_destroy");
  impl->decode_float = ResolveSymbol<Impl::DecodeFloatFn>(handle, "opus_decode_float");
  impl->decoder_ctl = ResolveSymbol<Impl::DecoderCtlFn>(handle, "opus_decoder_ctl");
  impl->strerror_fn = ResolveSymbol<Impl::StrErrorFn>(handle, "opus_strerror");

  if (impl->decoder_create == nullptr || impl->decoder_destroy == nullptr || impl->decode_float == nullptr ||
      impl->decoder_ctl == nullptr ||
      impl->strerror_fn == nullptr) {
    dlclose(handle);
    return BuildOpusError("libopus runtime is missing required symbols");
  }

  int error = OPUS_OK;
  impl->decoder = impl->decoder_create(kPipelineSampleRate, 1, &error);
  if (impl->decoder == nullptr || error != OPUS_OK) {
    const char* detail = impl->strerror_fn != nullptr ? impl->strerror_fn(error) : "unknown libopus error";
    dlclose(handle);
    return BuildOpusError(std::string("Unable to create Opus decoder: ") + detail);
  }

  return OpusDecoderWrapper(std::move(impl));
}

bool OpusDecoderWrapper::IsLoaded() const { return impl_ != nullptr; }

const std::string& OpusDecoderWrapper::library_path() const {
  static const std::string kEmpty;
  return impl_ == nullptr ? kEmpty : impl_->library_path;
}

core::Result<AudioFrame> OpusDecoderWrapper::Decode(const OpusPacket& packet) {
  if (impl_ == nullptr || impl_->decoder == nullptr) {
    return core::Error{core::ErrorCode::kStateError, "Opus decoder is not loaded"};
  }
  if (packet.payload.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Opus packet payload cannot be empty"};
  }

  AudioFrame frame;
  frame.sequence = packet.sequence;
  frame.format = AudioFormat{kPipelineSampleRate, 1};
  const auto decoded = impl_->decode_float(impl_->decoder,
                                           packet.payload.data(),
                                           static_cast<opus_int32>(packet.payload.size()),
                                           frame.samples.data(),
                                           static_cast<int>(kPipelineFrameSamples),
                                           0);
  if (decoded < 0) {
    return core::Error{core::ErrorCode::kUnavailable,
                       std::string("Opus decode failed: ") + impl_->strerror_fn(decoded)};
  }
  if (decoded != static_cast<int>(kPipelineFrameSamples)) {
    return core::Error{core::ErrorCode::kUnavailable, "Opus decoder returned an unexpected frame size"};
  }
  return frame;
}

core::Status OpusDecoderWrapper::Reset() {
  if (impl_ == nullptr || impl_->decoder == nullptr) {
    return core::Error{core::ErrorCode::kStateError, "Opus decoder is not loaded"};
  }
  if (impl_->decoder_ctl(impl_->decoder, OPUS_RESET_STATE) != OPUS_OK) {
    return BuildOpusError("Unable to reset Opus decoder state");
  }
  return core::OkStatus();
}

}  // namespace daffy::voice
