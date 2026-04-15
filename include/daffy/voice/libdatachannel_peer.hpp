#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/voice/media_worker.hpp"
#include "daffy/voice/rtp_srtp.hpp"

namespace daffy::voice {

struct IceServerConfig {
  std::string url;
  std::string username;
  std::string password;
};

struct LibDatachannelPeerConfig {
  std::string mid{"audio"};
  std::string cname{"daffy-audio"};
  std::string stream_id{"daffy-stream"};
  std::string track_id{"daffy-track"};
  std::vector<IceServerConfig> ice_servers{};
  bool relay_only{false};
  bool enable_ice_tcp{false};
  bool disable_auto_gathering{false};
  bool use_fake_transport{false};
  std::optional<std::size_t> mtu{};
  std::uint8_t payload_type{111};
  std::uint32_t local_ssrc{1};
};

struct LibDatachannelStateSnapshot {
  bool connected{false};
  bool local_track_open{false};
  bool remote_track_open{false};
  bool dtls_srtp_keying_ready{false};
  std::string connection_state{"new"};
  std::string ice_state{"new"};
  std::string gathering_state{"new"};
  std::string dtls_role{"unknown"};
  std::string srtp_profile{"unknown"};
  std::size_t dtls_srtp_key_block_bytes{0};
};

class LibDatachannelAudioPeer : public EncodedAudioSink, public EncodedAudioSource {
 public:
  using LocalDescriptionCallback = std::function<void(const std::string& type, const std::string& sdp)>;
  using LocalCandidateCallback = std::function<void(const std::string& candidate, const std::string& mid)>;
  using StateChangeCallback = std::function<void(const LibDatachannelStateSnapshot& state)>;

  struct Impl;

  LibDatachannelAudioPeer();
  LibDatachannelAudioPeer(LibDatachannelAudioPeer&& other) noexcept;
  LibDatachannelAudioPeer& operator=(LibDatachannelAudioPeer&& other) noexcept;
  LibDatachannelAudioPeer(const LibDatachannelAudioPeer&) = delete;
  LibDatachannelAudioPeer& operator=(const LibDatachannelAudioPeer&) = delete;
  ~LibDatachannelAudioPeer();

  static core::Result<LibDatachannelAudioPeer> Create(const LibDatachannelPeerConfig& config = {});

  core::Status StartOffer();
  core::Status SetRemoteDescription(const std::string& type, const std::string& sdp);
  core::Status AddRemoteCandidate(const std::string& candidate, const std::string& mid);

  void SetLocalDescriptionCallback(LocalDescriptionCallback callback);
  void SetLocalCandidateCallback(LocalCandidateCallback callback);
  void SetStateChangeCallback(StateChangeCallback callback);

  [[nodiscard]] bool IsReady() const;
  [[nodiscard]] LibDatachannelStateSnapshot state() const;
  [[nodiscard]] const RtpTelemetryStats& outbound_transport_stats() const;
  [[nodiscard]] const RtpTelemetryStats& inbound_transport_stats() const;
  [[nodiscard]] bool HasNegotiatedDtlsSrtpKeyBlock() const;
  core::Result<DtlsSrtpKeyBlock> NegotiatedDtlsSrtpKeyBlock() const;
  core::Result<SrtpSession> CreateExplicitSrtpSession() const;

  core::Status SendAudioPacket(const OpusPacket& packet) override;
  bool TryPopAudioPacket(OpusPacket& packet) override;

 private:
  explicit LibDatachannelAudioPeer(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace daffy::voice
