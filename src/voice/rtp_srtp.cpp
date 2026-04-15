#include "daffy/voice/rtp_srtp.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <utility>

#include <srtp2/srtp.h>

namespace daffy::voice {
namespace {

constexpr std::size_t kRtpHeaderSize = 12;

std::uint16_t ReadUint16(const std::uint8_t* data) {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(data[0]) << 8U) |
                                    static_cast<std::uint16_t>(data[1]));
}

std::uint32_t ReadUint32(const std::uint8_t* data) {
  return (static_cast<std::uint32_t>(data[0]) << 24U) | (static_cast<std::uint32_t>(data[1]) << 16U) |
         (static_cast<std::uint32_t>(data[2]) << 8U) | static_cast<std::uint32_t>(data[3]);
}

void WriteUint16(std::uint8_t* data, const std::uint16_t value) {
  data[0] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
  data[1] = static_cast<std::uint8_t>(value & 0xFFU);
}

void WriteUint32(std::uint8_t* data, const std::uint32_t value) {
  data[0] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
  data[1] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
  data[2] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
  data[3] = static_cast<std::uint8_t>(value & 0xFFU);
}

double NowSeconds() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::duration<double>>(now).count();
}

core::Error BuildRtpError(const std::string& message) {
  return core::Error{core::ErrorCode::kInvalidArgument, message};
}

core::Error BuildSrtpError(const std::string& message) {
  return core::Error{core::ErrorCode::kUnavailable, message};
}

srtp_profile_t ToSrtpProfile(const SrtpProfile profile) {
  switch (profile) {
    case SrtpProfile::kAes128CmSha1_80:
      return srtp_profile_aes128_cm_sha1_80;
    case SrtpProfile::kAes128CmSha1_32:
      return srtp_profile_aes128_cm_sha1_32;
    case SrtpProfile::kAeadAes128Gcm:
      return srtp_profile_aead_aes_128_gcm;
    case SrtpProfile::kAeadAes256Gcm:
      return srtp_profile_aead_aes_256_gcm;
  }
  return srtp_profile_aes128_cm_sha1_80;
}

std::size_t RequiredSrtpKeyMaterialSize(const SrtpProfile profile) {
  const auto srtp_profile = ToSrtpProfile(profile);
  return static_cast<std::size_t>(srtp_profile_get_master_key_length(srtp_profile) +
                                  srtp_profile_get_master_salt_length(srtp_profile));
}

struct SrtpRuntimeGuard {
  static std::mutex mutex;
  static std::uint64_t ref_count;

  static core::Status Acquire() {
    std::lock_guard<std::mutex> lock(mutex);
    if (ref_count == 0) {
      const auto status = srtp_init();
      if (status != srtp_err_status_ok) {
        return BuildSrtpError("libsrtp initialization failed");
      }
    }
    ++ref_count;
    return core::OkStatus();
  }

  static void Release() {
    std::lock_guard<std::mutex> lock(mutex);
    if (ref_count == 0) {
      return;
    }
    --ref_count;
    // libdatachannel manages its own DTLS-SRTP sessions in the same process, so tearing down the
    // global libsrtp runtime here can invalidate transport-owned sessions during shutdown.
  }
};

std::mutex SrtpRuntimeGuard::mutex;
std::uint64_t SrtpRuntimeGuard::ref_count = 0;

}  // namespace

RtpPacketizer::RtpPacketizer(RtpStreamConfig config)
    : config_(std::move(config)), next_sequence_(config_.first_sequence), next_timestamp_(config_.first_timestamp) {
  if (!config_.ssrc.has_value()) {
    config_.ssrc = 1;
  }
  if (config_.clock_rate == 0) {
    config_.clock_rate = kPipelineSampleRate;
  }
  if (config_.timestamp_step == 0) {
    config_.timestamp_step = static_cast<std::uint32_t>(kPipelineFrameSamples);
  }
}

core::Result<std::vector<std::uint8_t>> RtpPacketizer::Packetize(const OpusPacket& packet) {
  if (packet.payload.empty()) {
    return BuildRtpError("Opus payload cannot be empty");
  }

  std::vector<std::uint8_t> bytes(kRtpHeaderSize + packet.payload.size(), 0);
  bytes[0] = 0x80U;
  bytes[1] = static_cast<std::uint8_t>(config_.payload_type & 0x7FU);
  WriteUint16(bytes.data() + 2, next_sequence_);

  const auto timestamp = packet.rtp_timestamp != 0 ? packet.rtp_timestamp : next_timestamp_;
  WriteUint32(bytes.data() + 4, timestamp);
  WriteUint32(bytes.data() + 8, config_.ssrc.value_or(1));
  std::copy(packet.payload.begin(), packet.payload.end(), bytes.begin() + static_cast<std::ptrdiff_t>(kRtpHeaderSize));

  ++stats_.packets_sent;
  stats_.bytes_sent += bytes.size();
  ++next_sequence_;
  next_timestamp_ = timestamp + config_.timestamp_step;
  return bytes;
}

