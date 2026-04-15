#include <cassert>

#include "daffy/voice/portaudio_runtime.hpp"

int main() {
  daffy::voice::AudioDeviceInventory inventory;
  inventory.devices = {
      {0, "Studio Mic", "ALSA", 2, 0, 44100.0, 0.01, 0.0, true, false},
      {1, "Desk Speakers", "ALSA", 0, 2, 48000.0, 0.0, 0.02, false, true},
      {2, "USB Headset", "PulseAudio", 1, 2, 48000.0, 0.01, 0.01, false, false},
  };

  auto selected_input = daffy::voice::SelectDevice(inventory, daffy::voice::AudioDeviceDirection::kInput, "usb");
  assert(selected_input.ok());
  assert(selected_input.value().index == 2);

  auto selected_output = daffy::voice::SelectDevice(inventory, daffy::voice::AudioDeviceDirection::kOutput, "default");
  assert(selected_output.ok());
  assert(selected_output.value().index == 1);

  daffy::voice::VoiceRuntimeConfig config;
  config.preferred_input_device = "Studio Mic";
  config.preferred_output_device = "Desk Speakers";
  config.preferred_capture_sample_rate = 96000;
  config.preferred_playback_sample_rate = 96000;
  config.frames_per_buffer = 256;

  const auto session_plan = daffy::voice::BuildVoiceSessionPlan(
      inventory,
      config,
      [](daffy::voice::AudioDeviceDirection direction, const daffy::voice::PortAudioStreamRequest& request) {
        if (direction == daffy::voice::AudioDeviceDirection::kInput) {
          return request.sample_rate == 44100.0;
        }
        return request.sample_rate == 48000.0;
      });
  assert(session_plan.ok());
  assert(session_plan.value().input_device.index == 0);
  assert(session_plan.value().output_device.index == 1);
  assert(session_plan.value().input_stream.sample_rate == 44100.0);
  assert(session_plan.value().output_stream.sample_rate == 48000.0);
  assert(session_plan.value().capture_plan.needs_resample);
  assert(!session_plan.value().playback_plan.needs_resample);
  assert(session_plan.value().input_stream.frames_per_buffer == 256UL);
  assert(session_plan.value().output_stream.frames_per_buffer == 256UL);

  auto runtime = daffy::voice::PortAudioRuntime::Load();
  assert(runtime.ok());
  assert(runtime.value().IsLoaded());
  assert(!runtime.value().library_path().empty());

  return 0;
}
