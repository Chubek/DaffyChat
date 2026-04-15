#include "daffy/voice/media_worker.hpp"

#include <chrono>
#include <utility>

namespace daffy::voice {

core::Result<VoiceMediaWorker> VoiceMediaWorker::Create(const VoiceRuntimeConfig& config,
                                                        const VoiceSessionPlan& session_plan,
                                                        const OpusCodecConfig& opus_config) {
  auto encoder = OpusEncoderWrapper::Create(opus_config);
  if (!encoder.ok()) {
    return encoder.error();
  }

  auto decoder = OpusDecoderWrapper::Create();
  if (!decoder.ok()) {
    return decoder.error();
  }

  CaptureFrameAssembler capture_assembler(session_plan.capture_plan.device_format,
                                          session_plan.capture_plan.pipeline_format.sample_rate,
                                          session_plan.capture_plan.pipeline_frame_samples,
                                          config.enable_noise_suppression);
  PlaybackFrameAssembler playback_assembler(session_plan.playback_plan.device_format,
                                            session_plan.output_stream.frames_per_buffer,
                                            session_plan.playback_plan.pipeline_format.sample_rate);
  PlayoutBuffer playout_buffer(config.playout_buffer_frames, config.max_playout_buffer_frames);

  return VoiceMediaWorker(std::move(capture_assembler),
                          std::move(playback_assembler),
                          std::move(playout_buffer),
                          std::move(encoder.value()),
                          std::move(decoder.value()));
}

VoiceMediaWorker::VoiceMediaWorker(CaptureFrameAssembler capture_assembler,
                                   PlaybackFrameAssembler playback_assembler,
                                   PlayoutBuffer playout_buffer,
                                   OpusEncoderWrapper encoder,
                                   OpusDecoderWrapper decoder)
    : capture_assembler_(std::move(capture_assembler)),
      playback_assembler_(std::move(playback_assembler)),
      playout_buffer_(std::move(playout_buffer)),
      encoder_(std::move(encoder)),
      decoder_(std::move(decoder)) {}

VoiceMediaWorker::VoiceMediaWorker(VoiceMediaWorker&& other) noexcept = default;

VoiceMediaWorker& VoiceMediaWorker::operator=(VoiceMediaWorker&& other) noexcept = default;

VoiceMediaWorker::~VoiceMediaWorker() = default;

core::Result<std::vector<OpusPacket>> VoiceMediaWorker::ProcessCapturedBlock(const DeviceAudioBlock& block) {
  if (block.format.sample_rate != capture_assembler_.input_format().sample_rate ||
      block.format.channels != capture_assembler_.input_format().channels) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Captured block format does not match capture pipeline"};
  }

  auto frames = capture_assembler_.PushInterleaved(block.samples.data(), block.frame_count);
  if (!frames.ok()) {
    return frames.error();
  }

  std::vector<OpusPacket> packets;
  packets.reserve(frames.value().size());
  for (const auto& frame : frames.value()) {
    const auto encode_started = std::chrono::steady_clock::now();
    auto encoded = encoder_.Encode(frame);
    const auto encode_finished = std::chrono::steady_clock::now();
    const auto encode_microseconds =
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(encode_finished - encode_started)
                                       .count());
    if (!encoded.ok()) {
      ++codec_latency_.encode_failures;
      return encoded.error();
    }
    ++codec_latency_.encoded_packets;
    codec_latency_.total_encode_microseconds += encode_microseconds;
    codec_latency_.max_encode_microseconds = std::max(codec_latency_.max_encode_microseconds, encode_microseconds);
    packets.push_back(std::move(encoded.value()));
  }
  return packets;
}

core::Result<std::vector<DeviceAudioBlock>> VoiceMediaWorker::ProcessReceivedPacket(const OpusPacket& packet) {
  const auto decode_started = std::chrono::steady_clock::now();
  auto decoded = decoder_.Decode(packet);
  const auto decode_finished = std::chrono::steady_clock::now();
  const auto decode_microseconds =
      static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(decode_finished - decode_started)
                                     .count());
  if (!decoded.ok()) {
    ++codec_latency_.decode_failures;
    return decoded.error();
  }
  ++codec_latency_.decoded_packets;
  codec_latency_.total_decode_microseconds += decode_microseconds;
  codec_latency_.max_decode_microseconds = std::max(codec_latency_.max_decode_microseconds, decode_microseconds);
  playout_buffer_.Push(decoded.value());

  std::vector<DeviceAudioBlock> blocks;
  while (true) {
    auto ready = playout_buffer_.PopReadyFrame();
    if (!ready.has_value()) {
      break;
    }
    auto rendered = playback_assembler_.PushFrame(*ready);
    if (!rendered.ok()) {
      return rendered.error();
    }
    blocks.insert(blocks.end(), rendered.value().begin(), rendered.value().end());
  }
  return blocks;
}

core::Result<std::size_t> VoiceMediaWorker::PumpCapture(PortAudioStreamSession& session, EncodedAudioSink& sink) {
  std::size_t packets_sent = 0;
  DeviceAudioBlock block;
  while (session.TryPopCapturedBlock(block)) {
    auto packets = ProcessCapturedBlock(block);
    if (!packets.ok()) {
      return packets.error();
    }
    for (const auto& packet : packets.value()) {
      auto sent = sink.SendAudioPacket(packet);
      if (!sent.ok()) {
        return sent.error();
      }
      ++packets_sent;
    }
  }
  return packets_sent;
}

core::Result<std::size_t> VoiceMediaWorker::PumpPlayback(EncodedAudioSource& source, PortAudioStreamSession& session) {
  std::size_t blocks_queued = 0;
  OpusPacket packet;
  while (source.TryPopAudioPacket(packet)) {
    auto blocks = ProcessReceivedPacket(packet);
    if (!blocks.ok()) {
      return blocks.error();
    }
    for (const auto& block : blocks.value()) {
      if (!session.TryQueuePlaybackBlock(block)) {
        return core::Error{core::ErrorCode::kUnavailable, "Playback callback queue is full"};
      }
      ++blocks_queued;
    }
  }
  return blocks_queued;
}

core::Status VoiceMediaWorker::Reset() {
  auto capture_reset = capture_assembler_.Reset();
  if (!capture_reset.ok()) {
    return capture_reset;
  }
  auto playback_reset = playback_assembler_.Reset();
  if (!playback_reset.ok()) {
    return playback_reset;
  }
  auto encoder_reset = encoder_.Reset();
  if (!encoder_reset.ok()) {
    return encoder_reset;
  }
  auto decoder_reset = decoder_.Reset();
  if (!decoder_reset.ok()) {
    return decoder_reset;
  }
  playout_buffer_.Reset();
  return core::OkStatus();
}

const CaptureTransformStats& VoiceMediaWorker::capture_stats() const { return capture_assembler_.stats(); }

const PlaybackRenderStats& VoiceMediaWorker::playback_stats() const { return playback_assembler_.stats(); }

const PlayoutBufferStats& VoiceMediaWorker::playout_stats() const { return playout_buffer_.stats(); }

VoiceMediaTelemetry VoiceMediaWorker::telemetry() const {
  VoiceMediaTelemetry telemetry;
  telemetry.codec = codec_latency_;
  telemetry.playout_underruns = playout_buffer_.stats().underruns;
  return telemetry;
}

}  // namespace daffy::voice
