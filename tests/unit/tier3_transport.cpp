#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <thread>
#include <vector>

#include "daffy/voice/libdatachannel_peer.hpp"
#include "daffy/voice/rtp_srtp.hpp"

namespace {

daffy::voice::OpusPacket BuildPacket(const std::uint64_t sequence, const std::uint8_t seed) {
  daffy::voice::OpusPacket packet;
  packet.sequence = sequence;
  packet.payload.resize(120);
  for (std::size_t index = 0; index < packet.payload.size(); ++index) {
    packet.payload[index] = static_cast<std::uint8_t>(seed + static_cast<std::uint8_t>(index));
  }
  return packet;
}

bool WaitUntil(const std::function<bool()>& predicate, const int attempts = 200, const int sleep_ms = 20) {
  for (int attempt = 0; attempt < attempts; ++attempt) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  }
  return false;
}

}  // namespace

int main() {
  daffy::voice::RtpPacketizer sender({0x12345678U, 111, 48000, 77, 960, 480});
  daffy::voice::RtpPacketizer receiver({std::nullopt, 111, 48000, 1, 0, 480});

  auto raw = sender.Packetize(BuildPacket(9, 0x11));
  assert(raw.ok());
  auto roundtrip = receiver.Parse(raw.value());
  assert(roundtrip.ok());
  assert(roundtrip.value().sequence == 77);
  assert(roundtrip.value().rtp_timestamp == 960);
  assert(roundtrip.value().payload.size() == 120);
  assert(roundtrip.value().payload.front() == 0x11);

  daffy::voice::RtpPacketizer gap_receiver({std::nullopt, 111, 48000, 1, 0, 480});
  daffy::voice::OpusPacket second = BuildPacket(2, 0x21);
  auto seq1 = daffy::voice::RtpPacketizer({99U, 111, 48000, 1, 0, 480}).Packetize(BuildPacket(1, 0x20));
  assert(seq1.ok());
  daffy::voice::RtpPacketizer temp({99U, 111, 48000, 3, 960, 480});
  auto seq3 = temp.Packetize(second);
  assert(seq3.ok());
  assert(gap_receiver.Parse(seq1.value()).ok());
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  assert(gap_receiver.Parse(seq3.value()).ok());
  assert(gap_receiver.stats().packets_lost == 1);

  std::vector<std::uint8_t> outbound_key(30);
  std::vector<std::uint8_t> inbound_key(30);
  for (std::size_t index = 0; index < outbound_key.size(); ++index) {
    outbound_key[index] = static_cast<std::uint8_t>(index + 1);
    inbound_key[index] = static_cast<std::uint8_t>(index + 31);
  }

  daffy::voice::SrtpKeyMaterial sender_keys;
  sender_keys.profile = daffy::voice::SrtpProfile::kAes128CmSha1_80;
  sender_keys.inbound_key = inbound_key;
  sender_keys.outbound_key = outbound_key;
  auto sender_srtp = daffy::voice::SrtpSession::Create(sender_keys);
  assert(sender_srtp.ok());

  daffy::voice::SrtpKeyMaterial receiver_keys;
  receiver_keys.profile = daffy::voice::SrtpProfile::kAes128CmSha1_80;
  receiver_keys.inbound_key = outbound_key;
  receiver_keys.outbound_key = inbound_key;
  auto receiver_srtp = daffy::voice::SrtpSession::Create(receiver_keys);
  assert(receiver_srtp.ok());

  std::vector<std::uint8_t> dtls_key_block;
  dtls_key_block.reserve(60);
  dtls_key_block.insert(dtls_key_block.end(), outbound_key.begin(), outbound_key.begin() + 16);
  dtls_key_block.insert(dtls_key_block.end(), inbound_key.begin(), inbound_key.begin() + 16);
  dtls_key_block.insert(dtls_key_block.end(), outbound_key.begin() + 16, outbound_key.end());
  dtls_key_block.insert(dtls_key_block.end(), inbound_key.begin() + 16, inbound_key.end());

  daffy::voice::DtlsSrtpKeyBlock sender_dtls_keys;
  sender_dtls_keys.profile = daffy::voice::SrtpProfile::kAes128CmSha1_80;
  sender_dtls_keys.local_role = daffy::voice::DtlsRole::kClient;
  sender_dtls_keys.key_block = dtls_key_block;
  auto derived_sender_srtp = daffy::voice::SrtpSession::CreateFromDtlsKeyBlock(sender_dtls_keys);
  assert(derived_sender_srtp.ok());

  daffy::voice::DtlsSrtpKeyBlock receiver_dtls_keys = sender_dtls_keys;
  receiver_dtls_keys.local_role = daffy::voice::DtlsRole::kServer;
  auto derived_receiver_srtp = daffy::voice::SrtpSession::CreateFromDtlsKeyBlock(receiver_dtls_keys);
  assert(derived_receiver_srtp.ok());

  auto encrypted = sender_srtp.value().ProtectRtp(raw.value());
  assert(encrypted.ok());
  auto decrypted = receiver_srtp.value().UnprotectRtp(encrypted.value());
  assert(decrypted.ok());
  auto unprotected_packet = receiver.Parse(decrypted.value());
  assert(unprotected_packet.ok());
  assert(unprotected_packet.value().payload.front() == 0x11);
  assert(sender_srtp.value().stats().srtp_protected_packets == 1);
  assert(receiver_srtp.value().stats().srtp_unprotected_packets == 1);

  auto dtls_encrypted = derived_sender_srtp.value().ProtectRtp(raw.value());
  assert(dtls_encrypted.ok());
  auto dtls_decrypted = derived_receiver_srtp.value().UnprotectRtp(dtls_encrypted.value());
  assert(dtls_decrypted.ok());
  auto dtls_packet = receiver.Parse(dtls_decrypted.value());
  assert(dtls_packet.ok());
  assert(dtls_packet.value().payload.front() == 0x11);

  daffy::voice::LibDatachannelPeerConfig offerer_config;
  offerer_config.local_ssrc = 1001;
  daffy::voice::LibDatachannelPeerConfig answerer_config;
  answerer_config.local_ssrc = 2002;

  auto offerer = daffy::voice::LibDatachannelAudioPeer::Create(offerer_config);
  auto answerer = daffy::voice::LibDatachannelAudioPeer::Create(answerer_config);
  assert(offerer.ok());
  assert(answerer.ok());

  offerer.value().SetLocalDescriptionCallback([&answerer](const std::string& type, const std::string& sdp) {
    auto status = answerer.value().SetRemoteDescription(type, sdp);
    assert(status.ok());
  });
  answerer.value().SetLocalDescriptionCallback([&offerer](const std::string& type, const std::string& sdp) {
    auto status = offerer.value().SetRemoteDescription(type, sdp);
    assert(status.ok());
  });
  offerer.value().SetLocalCandidateCallback([&answerer](const std::string& candidate, const std::string& mid) {
    auto status = answerer.value().AddRemoteCandidate(candidate, mid);
    assert(status.ok());
  });
  answerer.value().SetLocalCandidateCallback([&offerer](const std::string& candidate, const std::string& mid) {
    auto status = offerer.value().AddRemoteCandidate(candidate, mid);
    assert(status.ok());
  });

  assert(offerer.value().StartOffer().ok());
  assert(WaitUntil([&offerer, &answerer]() {
    const auto offerer_state = offerer.value().state();
    const auto answerer_state = answerer.value().state();
    return offerer_state.connected && answerer_state.connected && offerer_state.local_track_open &&
           answerer_state.remote_track_open;
  }));
  assert(WaitUntil([&offerer, &answerer]() {
    return offerer.value().HasNegotiatedDtlsSrtpKeyBlock() && answerer.value().HasNegotiatedDtlsSrtpKeyBlock();
  }));

  auto offerer_live_srtp = offerer.value().CreateExplicitSrtpSession();
  auto answerer_live_srtp = answerer.value().CreateExplicitSrtpSession();
  assert(offerer_live_srtp.ok());
  assert(answerer_live_srtp.ok());

  auto live_rtp = sender.Packetize(BuildPacket(10, 0x51));
  assert(live_rtp.ok());
  auto live_encrypted = offerer_live_srtp.value().ProtectRtp(live_rtp.value());
  assert(live_encrypted.ok());
  auto live_decrypted = answerer_live_srtp.value().UnprotectRtp(live_encrypted.value());
  assert(live_decrypted.ok());
  auto live_roundtrip = receiver.Parse(live_decrypted.value());
  assert(live_roundtrip.ok());
  assert(live_roundtrip.value().payload.front() == 0x51);

  auto send_status = offerer.value().SendAudioPacket(BuildPacket(1, 0x42));
  assert(send_status.ok());

  daffy::voice::OpusPacket received_packet;
  assert(WaitUntil([&answerer, &received_packet]() { return answerer.value().TryPopAudioPacket(received_packet); }));
  assert(received_packet.payload.size() == 120);
  assert(received_packet.payload.front() == 0x42);
  assert(offerer.value().outbound_transport_stats().packets_sent >= 1);
  assert(answerer.value().inbound_transport_stats().packets_received >= 1);

  return 0;
}
