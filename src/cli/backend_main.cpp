#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "daffy/config/app_config.hpp"
#include "daffy/core/build_info.hpp"
#include "daffy/core/logger.hpp"
#include "daffy/rooms/room_registry.hpp"
#include "daffy/runtime/event_bus.hpp"
#include "daffy/util/json.hpp"
#include "daffy/voice/native_voice_client.hpp"
#include "daffy/voice/portaudio_runtime.hpp"
#include "daffy/voice/voice_loopback_harness.hpp"
#include "daffy/web/voice_diagnostics_http_server.hpp"

namespace {

std::atomic<bool> g_voice_peer_stop_requested{false};

void HandleVoicePeerSignal(const int) { g_voice_peer_stop_requested.store(true); }

struct ScopedSignalHandlers {
  using Handler = void (*)(int);

  ScopedSignalHandlers() {
    previous_sigint = std::signal(SIGINT, HandleVoicePeerSignal);
    previous_sigterm = std::signal(SIGTERM, HandleVoicePeerSignal);
  }

  ~ScopedSignalHandlers() {
    std::signal(SIGINT, previous_sigint);
    std::signal(SIGTERM, previous_sigterm);
  }

  Handler previous_sigint{SIG_DFL};
  Handler previous_sigterm{SIG_DFL};
};

daffy::voice::VoiceRuntimeConfig ToVoiceRuntimeConfig(const daffy::config::VoiceConfig& config) {
  daffy::voice::VoiceRuntimeConfig runtime_config;
  runtime_config.preferred_input_device = config.preferred_input_device;
  runtime_config.preferred_output_device = config.preferred_output_device;
  runtime_config.preferred_capture_sample_rate = config.preferred_capture_sample_rate;
  runtime_config.preferred_playback_sample_rate = config.preferred_playback_sample_rate;
  runtime_config.preferred_channels = config.preferred_channels;
  runtime_config.frames_per_buffer = config.frames_per_buffer;
  runtime_config.playout_buffer_frames = config.playout_buffer_frames;
  runtime_config.max_playout_buffer_frames = config.max_playout_buffer_frames;
  runtime_config.enable_noise_suppression = config.enable_noise_suppression;
  runtime_config.enable_metrics = config.enable_metrics;
  return runtime_config;
}

std::vector<daffy::voice::IceServerConfig> ToIceServers(const daffy::config::AppConfig& config) {
  std::vector<daffy::voice::IceServerConfig> servers;
  servers.reserve(config.signaling.stun_servers.size() + 1);
  for (const auto& stun_server : config.signaling.stun_servers) {
    servers.push_back({stun_server, {}, {}});
  }
  if (!config.turn.uri.empty()) {
    servers.push_back({config.turn.uri, config.turn.username, config.turn.password});
  }
  return servers;
}

void PrintDevices(const daffy::voice::AudioDeviceInventory& inventory) {
  std::cout << "portaudio: " << inventory.version_text << " (" << inventory.library_path << ")\n";
  for (const auto& device : inventory.devices) {
    std::cout << "device[" << device.index << "]: " << device.name << " host_api=" << device.host_api
              << " in=" << device.max_input_channels << " out=" << device.max_output_channels
              << " default_rate=" << device.default_sample_rate
              << " default_input=" << (device.is_default_input ? "true" : "false")
              << " default_output=" << (device.is_default_output ? "true" : "false") << '\n';
  }
}

void PrintVoicePeerSnapshot(const daffy::voice::NativeVoiceClientStateSnapshot& state,
                            const daffy::voice::NativeVoiceClientTelemetry& telemetry) {
  const auto average_encode_ms =
      telemetry.live.peer.media.codec.encoded_packets == 0
          ? 0.0
          : static_cast<double>(telemetry.live.peer.media.codec.total_encode_microseconds) /
                static_cast<double>(telemetry.live.peer.media.codec.encoded_packets) / 1000.0;
  const auto average_decode_ms =
      telemetry.live.peer.media.codec.decoded_packets == 0
          ? 0.0
          : static_cast<double>(telemetry.live.peer.media.codec.total_decode_microseconds) /
                static_cast<double>(telemetry.live.peer.media.codec.decoded_packets) / 1000.0;
  std::cout << "voice peer telemetry: joined=" << (state.signaling.joined ? "true" : "false")
            << " ready=" << (state.live.peer.ready ? "true" : "false")
            << " signaling_out=" << telemetry.signaling.outbound_messages
            << " signaling_in=" << telemetry.signaling.inbound_messages
            << " turn_ice=" << state.signaling.discovered_ice_servers
            << " has_turn=" << (state.signaling.has_turn_credentials ? "true" : "false")
            << " turn_fetch_retries=" << telemetry.signaling.turn_fetch_retry_attempts
            << " turn_refreshes=" << telemetry.signaling.turn_refreshes
            << " turn_fetch_error="
            << (state.signaling.last_turn_fetch_error.empty() ? "<none>" : state.signaling.last_turn_fetch_error)
            << " nego_resets=" << telemetry.negotiation_resets
            << " nego_reoffers=" << telemetry.negotiation_reoffers
            << " last_reset="
            << (telemetry.last_negotiation_reset_reason.empty() ? "<none>" : telemetry.last_negotiation_reset_reason)
            << " last_reoffer="
            << (telemetry.last_reoffer_reason.empty() ? "<none>" : telemetry.last_reoffer_reason)
            << " transport_resets=" << telemetry.live.peer.transport_resets
            << " transport_reset_reason="
            << (telemetry.live.peer.last_transport_reset_reason.empty() ? "<none>"
                                                                       : telemetry.live.peer.last_transport_reset_reason)
            << " srtp_keys=" << (telemetry.live.peer.dtls_srtp_keying_ready ? "true" : "false")
            << " srtp_role=" << telemetry.live.peer.dtls_role
            << " srtp_profile=" << telemetry.live.peer.srtp_profile
            << " encoded=" << telemetry.live.peer.media.codec.encoded_packets
            << " decoded=" << telemetry.live.peer.media.codec.decoded_packets
            << " packets_sent=" << telemetry.live.peer.outbound_transport.packets_sent
            << " packets_received=" << telemetry.live.peer.inbound_transport.packets_received
            << " jitter_ms=" << telemetry.live.peer.inbound_transport.jitter_ms
            << " codec_avg_ms=" << average_encode_ms << '/' << average_decode_ms
            << " underruns=" << telemetry.live.stream.playback_underruns << '\n';
}

int RunVoiceDevices(const daffy::config::AppConfig& config) {
  auto runtime = daffy::voice::PortAudioRuntime::Load();
  if (!runtime.ok()) {
    std::cerr << "failed to load PortAudio: " << runtime.error().ToString() << '\n';
    return 1;
  }

  auto inventory = runtime.value().EnumerateDevices();
  if (!inventory.ok()) {
    std::cerr << "failed to enumerate audio devices: " << inventory.error().ToString() << '\n';
    return 1;
  }

  auto plan = runtime.value().BuildSessionPlan(ToVoiceRuntimeConfig(config.voice));
  std::cout << daffy::core::BuildSummary() << '\n';
  if (plan.ok()) {
    std::cout << "voice session plan: input=" << plan.value().input_device.name << "@"
              << plan.value().input_stream.sample_rate << "Hz output=" << plan.value().output_device.name << "@"
              << plan.value().output_stream.sample_rate << "Hz\n";
  } else {
    std::cout << "voice session plan: unavailable (" << plan.error().ToString() << ")\n";
  }
  PrintDevices(inventory.value());
  return 0;
}

int RunVoiceSmoke(const daffy::config::AppConfig& config, const int duration_ms) {
  daffy::voice::VoiceLoopbackHarnessConfig harness_config;
  harness_config.live_config.peer_config.room = "local-audio";
  harness_config.live_config.peer_config.peer_id = "peer-local";
  harness_config.live_config.peer_config.runtime_config = ToVoiceRuntimeConfig(config.voice);
  harness_config.live_config.peer_config.transport_config.ice_servers = ToIceServers(config);

  auto harness = daffy::voice::VoiceLoopbackHarness::Create(harness_config);
  if (!harness.ok()) {
    std::cerr << "failed to create voice loopback harness: " << harness.error().ToString() << '\n';
    return 1;
  }

  auto started = harness.value().Start();
  if (!started.ok()) {
    std::cerr << "failed to start voice loopback harness: " << started.error().ToString() << '\n';
    return 1;
  }

  std::cout << daffy::core::BuildSummary() << '\n';
  std::cout << "voice smoke: running local capture -> transport -> echo -> playback loopback for " << duration_ms
            << "ms\n";
  PrintDevices(harness.value().devices());
  std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));

  auto stopped = harness.value().Stop();
  if (!stopped.ok()) {
    std::cerr << "failed to stop voice loopback harness: " << stopped.error().ToString() << '\n';
    return 1;
  }

  const auto state = harness.value().state();
  const auto telemetry = harness.value().telemetry();
  std::cout << "voice smoke state: running=" << (state.running ? "true" : "false")
            << " negotiation_started=" << (state.negotiation_started ? "true" : "false")
            << " last_error=" << (state.last_error.empty() ? "<none>" : state.last_error) << '\n';
  std::cout << "voice smoke telemetry: capture_callbacks=" << telemetry.live.stream.capture_callbacks
            << " playback_callbacks=" << telemetry.live.stream.playback_callbacks
            << " loopback_echoed=" << telemetry.echo_blocks_returned
            << " local_encoded=" << telemetry.live.peer.media.codec.encoded_packets
            << " local_decoded=" << telemetry.live.peer.media.codec.decoded_packets
            << " remote_decoded=" << telemetry.echo.media.codec.decoded_packets
            << " pump_iterations=" << telemetry.pump_iterations << '\n';
  return 0;
}

