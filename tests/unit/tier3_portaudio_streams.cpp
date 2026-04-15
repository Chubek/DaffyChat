#include <algorithm>
#include <array>
#include <cassert>

#include "daffy/voice/portaudio_streams.hpp"

int main() {
  daffy::voice::PortAudioCallbackBridge bridge({44100, 2}, 4, {48000, 1}, 4);

  std::array<float, 8> capture_samples = {0.1F, 0.2F, 0.3F, 0.4F, 0.5F, 0.6F, 0.7F, 0.8F};
  assert(bridge.HandleCapture(capture_samples.data(), 4));

  daffy::voice::DeviceAudioBlock captured;
  assert(bridge.TryPopCapturedBlock(captured));
  assert(captured.sequence == 1);
  assert(captured.frame_count == 4);
  assert(captured.format.sample_rate == 44100);
  assert(captured.format.channels == 2);
  assert(captured.samples[0] == 0.1F);
  assert(captured.samples[7] == 0.8F);

  daffy::voice::DeviceAudioBlock playback_one;
  playback_one.sequence = 10;
  playback_one.format = {48000, 1};
  playback_one.frame_count = 4;
  playback_one.samples[0] = 0.1F;
  playback_one.samples[1] = 0.2F;
  playback_one.samples[2] = 0.3F;
  playback_one.samples[3] = 0.4F;

  daffy::voice::DeviceAudioBlock playback_two;
  playback_two.sequence = 11;
  playback_two.format = {48000, 1};
  playback_two.frame_count = 4;
  playback_two.samples[0] = 0.5F;
  playback_two.samples[1] = 0.6F;
  playback_two.samples[2] = 0.7F;
  playback_two.samples[3] = 0.8F;

  assert(bridge.TryQueuePlaybackBlock(playback_one));
  assert(bridge.TryQueuePlaybackBlock(playback_two));

  std::array<float, 6> playback_buffer{};
  bridge.HandlePlayback(playback_buffer.data(), 6);
  assert(playback_buffer[0] == 0.1F);
  assert(playback_buffer[1] == 0.2F);
  assert(playback_buffer[2] == 0.3F);
  assert(playback_buffer[3] == 0.4F);
  assert(playback_buffer[4] == 0.5F);
  assert(playback_buffer[5] == 0.6F);

  std::array<float, 2> playback_tail{};
  bridge.HandlePlayback(playback_tail.data(), 2);
  assert(playback_tail[0] == 0.7F);
  assert(playback_tail[1] == 0.8F);

  std::array<float, 2> playback_underrun{};
  bridge.HandlePlayback(playback_underrun.data(), 2);
  assert(std::all_of(playback_underrun.begin(), playback_underrun.end(), [](float sample) { return sample == 0.0F; }));

  const auto stats = bridge.stats();
  assert(stats.capture_callbacks == 1);
  assert(stats.playback_callbacks == 3);
  assert(stats.captured_blocks == 1);
  assert(stats.played_blocks == 2);
  assert(stats.playback_underruns >= 1);

  return 0;
}
