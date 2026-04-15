#include <cassert>
#include <chrono>
#include <cmath>
#include <functional>
#include <sstream>
#include <thread>
#include <unordered_map>

#include "daffy/config/app_config.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/signaling/server.hpp"
#include "daffy/voice/voice_peer_session.hpp"

namespace {

bool WaitUntil(const std::function<bool()>& predicate, const int attempts = 250, const int sleep_ms = 20) {
  for (int attempt = 0; attempt < attempts; ++attempt) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  }
  return false;
}

daffy::voice::DeviceAudioBlock BuildCaptureBlock(const daffy::voice::VoiceSessionPlan& plan,
                                                 const std::uint64_t sequence,
                                                 const float amplitude) {
  daffy::voice::DeviceAudioBlock block;
  block.sequence = sequence;
  block.format = plan.capture_plan.device_format;
  block.frame_count = static_cast<std::size_t>(plan.capture_plan.device_frames_per_buffer);
  for (std::size_t index = 0; index < block.frame_count; ++index) {
    const float sample = amplitude * std::sin(static_cast<float>(index) * 2.0F * 3.14159265F / 48.0F);
    for (int channel = 0; channel < block.format.channels; ++channel) {
      block.samples[index * static_cast<std::size_t>(block.format.channels) + static_cast<std::size_t>(channel)] = sample;
    }
  }
  return block;
}

struct TestSignalingHarness {
  daffy::signaling::SignalingServer server;
  std::unordered_map<std::string, daffy::voice::VoicePeerSession*> sessions_by_connection;

  explicit TestSignalingHarness(daffy::signaling::SignalingServer signaling_server) : server(std::move(signaling_server)) {}

  void Register(const std::string& connection_id, daffy::voice::VoicePeerSession& session) {
    sessions_by_connection[connection_id] = &session;
  }

  void Open(const std::string& connection_id) {
    daffy::signaling::ConnectionContext context;
    context.connection_id = connection_id;
    server.OpenConnection(context);
  }

  void Deliver(const std::string& source_connection_id, const daffy::signaling::Message& message) {
    const auto dispatch = server.HandleMessage(source_connection_id, daffy::signaling::SerializeMessage(message));
    assert(dispatch.accepted);
    for (const auto& envelope : dispatch.outgoing) {
      auto session_it = sessions_by_connection.find(envelope.connection_id);
      assert(session_it != sessions_by_connection.end());
      auto status = session_it->second->HandleSignalingMessage(envelope.message);
      assert(status.ok());
    }
  }

  void Join(const std::string& connection_id, const std::string& room, const std::string& peer_id) {
    daffy::signaling::Message join;
    join.type = daffy::signaling::MessageType::kJoin;
    join.room = room;
    join.peer_id = peer_id;
    Deliver(connection_id, join);
  }
};

}  // namespace