int RunVoicePeer(const daffy::config::AppConfig& config,
                 const int duration_ms,
                 const int telemetry_interval_ms,
                 const std::string& websocket_url,
                 const std::string& room,
                 const std::string& peer_id,
                 const std::string& target_peer_id) {
  daffy::voice::NativeVoiceClientConfig client_config;
  client_config.live_config.peer_config.room = room;
  client_config.live_config.peer_config.peer_id = peer_id;
  client_config.live_config.peer_config.target_peer_id = target_peer_id;
  client_config.live_config.peer_config.runtime_config = ToVoiceRuntimeConfig(config.voice);
  client_config.live_config.peer_config.transport_config.ice_servers = ToIceServers(config);
  client_config.signaling_config.websocket_url = websocket_url;
  client_config.signaling_config.room = room;
  client_config.signaling_config.peer_id = peer_id;
  client_config.signaling_config.ping_interval_ms = config.signaling.ping_interval_ms;

  auto client = daffy::voice::NativeVoiceClient::Create(client_config);
  if (!client.ok()) {
    std::cerr << "failed to create native voice client: " << client.error().ToString() << '\n';
    return 1;
  }

  auto started = client.value().Start();
  if (!started.ok()) {
    std::cerr << "failed to start native voice client: " << started.error().ToString() << '\n';
    return 1;
  }

  auto logger = daffy::core::CreateConsoleLogger("daffy-backend-voice-admin",
                                                 daffy::core::ParseLogLevel(config.server.log_level));
  std::unique_ptr<daffy::web::VoiceDiagnosticsHttpServer> diagnostics_server;
  if (config.frontend_bridge.enabled) {
    diagnostics_server = std::make_unique<daffy::web::VoiceDiagnosticsHttpServer>(
        config,
        logger,
        [&client]() {
          return daffy::web::BuildNativeVoiceDiagnosticsPayload(client.value().state(), client.value().telemetry());
        });
    const auto admin_started = diagnostics_server->Start();
    if (!admin_started.ok()) {
      std::cerr << "failed to start voice diagnostics HTTP server: " << admin_started.error().ToString() << '\n';
    } else {
      std::cout << "voice_admin_url: http://" << config.server.bind_address << ':' << admin_started.value()
                << config.frontend_bridge.bridge_endpoint << '\n';
    }
  }

  std::cout << daffy::core::BuildSummary() << '\n';
  std::cout << "voice peer: " << peer_id << " joining room " << room << " via " << websocket_url;
  if (!target_peer_id.empty()) {
    std::cout << " targeting " << target_peer_id;
  }
  std::cout << " telemetry=" << telemetry_interval_ms << "ms";
  if (duration_ms > 0) {
    std::cout << " duration=" << duration_ms << "ms";
  } else {
    std::cout << " duration=until-signal";
  }
  std::cout << '\n';
  PrintDevices(client.value().devices());
  g_voice_peer_stop_requested.store(false);
  ScopedSignalHandlers signal_handlers;
  const auto started_at = std::chrono::steady_clock::now();
  while (!g_voice_peer_stop_requested.load()) {
    const auto state = client.value().state();
    const auto telemetry = client.value().telemetry();
    PrintVoicePeerSnapshot(state, telemetry);

    if (duration_ms > 0) {
      const auto elapsed_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started_at).count();
      if (elapsed_ms >= duration_ms) {
        break;
      }
      const auto remaining_ms = duration_ms - static_cast<int>(elapsed_ms);
      std::this_thread::sleep_for(std::chrono::milliseconds(std::min(telemetry_interval_ms, remaining_ms)));
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(telemetry_interval_ms));
    }
  }

  auto stopped = client.value().Stop();
  if (!stopped.ok()) {
    std::cerr << "failed to stop native voice client: " << stopped.error().ToString() << '\n';
    return 1;
  }

  const auto state = client.value().state();
  const auto telemetry = client.value().telemetry();
  std::cout << "voice peer state: signaling_open=" << (state.signaling.websocket_open ? "true" : "false")
            << " joined=" << (state.signaling.joined ? "true" : "false")
            << " transport_ready=" << (state.live.peer.ready ? "true" : "false")
            << " last_error=" << (state.last_error.empty() ? "<none>" : state.last_error) << '\n';
  PrintVoicePeerSnapshot(state, telemetry);
  return 0;
}

