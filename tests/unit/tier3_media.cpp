#include <cassert>
#include <cmath>
#include <vector>

#include "daffy/voice/audio_processing.hpp"
#include "daffy/voice/media_worker.hpp"
#include "daffy/voice/portaudio_runtime.hpp"

namespace {

daffy::voice::AudioFrame BuildSineFrame(float amplitude, std::uint64_t sequence) {
  daffy::voice::AudioFrame frame;
  frame.sequence = sequence;
  frame.format = {daffy::voice::kPipelineSampleRate, 1};
  for (std::size_t index = 0; index < frame.samples.size(); ++index) {
    frame.samples[index] =
        amplitude * std::sin(static_cast<float>(index) * 2.0F * 3.14159265F / 48.0F);
  }
  return frame;
}

}  // namespace

int main() {
  auto resampler = daffy::voice::SampleRateConverter::Create(44100, 48000, 1);
  assert(resampler.ok());
  std::vector<float> constant(441, 0.25F);
  auto resampled = resampler.value().Process(constant);
  assert(resampled.ok());
  assert(!resampled.value().empty());
  assert(std::fabs(resampled.value().front() - 0.25F) < 0.05F);

  auto suppressor = daffy::voice::NoiseSuppressor::Create();
  assert(suppressor.ok());
  std::vector<float> quiet(daffy::voice::kPipelineFrameSamples, 0.0F);
  auto vad = suppressor.value().ProcessFrame(quiet.data(), quiet.size());
  assert(vad.ok());

  daffy::voice::CaptureFrameAssembler capture_assembler({48000, 1});
  std::vector<float> half_frame(daffy::voice::kPipelineFrameSamples / 2, 0.1F);
  auto partial = capture_assembler.PushInterleaved(half_frame);
  assert(partial.ok());
  assert(partial.value().empty());
  assert(capture_assembler.pending_pipeline_samples() == daffy::voice::kPipelineFrameSamples / 2);
  assert(capture_assembler.Reset().ok());
  assert(capture_assembler.pending_pipeline_samples() == 0);

  daffy::voice::PlayoutBuffer playout(2, 4);
  assert(playout.Push(BuildSineFrame(0.1F, 1)));
  assert(playout.depth() == 1);
  playout.Reset();
  assert(playout.depth() == 0);
  assert(!playout.PopReadyFrame().has_value());

  daffy::voice::PlaybackFrameAssembler renderer({48000, 2}, daffy::voice::kPipelineFrameSamples);
  auto playback_blocks = renderer.PushFrame(BuildSineFrame(0.2F, 1));
  assert(playback_blocks.ok());
  assert(playback_blocks.value().size() == 1);
  assert(playback_blocks.value().front().format.channels == 2);
  assert(std::fabs(playback_blocks.value().front().samples[0] - playback_blocks.value().front().samples[1]) < 0.0001F);

  daffy::voice::PlaybackFrameAssembler buffered_renderer({48000, 1}, daffy::voice::kPipelineFrameSamples * 2);
  auto buffered_blocks = buffered_renderer.PushFrame(BuildSineFrame(0.15F, 2));
  assert(buffered_blocks.ok());
  assert(buffered_blocks.value().empty());
  assert(buffered_renderer.pending_device_frames() == daffy::voice::kPipelineFrameSamples);
  assert(buffered_renderer.Reset().ok());
  assert(buffered_renderer.pending_device_frames() == 0);

  auto encoder = daffy::voice::OpusEncoderWrapper::Create();
  assert(encoder.ok());
  auto decoder = daffy::voice::OpusDecoderWrapper::Create();
  assert(decoder.ok());

  const auto source_frame = BuildSineFrame(0.35F, 7);
  auto packet = encoder.value().Encode(source_frame);
  assert(packet.ok());
  assert(!packet.value().payload.empty());
  auto decoded = decoder.value().Decode(packet.value());
  assert(decoded.ok());
  assert(encoder.value().Reset().ok());
  assert(decoder.value().Reset().ok());
  float decoded_energy = 0.0F;
  for (float sample : decoded.value().samples) {
    decoded_energy += std::fabs(sample);
  }
  assert(decoded_energy > 1.0F);

  daffy::voice::AudioDeviceInventory inventory;
  inventory.devices = {
      {0, "Mic", "ALSA", 1, 0, 48000.0, 0.01, 0.0, true, false},
      {1, "Speakers", "ALSA", 0, 2, 48000.0, 0.0, 0.01, false, true},
  };

  daffy::voice::VoiceRuntimeConfig config;
  config.enable_noise_suppression = false;
  config.playout_buffer_frames = 1;
  config.max_playout_buffer_frames = 2;

  auto session_plan = daffy::voice::BuildVoiceSessionPlan(
      inventory,
      config,
      [](daffy::voice::AudioDeviceDirection, const daffy::voice::PortAudioStreamRequest&) { return true; });
  assert(session_plan.ok());

  auto worker = daffy::voice::VoiceMediaWorker::Create(config, session_plan.value());
  assert(worker.ok());

  daffy::voice::DeviceAudioBlock capture_block;
  capture_block.sequence = 1;
  capture_block.format = session_plan.value().capture_plan.device_format;
  capture_block.frame_count = daffy::voice::kPipelineFrameSamples;
  for (std::size_t index = 0; index < capture_block.frame_count; ++index) {
    capture_block.samples[index] = source_frame.samples[index];
  }

  auto outbound = worker.value().ProcessCapturedBlock(capture_block);
  assert(outbound.ok());
  assert(outbound.value().size() == 1);

  auto inbound = worker.value().ProcessReceivedPacket(outbound.value().front());
  assert(inbound.ok());
  assert(!inbound.value().empty());
  assert(inbound.value().front().format.channels == 1 || inbound.value().front().format.channels == 2);
  assert(worker.value().Reset().ok());
  const auto telemetry = worker.value().telemetry();
  assert(telemetry.codec.encoded_packets >= 1);
  assert(telemetry.codec.decoded_packets >= 1);

  return 0;
}
