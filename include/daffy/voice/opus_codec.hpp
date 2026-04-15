#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/voice/audio_pipeline.hpp"

namespace daffy::voice {

struct OpusPacket {
  std::uint64_t sequence{0};
  std::uint32_t rtp_timestamp{0};
  std::vector<std::uint8_t> payload;
};

struct OpusCodecConfig {
  int bitrate_bps{32000};
  int complexity{5};
  int max_packet_size{4000};
};

class OpusEncoderWrapper {
 public:
  struct Impl;

  OpusEncoderWrapper();
  OpusEncoderWrapper(OpusEncoderWrapper&& other) noexcept;
  OpusEncoderWrapper& operator=(OpusEncoderWrapper&& other) noexcept;
  OpusEncoderWrapper(const OpusEncoderWrapper&) = delete;
  OpusEncoderWrapper& operator=(const OpusEncoderWrapper&) = delete;
  ~OpusEncoderWrapper();

  static core::Result<OpusEncoderWrapper> Create(const OpusCodecConfig& config = {});

  [[nodiscard]] bool IsLoaded() const;
  [[nodiscard]] const std::string& library_path() const;
  core::Result<OpusPacket> Encode(const AudioFrame& frame);
  core::Status Reset();

 private:
  explicit OpusEncoderWrapper(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

class OpusDecoderWrapper {
 public:
  struct Impl;

  OpusDecoderWrapper();
  OpusDecoderWrapper(OpusDecoderWrapper&& other) noexcept;
  OpusDecoderWrapper& operator=(OpusDecoderWrapper&& other) noexcept;
  OpusDecoderWrapper(const OpusDecoderWrapper&) = delete;
  OpusDecoderWrapper& operator=(const OpusDecoderWrapper&) = delete;
  ~OpusDecoderWrapper();

  static core::Result<OpusDecoderWrapper> Create();

  [[nodiscard]] bool IsLoaded() const;
  [[nodiscard]] const std::string& library_path() const;
  core::Result<AudioFrame> Decode(const OpusPacket& packet);
  core::Status Reset();

 private:
  explicit OpusDecoderWrapper(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace daffy::voice
