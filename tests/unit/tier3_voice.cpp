#include <cassert>
#include <cmath>
#include <vector>

#include "daffy/voice/audio_pipeline.hpp"

namespace {

bool NearlyEqual(float left, float right, float epsilon = 0.0001F) {
  return std::fabs(left - right) <= epsilon;
}

}  // namespace

int main() {
  const auto stream_plan = daffy::voice::BuildCaptureStreamPlan({44100, 2}, 256);
  assert(stream_plan.ok());
  assert(stream_plan.value().needs_resample);
  assert(stream_plan.value().needs_downmix);
  assert(stream_plan.value().pipeline_frame_samples == daffy::voice::kPipelineFrameSamples);

  daffy::voice::CaptureFrameAssembler assembler({44100, 2});
  std::vector<daffy::voice::AudioFrame> emitted_frames;
  for (int iteration = 0; iteration < 8 && emitted_frames.empty(); ++iteration) {
    std::vector<float> chunk(128 * 2, 0.25F);
    auto pushed = assembler.PushInterleaved(chunk);
    assert(pushed.ok());
    emitted_frames = std::move(pushed.value());
  }

  assert(!emitted_frames.empty());
  assert(emitted_frames.front().sequence == 1);
  assert(emitted_frames.front().format.sample_rate == daffy::voice::kPipelineSampleRate);
  float average_sample = 0.0F;
  for (float sample : emitted_frames.front().samples) {
    average_sample += sample;
  }
  average_sample /= static_cast<float>(emitted_frames.front().samples.size());
  assert(NearlyEqual(average_sample, 0.25F, 0.01F));
  assert(assembler.stats().input_frames >= 256);
  assert(assembler.stats().emitted_frames == 1);

  daffy::voice::AudioFrameRingBuffer<4> ring_buffer;
  daffy::voice::AudioFrame frame_one;
  frame_one.sequence = 10;
  daffy::voice::AudioFrame frame_two;
  frame_two.sequence = 11;
  daffy::voice::AudioFrame frame_three;
  frame_three.sequence = 12;
  daffy::voice::AudioFrame frame_four;
  frame_four.sequence = 13;

  assert(ring_buffer.EffectiveCapacity() == 3);
  assert(ring_buffer.TryPush(frame_one));
  assert(ring_buffer.TryPush(frame_two));
  assert(ring_buffer.TryPush(frame_three));
  assert(!ring_buffer.TryPush(frame_four));
  assert(ring_buffer.ApproxSize() == 3);

  daffy::voice::AudioFrame popped;
  assert(ring_buffer.TryPop(popped));
  assert(popped.sequence == 10);
  assert(ring_buffer.TryPush(frame_four));
  assert(ring_buffer.TryPop(popped));
  assert(popped.sequence == 11);
  assert(ring_buffer.TryPop(popped));
  assert(popped.sequence == 12);
  assert(ring_buffer.TryPop(popped));
  assert(popped.sequence == 13);
  assert(!ring_buffer.TryPop(popped));

  daffy::voice::PlayoutBuffer playout(2, 3);
  daffy::voice::AudioFrame playout_one;
  playout_one.sequence = 1;
  daffy::voice::AudioFrame playout_two;
  playout_two.sequence = 2;
  daffy::voice::AudioFrame playout_three;
  playout_three.sequence = 3;
  daffy::voice::AudioFrame playout_four;
  playout_four.sequence = 4;

  assert(!playout.PopReadyFrame().has_value());
  assert(playout.stats().underruns == 1);
  assert(playout.Push(playout_one));
  assert(!playout.PopReadyFrame().has_value());
  assert(playout.Push(playout_two));
  auto ready = playout.PopReadyFrame();
  assert(ready.has_value());
  assert(ready->sequence == 1);
  assert(playout.Push(playout_three));
  assert(playout.Push(playout_four));
  ready = playout.PopReadyFrame();
  assert(ready.has_value());
  assert(ready->sequence == 2);
  ready = playout.PopReadyFrame();
  assert(ready.has_value());
  assert(ready->sequence == 3);
  ready = playout.PopReadyFrame();
  assert(ready.has_value());
  assert(ready->sequence == 4);
  assert(!playout.PopReadyFrame().has_value());
  assert(playout.stats().pushed_frames == 4);
  assert(playout.stats().popped_frames == 4);
  assert(playout.stats().high_watermark_frames >= 2);

  return 0;
}