core::Result<OpusPacket> RtpPacketizer::Parse(const std::vector<std::uint8_t>& packet) {
  return Parse(packet.data(), packet.size());
}

core::Result<OpusPacket> RtpPacketizer::Parse(const std::uint8_t* packet, const std::size_t size) {
  if (packet == nullptr || size < kRtpHeaderSize) {
    ++stats_.rejected_packets;
    return BuildRtpError("RTP packet is too small");
  }

  const auto version = static_cast<std::uint8_t>((packet[0] >> 6U) & 0x03U);
  if (version != 2U) {
    ++stats_.rejected_packets;
    return BuildRtpError("Unsupported RTP version");
  }

  const bool padding = (packet[0] & 0x20U) != 0;
  const bool extension = (packet[0] & 0x10U) != 0;
  const std::size_t csrc_count = static_cast<std::size_t>(packet[0] & 0x0FU);
  std::size_t header_size = kRtpHeaderSize + csrc_count * 4U;
  if (size < header_size) {
    ++stats_.rejected_packets;
    return BuildRtpError("RTP packet truncated before CSRC list");
  }

  if (extension) {
    if (size < header_size + 4U) {
      ++stats_.rejected_packets;
      return BuildRtpError("RTP packet truncated before extension header");
    }
    const std::size_t extension_words = ReadUint16(packet + header_size + 2U);
    header_size += 4U + extension_words * 4U;
    if (size < header_size) {
      ++stats_.rejected_packets;
      return BuildRtpError("RTP extension body exceeds packet size");
    }
  }

  std::size_t payload_size = size - header_size;
  if (padding) {
    const auto padding_size = static_cast<std::size_t>(packet[size - 1]);
    if (padding_size == 0 || padding_size > payload_size) {
      ++stats_.rejected_packets;
      return BuildRtpError("RTP padding exceeds packet payload");
    }
    payload_size -= padding_size;
  }

  const auto payload_type = static_cast<std::uint8_t>(packet[1] & 0x7FU);
  if (payload_type != config_.payload_type) {
    ++stats_.rejected_packets;
    return BuildRtpError("Unexpected RTP payload type");
  }

  const auto sequence = ReadUint16(packet + 2U);
  const auto timestamp = ReadUint32(packet + 4U);
  const auto ssrc = ReadUint32(packet + 8U);
  if (!config_.ssrc.has_value()) {
    config_.ssrc = ssrc;
  }

  OpusPacket decoded;
  decoded.sequence = ExtendSequence(sequence);
  decoded.rtp_timestamp = timestamp;
  decoded.payload.assign(packet + static_cast<std::ptrdiff_t>(header_size),
                         packet + static_cast<std::ptrdiff_t>(header_size + payload_size));

  NoteReceive(decoded.sequence, timestamp, decoded.payload.size());
  return decoded;
}

const RtpStreamConfig& RtpPacketizer::config() const { return config_; }

const RtpTelemetryStats& RtpPacketizer::stats() const { return stats_; }

std::uint64_t RtpPacketizer::ExtendSequence(const std::uint16_t sequence) {
  if (!highest_extended_sequence_.has_value()) {
    highest_extended_sequence_ = sequence;
    highest_sequence_low_ = sequence;
    return *highest_extended_sequence_;
  }

  const auto highest_low = highest_sequence_low_.value_or(sequence);
  auto delta = static_cast<int>(sequence) - static_cast<int>(highest_low);
  if (delta < -32768) {
    delta += 65536;
  } else if (delta > 32768) {
    delta -= 65536;
  }

  return static_cast<std::uint64_t>(static_cast<std::int64_t>(*highest_extended_sequence_) + delta);
}