int RunBackend(const std::string& config_path) {
  auto config_result = daffy::config::LoadAppConfigFromFile(config_path);
  if (!config_result.ok()) {
    std::cerr << "failed to load config: " << config_result.error().ToString() << '\n';
    return 1;
  }

  const auto& config = config_result.value();
  auto logger = daffy::core::CreateConsoleLogger("daffy-backend",
                                                 daffy::core::ParseLogLevel(config.server.log_level));
  daffy::runtime::InMemoryEventBus event_bus;
  std::vector<daffy::runtime::EventEnvelope> observed_events;
  event_bus.Subscribe("room.lifecycle", [&](const daffy::runtime::EventEnvelope& event) {
    observed_events.push_back(event);
  });

  daffy::rooms::RoomRegistry room_registry(logger, event_bus);

  auto room = room_registry.CreateRoom("bootstrap-room");
  if (!room.ok()) {
    std::cerr << "failed to create room: " << room.error().ToString() << '\n';
    return 1;
  }

  auto participant = room_registry.AddParticipant(room.value().id, "bootstrap-admin", daffy::rooms::ParticipantRole::kAdmin);
  if (!participant.ok()) {
    std::cerr << "failed to add participant: " << participant.error().ToString() << '\n';
    return 1;
  }

  auto session = room_registry.AttachSession(room.value().id, participant.value().id, "peer-bootstrap");
  if (!session.ok()) {
    std::cerr << "failed to attach session: " << session.error().ToString() << '\n';
    return 1;
  }

  std::cout << daffy::core::BuildSummary() << '\n';
  std::cout << "backend config: " << daffy::config::DescribeAppConfig(config) << '\n';
  std::cout << "rooms: " << room_registry.List().size() << ", emitted events: " << observed_events.size() << '\n';
  if (!observed_events.empty()) {
    std::cout << "last event: " << daffy::util::json::Serialize(daffy::runtime::EventEnvelopeToJson(observed_events.back()))
              << '\n';
  }
  std::cout << "status: Tier 1 backend foundation ready" << '\n';
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  std::string_view mode;
  std::string config_path = daffy::config::DefaultConfigPath();
  int voice_smoke_duration_ms = 2000;
  int voice_peer_telemetry_interval_ms = 1000;
  bool duration_overridden = false;
  std::string signaling_url;
  std::string room = "alpha";
  std::string peer_id = "peer-a";
  std::string target_peer_id;

  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "--version") {
      std::cout << daffy::core::ProjectVersion() << '\n';
      return 0;
    }
    if (argument == "--voice-devices" || argument == "--voice-smoke" || argument == "--voice-peer") {
      mode = argument;
      continue;
    }
    if (argument.rfind("--duration-ms=", 0) == 0) {
      voice_smoke_duration_ms = std::stoi(std::string(argument.substr(std::string_view("--duration-ms=").size())));
      duration_overridden = true;
      continue;
    }
    if (argument.rfind("--telemetry-interval-ms=", 0) == 0) {
      voice_peer_telemetry_interval_ms =
          std::stoi(std::string(argument.substr(std::string_view("--telemetry-interval-ms=").size())));
      continue;
    }
    if (argument.rfind("--signaling-url=", 0) == 0) {
      signaling_url = std::string(argument.substr(std::string_view("--signaling-url=").size()));
      continue;
    }
    if (argument.rfind("--room=", 0) == 0) {
      room = std::string(argument.substr(std::string_view("--room=").size()));
      continue;
    }
    if (argument.rfind("--peer-id=", 0) == 0) {
      peer_id = std::string(argument.substr(std::string_view("--peer-id=").size()));
      continue;
    }
    if (argument.rfind("--target-peer=", 0) == 0) {
      target_peer_id = std::string(argument.substr(std::string_view("--target-peer=").size()));
      continue;
    }
    config_path = argv[index];
  }

  auto config_result = daffy::config::LoadAppConfigFromFile(config_path);
  if (!config_result.ok()) {
    std::cerr << "failed to load config: " << config_result.error().ToString() << '\n';
    return 1;
  }

  if (mode == "--voice-devices") {
    return RunVoiceDevices(config_result.value());
  }
  if (mode == "--voice-smoke") {
    return RunVoiceSmoke(config_result.value(), voice_smoke_duration_ms);
  }
  if (mode == "--voice-peer") {
    if (signaling_url.empty()) {
      signaling_url = "ws://" + config_result.value().signaling.bind_address + ':' +
                      std::to_string(config_result.value().signaling.port) + '/';
    }
    if (!duration_overridden) {
      voice_smoke_duration_ms = 0;
    }
    return RunVoicePeer(config_result.value(),
                        voice_smoke_duration_ms,
                        voice_peer_telemetry_interval_ms,
                        signaling_url,
                        room,
                        peer_id,
                        target_peer_id);
  }

  return RunBackend(config_path);
}
