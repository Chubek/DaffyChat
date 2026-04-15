#pragma once

#include <cstddef>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/voice/audio_pipeline.hpp"
#include "daffy/voice/opus_codec.hpp"
#include "daffy/voice/portaudio_streams.hpp"

namespace daffy::voice {

class EncodedAudioSink {
 public:
  virtual ~EncodedAudioSink() = default;
  virtual core::Status SendAudioPacket(const OpusPacket& packet) = 0;
};

class EncodedAudioSource {
 public:
  virtual ~EncodedAudioSource() = default;
  virtual bool TryPopAudioPacket(OpusPacket& packet) = 0;
};

struct CodecLatencyStats {
  std::uint64_t encoded_packets{0};
  std::uint64_t decoded_packets{0};
  std::uint64_t encode_failures{0};
  std::uint64_t decode_failures{0};
  std::uint64_t total_encode_microseconds{0};
  std::uint64_t total_decode_microseconds{0};
  std::uint64_t max_encode_microseconds{0};
  std::uint64_t max_decode_microseconds{0};
};

struct VoiceMediaTelemetry {
  CodecLatencyStats codec{};
  std::uint64_t playout_underruns{0};
};

class VoiceMediaWorker {
 public:
  static core::Result<VoiceMediaWorker> Create(const VoiceRuntimeConfig& config,
                                               const VoiceSessionPlan& session_plan,
                                               const OpusCodecConfig& opus_config = {});

  VoiceMediaWorker(VoiceMediaWorker&& other) noexcept;
  VoiceMediaWorker& operator=(VoiceMediaWorker&& other) noexcept;
  VoiceMediaWorker(const VoiceMediaWorker&) = delete;
  VoiceMediaWorker& operator=(const VoiceMediaWorker&) = delete;
  ~VoiceMediaWorker();

  core::Result<std::vector<OpusPacket>> ProcessCapturedBlock(const DeviceAudioBlock& block);
  core::Result<std::vector<DeviceAudioBlock>> ProcessReceivedPacket(const OpusPacket& packet);
  core::Result<std::size_t> PumpCapture(PortAudioStreamSession& session, EncodedAudioSink& sink);
  core::Result<std::size_t> PumpPlayback(EncodedAudioSource& source, PortAudioStreamSession& session);
  core::Status Reset();

  [[nodiscard]] const CaptureTransformStats& capture_stats() const;
  [[nodiscard]] const PlaybackRenderStats& playback_stats() const;
  [[nodiscard]] const PlayoutBufferStats& playout_stats() const;
  [[nodiscard]] VoiceMediaTelemetry telemetry() const;

 private:
  VoiceMediaWorker(CaptureFrameAssembler capture_assembler,
                   PlaybackFrameAssembler playback_assembler,
                   PlayoutBuffer playout_buffer,
                   OpusEncoderWrapper encoder,
                   OpusDecoderWrapper decoder);

  CaptureFrameAssembler capture_assembler_;
  PlaybackFrameAssembler playback_assembler_;
  PlayoutBuffer playout_buffer_;
  OpusEncoderWrapper encoder_;
  OpusDecoderWrapper decoder_;
  CodecLatencyStats codec_latency_{};
};

}  // namespace daffy::voice