void RtpPacketizer::NoteReceive(const std::uint64_t extended_sequence,
                                const std::uint32_t rtp_timestamp,
                                const std::size_t payload_size) {
  ++stats_.packets_received;
  stats_.bytes_received += payload_size + kRtpHeaderSize;

  if (!highest_extended_sequence_.has_value()) {
    highest_extended_sequence_ = extended_sequence;
    highest_sequence_low_ = static_cast<std::uint16_t>(extended_sequence & 0xFFFFU);
  } else if (extended_sequence > *highest_extended_sequence_) {
    const auto delta = extended_sequence - *highest_extended_sequence_;
    if (delta > 1) {
      stats_.packets_lost += delta - 1;
    }
    highest_extended_sequence_ = extended_sequence;
    highest_sequence_low_ = static_cast<std::uint16_t>(extended_sequence & 0xFFFFU);
  } else if (extended_sequence == *highest_extended_sequence_) {
    ++stats_.duplicate_packets;
  } else {
    ++stats_.out_of_order_packets;
  }

  stats_.last_extended_sequence_received = std::max(stats_.last_extended_sequence_received, extended_sequence);

  const auto arrival_seconds = NowSeconds();
  const auto transit_seconds = arrival_seconds - (static_cast<double>(rtp_timestamp) / static_cast<double>(config_.clock_rate));
  if (last_transit_seconds_.has_value()) {
    const auto difference = std::fabs(transit_seconds - *last_transit_seconds_);
    const auto current_jitter_seconds = stats_.jitter_ms / 1000.0;
    stats_.jitter_ms = (current_jitter_seconds + (difference - current_jitter_seconds) / 16.0) * 1000.0;
  }
  last_transit_seconds_ = transit_seconds;
}

struct SrtpSession::Impl {
  srtp_t inbound{nullptr};
  srtp_t outbound{nullptr};
  RtpTelemetryStats stats{};
  bool runtime_acquired{false};
};

SrtpSession::SrtpSession() = default;

