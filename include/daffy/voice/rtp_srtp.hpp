#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/voice/audio_pipeline.hpp"
#include "daffy/voice/opus_codec.hpp"

namespace daffy::voice {

struct RtpStreamConfig {
  std::optional<std::uint32_t> ssrc{1};
  std::uint8_t payload_type{111};
  std::uint32_t clock_rate{kPipelineSampleRate};
  std::uint16_t first_sequence{1};
  std::uint32_t first_timestamp{0};
  std::uint32_t timestamp_step{static_cast<std::uint32_t>(kPipelineFrameSamples)};
};

struct RtpTelemetryStats {
  std::uint64_t packets_sent{0};
  std::uint64_t bytes_sent{0};
  std::uint64_t packets_received{0};
  std::uint64_t bytes_received{0};
  std::uint64_t packets_lost{0};
  std::uint64_t out_of_order_packets{0};
  std::uint64_t duplicate_packets{0};
  std::uint64_t rejected_packets{0};
  std::uint64_t srtp_protected_packets{0};
  std::uint64_t srtp_unprotected_packets{0};
  double jitter_ms{0.0};
  std::uint64_t last_extended_sequence_received{0};
};

class RtpPacketizer {
 public:
  explicit RtpPacketizer(RtpStreamConfig config = {});

  core::Result<std::vector<std::uint8_t>> Packetize(const OpusPacket& packet);
  core::Result<OpusPacket> Parse(const std::vector<std::uint8_t>& packet);
  core::Result<OpusPacket> Parse(const std::uint8_t* packet, std::size_t size);

  [[nodiscard]] const RtpStreamConfig& config() const;
  [[nodiscard]] const RtpTelemetryStats& stats() const;

 private:
  std::uint64_t ExtendSequence(std::uint16_t sequence);
  void NoteReceive(std::uint64_t extended_sequence, std::uint32_t rtp_timestamp, std::size_t payload_size);

  RtpStreamConfig config_{};
  RtpTelemetryStats stats_{};
  std::uint16_t next_sequence_{1};
  std::uint32_t next_timestamp_{0};
  std::optional<std::uint64_t> highest_extended_sequence_{};
  std::optional<std::uint16_t> highest_sequence_low_{};
  std::optional<double> last_transit_seconds_{};
};

enum class SrtpProfile {
  kAes128CmSha1_80,
  kAes128CmSha1_32,
  kAeadAes128Gcm,
  kAeadAes256Gcm,
};

enum class DtlsRole {
  kClient,
  kServer,
};

struct SrtpKeyMaterial {
  SrtpProfile profile{SrtpProfile::kAes128CmSha1_80};
  std::vector<std::uint8_t> inbound_key;
  std::vector<std::uint8_t> outbound_key;
};

struct DtlsSrtpKeyBlock {
  SrtpProfile profile{SrtpProfile::kAes128CmSha1_80};
  DtlsRole local_role{DtlsRole::kClient};
  std::vector<std::uint8_t> key_block;
};

std::size_t SrtpKeyMaterialSize(SrtpProfile profile);
std::size_t DtlsSrtpKeyBlockSize(SrtpProfile profile);
core::Result<SrtpKeyMaterial> DeriveSrtpKeyMaterialFromDtls(const DtlsSrtpKeyBlock& key_block);

class SrtpSession {
 public:
  struct Impl;

  SrtpSession();
  SrtpSession(SrtpSession&& other) noexcept;
  SrtpSession& operator=(SrtpSession&& other) noexcept;
  SrtpSession(const SrtpSession&) = delete;
  SrtpSession& operator=(const SrtpSession&) = delete;
  ~SrtpSession();

  static core::Result<SrtpSession> Create(const SrtpKeyMaterial& key_material);
  static core::Result<SrtpSession> CreateFromDtlsKeyBlock(const DtlsSrtpKeyBlock& key_block);

  [[nodiscard]] bool IsReady() const;
  [[nodiscard]] const RtpTelemetryStats& stats() const;

  core::Result<std::vector<std::uint8_t>> ProtectRtp(const std::vector<std::uint8_t>& packet);
  core::Result<std::vector<std::uint8_t>> UnprotectRtp(const std::vector<std::uint8_t>& packet);

 private:
  explicit SrtpSession(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;
};

}  // namespace daffy::voice