int main() {
  daffy::voice::AudioDeviceInventory inventory;
  inventory.devices = {
      {0, "Mic", "ALSA", 1, 0, 48000.0, 0.01, 0.0, true, false},
      {1, "Speaker", "ALSA", 0, 1, 48000.0, 0.0, 0.01, false, true},
  };

  daffy::voice::VoiceRuntimeConfig runtime_config;
  runtime_config.enable_noise_suppression = false;
  runtime_config.playout_buffer_frames = 1;
  runtime_config.max_playout_buffer_frames = 2;

  auto plan = daffy::voice::BuildVoiceSessionPlan(
      inventory,
      runtime_config,
      [](daffy::voice::AudioDeviceDirection, const daffy::voice::PortAudioStreamRequest&) { return true; });
  assert(plan.ok());

  daffy::voice::VoicePeerSessionConfig offerer_config;
  offerer_config.room = "alpha";
  offerer_config.peer_id = "peer-a";
  offerer_config.session_plan = plan.value();
  offerer_config.runtime_config = runtime_config;
  offerer_config.transport_config.use_fake_transport = true;
  offerer_config.transport_config.local_ssrc = 1001;

  daffy::voice::VoicePeerSessionConfig answerer_config = offerer_config;
  answerer_config.peer_id = "peer-b";
  answerer_config.transport_config.local_ssrc = 2002;

  auto offerer = daffy::voice::VoicePeerSession::Create(offerer_config);
  auto answerer = daffy::voice::VoicePeerSession::Create(answerer_config);
  assert(offerer.ok());
  assert(answerer.ok());

  std::ostringstream log_stream;
  auto logger = daffy::core::CreateOstreamLogger("tier3-peer-session-test", daffy::core::LogLevel::kInfo, log_stream);
  auto app_config = daffy::config::DefaultAppConfig();
  daffy::signaling::SignalingServer server(app_config, logger);
  TestSignalingHarness harness(std::move(server));
  harness.Open("conn-a");
  harness.Open("conn-b");
  harness.Register("conn-a", offerer.value());
  harness.Register("conn-b", answerer.value());

  offerer.value().SetSignalingMessageCallback([&harness](const daffy::signaling::Message& message) {
    harness.Deliver("conn-a", message);
  });
  answerer.value().SetSignalingMessageCallback([&harness](const daffy::signaling::Message& message) {
    harness.Deliver("conn-b", message);
  });

  harness.Join("conn-a", "alpha", "peer-a");
  harness.Join("conn-b", "alpha", "peer-b");

  assert(offerer.value().StartNegotiation("peer-b").ok());
  assert(WaitUntil([&offerer, &answerer]() { return offerer.value().IsReady() && answerer.value().IsReady(); }));

  auto sent = offerer.value().SendCapturedBlock(BuildCaptureBlock(offerer.value().plan(), 1, 0.35F));
  assert(sent.ok());
  assert(sent.value() >= 1);

  assert(WaitUntil([&answerer]() {
    auto pumped = answerer.value().PumpInboundAudio();
    return pumped.ok() && pumped.value() >= 1;
  }));

  daffy::voice::DeviceAudioBlock playback_block;
  assert(answerer.value().TryPopPlaybackBlock(playback_block));
  assert(playback_block.frame_count > 0);

  float energy = 0.0F;
  for (std::size_t index = 0;
       index < playback_block.frame_count * static_cast<std::size_t>(playback_block.format.channels);
       ++index) {
    energy += std::fabs(playback_block.samples[index]);
  }
  assert(energy > 1.0F);

  const auto offerer_state = offerer.value().state();
  const auto answerer_state = answerer.value().state();
  assert(offerer_state.target_peer_id == "peer-b");
  assert(answerer_state.target_peer_id == "peer-a");
  assert(offerer_state.ready);
  assert(answerer_state.ready);

  const auto offerer_telemetry = offerer.value().telemetry();
  const auto answerer_telemetry = answerer.value().telemetry();
  assert(offerer_telemetry.media.codec.encoded_packets >= 1);
  assert(answerer_telemetry.media.codec.decoded_packets >= 1);
  assert(offerer_telemetry.outbound_transport.packets_sent >= 1);
  assert(answerer_telemetry.inbound_transport.packets_received >= 1);
  assert(offerer_telemetry.signaling_messages_sent >= 1);
  assert(answerer_telemetry.signaling_messages_received >= 1);

  daffy::signaling::Message peer_left;
  peer_left.type = daffy::signaling::MessageType::kPeerLeft;
  peer_left.room = "alpha";
  peer_left.peer_id = "peer-b";
  assert(offerer.value().HandleSignalingMessage(peer_left).ok());
  assert(!offerer.value().IsReady());
  assert(offerer.value().state().target_peer_id.empty());
  assert(offerer.value().state().transport_resets == 1);
  assert(offerer.value().state().last_transport_reset_reason == "peer-left:peer-b");
  assert(offerer.value().telemetry().transport_resets == 1);
  assert(offerer.value().telemetry().last_transport_reset_reason == "peer-left:peer-b");
  assert(offerer.value().telemetry().playback_blocks_buffered == 0);

  auto updated_transport = offerer_config.transport_config;
  updated_transport.use_fake_transport = true;
  updated_transport.local_ssrc = 3003;
  assert(offerer.value().UpdateTransportConfig(updated_transport).ok());

  return 0;
}