SrtpSession::SrtpSession(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

SrtpSession::SrtpSession(SrtpSession&& other) noexcept = default;

SrtpSession& SrtpSession::operator=(SrtpSession&& other) noexcept = default;

SrtpSession::~SrtpSession() {
  if (impl_ != nullptr) {
    if (impl_->inbound != nullptr) {
      srtp_dealloc(impl_->inbound);
      impl_->inbound = nullptr;
    }
    if (impl_->outbound != nullptr) {
      srtp_dealloc(impl_->outbound);
      impl_->outbound = nullptr;
    }
    if (impl_->runtime_acquired) {
      SrtpRuntimeGuard::Release();
      impl_->runtime_acquired = false;
    }
  }
}

std::size_t SrtpKeyMaterialSize(const SrtpProfile profile) { return RequiredSrtpKeyMaterialSize(profile); }

std::size_t DtlsSrtpKeyBlockSize(const SrtpProfile profile) { return RequiredSrtpKeyMaterialSize(profile) * 2U; }

core::Result<SrtpKeyMaterial> DeriveSrtpKeyMaterialFromDtls(const DtlsSrtpKeyBlock& key_block) {
  const auto profile = ToSrtpProfile(key_block.profile);
  const auto key_length = static_cast<std::size_t>(srtp_profile_get_master_key_length(profile));
  const auto salt_length = static_cast<std::size_t>(srtp_profile_get_master_salt_length(profile));
  const auto key_material_size = key_length + salt_length;
  const auto required_size = DtlsSrtpKeyBlockSize(key_block.profile);
  if (key_material_size == 0U || key_block.key_block.size() != required_size) {
    return BuildSrtpError("DTLS-SRTP key block length does not match the selected profile");
  }

  const auto client_key_begin = key_block.key_block.begin();
  const auto server_key_begin = client_key_begin + static_cast<std::ptrdiff_t>(key_length);
  const auto client_salt_begin = server_key_begin + static_cast<std::ptrdiff_t>(key_length);
  const auto server_salt_begin = client_salt_begin + static_cast<std::ptrdiff_t>(salt_length);

  std::vector<std::uint8_t> client_material;
  client_material.reserve(key_material_size);
  client_material.insert(client_material.end(), client_key_begin, client_key_begin + static_cast<std::ptrdiff_t>(key_length));
  client_material.insert(client_material.end(),
                         client_salt_begin,
                         client_salt_begin + static_cast<std::ptrdiff_t>(salt_length));

  std::vector<std::uint8_t> server_material;
  server_material.reserve(key_material_size);
  server_material.insert(server_material.end(), server_key_begin, server_key_begin + static_cast<std::ptrdiff_t>(key_length));
  server_material.insert(server_material.end(),
                         server_salt_begin,
                         server_salt_begin + static_cast<std::ptrdiff_t>(salt_length));

  SrtpKeyMaterial material;
  material.profile = key_block.profile;
  if (key_block.local_role == DtlsRole::kClient) {
    material.outbound_key = std::move(client_material);
    material.inbound_key = std::move(server_material);
  } else {
    material.outbound_key = std::move(server_material);
    material.inbound_key = std::move(client_material);
  }
  return material;
}

core::Result<SrtpSession> SrtpSession::Create(const SrtpKeyMaterial& key_material) {
  const auto acquire = SrtpRuntimeGuard::Acquire();
  if (!acquire.ok()) {
    return acquire.error();
  }

  auto impl = std::make_unique<Impl>();
  impl->runtime_acquired = true;

  const auto profile = ToSrtpProfile(key_material.profile);
  const auto required_key_size = SrtpKeyMaterialSize(key_material.profile);
  if (key_material.inbound_key.size() != required_key_size || key_material.outbound_key.size() != required_key_size) {
    SrtpRuntimeGuard::Release();
    return BuildSrtpError("SRTP key material length does not match the selected profile");
  }

  if (srtp_create(&impl->inbound, nullptr) != srtp_err_status_ok ||
      srtp_create(&impl->outbound, nullptr) != srtp_err_status_ok) {
    if (impl->inbound != nullptr) {
      srtp_dealloc(impl->inbound);
    }
    if (impl->outbound != nullptr) {
      srtp_dealloc(impl->outbound);
    }
    SrtpRuntimeGuard::Release();
    return BuildSrtpError("Unable to allocate SRTP session");
  }

  srtp_policy_t inbound_policy{};
  inbound_policy.ssrc.type = ssrc_any_inbound;
  inbound_policy.ssrc.value = 0;
  inbound_policy.key = const_cast<unsigned char*>(key_material.inbound_key.data());
  inbound_policy.window_size = 128;
  inbound_policy.allow_repeat_tx = 1;
  inbound_policy.next = nullptr;

  srtp_policy_t outbound_policy{};
  outbound_policy.ssrc.type = ssrc_any_outbound;
  outbound_policy.ssrc.value = 0;
  outbound_policy.key = const_cast<unsigned char*>(key_material.outbound_key.data());
  outbound_policy.window_size = 128;
  outbound_policy.allow_repeat_tx = 1;
  outbound_policy.next = nullptr;

  if (srtp_crypto_policy_set_from_profile_for_rtp(&inbound_policy.rtp, profile) != srtp_err_status_ok ||
      srtp_crypto_policy_set_from_profile_for_rtcp(&inbound_policy.rtcp, profile) != srtp_err_status_ok ||
      srtp_crypto_policy_set_from_profile_for_rtp(&outbound_policy.rtp, profile) != srtp_err_status_ok ||
      srtp_crypto_policy_set_from_profile_for_rtcp(&outbound_policy.rtcp, profile) != srtp_err_status_ok) {
    SrtpRuntimeGuard::Release();
    return BuildSrtpError("Unable to map SRTP crypto profile");
  }

  if (srtp_add_stream(impl->inbound, &inbound_policy) != srtp_err_status_ok ||
      srtp_add_stream(impl->outbound, &outbound_policy) != srtp_err_status_ok) {
    SrtpRuntimeGuard::Release();
    return BuildSrtpError("Unable to configure SRTP stream policy");
  }

  return SrtpSession(std::move(impl));
}

core::Result<SrtpSession> SrtpSession::CreateFromDtlsKeyBlock(const DtlsSrtpKeyBlock& key_block) {
  auto material = DeriveSrtpKeyMaterialFromDtls(key_block);
  if (!material.ok()) {
    return material.error();
  }
  return Create(material.value());
}

bool SrtpSession::IsReady() const { return impl_ != nullptr && impl_->inbound != nullptr && impl_->outbound != nullptr; }

const RtpTelemetryStats& SrtpSession::stats() const {
  static const RtpTelemetryStats kEmpty;
  return impl_ == nullptr ? kEmpty : impl_->stats;
}

core::Result<std::vector<std::uint8_t>> SrtpSession::ProtectRtp(const std::vector<std::uint8_t>& packet) {
  if (!IsReady()) {
    return BuildSrtpError("SRTP session is not ready");
  }

  std::vector<std::uint8_t> encrypted(packet.size() + SRTP_MAX_TRAILER_LEN, 0);
  std::copy(packet.begin(), packet.end(), encrypted.begin());
  int size = static_cast<int>(packet.size());
  const auto status = srtp_protect(impl_->outbound, encrypted.data(), &size);
  if (status != srtp_err_status_ok) {
    return BuildSrtpError("SRTP protect failed");
  }

  encrypted.resize(static_cast<std::size_t>(size));
  ++impl_->stats.srtp_protected_packets;
  return encrypted;
}

core::Result<std::vector<std::uint8_t>> SrtpSession::UnprotectRtp(const std::vector<std::uint8_t>& packet) {
  if (!IsReady()) {
    return BuildSrtpError("SRTP session is not ready");
  }

  std::vector<std::uint8_t> decrypted(packet);
  int size = static_cast<int>(decrypted.size());
  const auto status = srtp_unprotect(impl_->inbound, decrypted.data(), &size);
  if (status != srtp_err_status_ok) {
    return BuildSrtpError("SRTP unprotect failed");
  }

  decrypted.resize(static_cast<std::size_t>(size));
  ++impl_->stats.srtp_unprotected_packets;
  return decrypted;
}

}  // namespace daffy::voice
