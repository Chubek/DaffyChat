#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

#include "daffy/core/error.hpp"
#include "daffy/signaling/messages.hpp"
#include "daffy/voice/libdatachannel_peer.hpp"
#include "daffy/voice/media_worker.hpp"
#include "daffy/voice/portaudio_streams.hpp"
#include "daffy/util/json.hpp"

namespace daffy::voice {

struct VoicePeerSessionConfig {
  std::string room;
  std::string peer_id;
  std::string target_peer_id;
  VoiceRuntimeConfig runtime_config{};
  VoiceSessionPlan session_plan{};
  OpusCodecConfig opus_config{};
  LibDatachannelPeerConfig transport_config{};
};

struct VoicePeerSessionPumpStats {
  std::size_t capture_packets_sent{0};
  std::size_t playback_blocks_queued{0};
};

struct VoicePeerSessionTelemetry {
  VoiceMediaTelemetry media{};
  RtpTelemetryStats outbound_transport{};
  RtpTelemetryStats inbound_transport{};
  std::uint64_t signaling_messages_sent{0};
  std::uint64_t signaling_messages_received{0};
  std::uint64_t playback_blocks_buffered{0};
  std::uint64_t playback_blocks_dropped{0};
  std::uint64_t transport_resets{0};
  std::string last_transport_reset_reason;
  bool dtls_srtp_keying_ready{false};
  std::string dtls_role{"unknown"};
  std::string srtp_profile{"unknown"};
};

struct VoicePeerSessionStateSnapshot {
  std::string room;
  std::string peer_id;
  std::string target_peer_id;
  bool ready{false};
  std::uint64_t transport_resets{0};
  std::string last_transport_reset_reason;
  LibDatachannelStateSnapshot transport{};
};

util::json::Value VoicePeerSessionStateToJson(const VoicePeerSessionStateSnapshot& state);
util::json::Value VoicePeerSessionTelemetryToJson(const VoicePeerSessionTelemetry& telemetry);

class VoicePeerSession {
 public:
  using SignalingMessageCallback = std::function<void(const signaling::Message& message)>;
  using StateChangeCallback = std::function<void(const VoicePeerSessionStateSnapshot& state)>;

  struct Impl;

  VoicePeerSession();
  VoicePeerSession(VoicePeerSession&& other) noexcept;
  VoicePeerSession& operator=(VoicePeerSession&& other) noexcept;
  VoicePeerSession(const VoicePeerSession&) = delete;
  VoicePeerSession& operator=(const VoicePeerSession&) = delete;
  ~VoicePeerSession();

  static core::Result<VoicePeerSession> Create(const VoicePeerSessionConfig& config);

  core::Status StartNegotiation(std::string target_peer_id = {});
  core::Status HandleSignalingMessage(const signaling::Message& message);
  core::Status UpdateTransportConfig(const LibDatachannelPeerConfig& transport_config);

  core::Result<std::size_t> SendCapturedBlock(const DeviceAudioBlock& block);
  core::Result<std::size_t> PumpInboundAudio();
  core::Result<std::size_t> PumpCapture(PortAudioStreamSession& session);
  core::Result<std::size_t> PumpPlayback(PortAudioStreamSession& session);
  core::Result<VoicePeerSessionPumpStats> PumpMedia(PortAudioStreamSession& session);

  bool TryPopPlaybackBlock(DeviceAudioBlock& block);

  void SetSignalingMessageCallback(SignalingMessageCallback callback);
  void SetStateChangeCallback(StateChangeCallback callback);

  [[nodiscard]] const VoiceSessionPlan& plan() const;
  [[nodiscard]] bool IsReady() const;
  [[nodiscard]] VoicePeerSessionStateSnapshot state() const;
  [[nodiscard]] VoicePeerSessionTelemetry telemetry() const;

 private:
  explicit VoicePeerSession(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace daffy::voice
