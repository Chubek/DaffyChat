#include "daffy/voice/voice_peer_session.hpp"

#include <deque>
#include <mutex>
#include <utility>

namespace daffy::voice {
namespace {

constexpr std::size_t kBufferedPlaybackLimit = 64;

core::Error BuildSessionError(const std::string& message) {
  return core::Error{core::ErrorCode::kStateError, message};
}

bool MessageTargetsPeer(const signaling::Message& message, const std::string& peer_id) {
  return message.target_peer_id.empty() || message.target_peer_id == peer_id;
}

util::json::Value TransportStateToJson(const LibDatachannelStateSnapshot& state) {
  return util::json::Value::Object{{"connected", state.connected},
                                   {"local_track_open", state.local_track_open},
                                   {"remote_track_open", state.remote_track_open},
                                   {"dtls_srtp_keying_ready", state.dtls_srtp_keying_ready},
                                   {"connection_state", state.connection_state},
                                   {"ice_state", state.ice_state},
                                   {"gathering_state", state.gathering_state},
                                   {"dtls_role", state.dtls_role},
                                   {"srtp_profile", state.srtp_profile},
                                   {"dtls_srtp_key_block_bytes", static_cast<int>(state.dtls_srtp_key_block_bytes)}};
}

util::json::Value TransportStatsToJson(const RtpTelemetryStats& stats) {
  return util::json::Value::Object{{"packets_sent", static_cast<int>(stats.packets_sent)},
                                   {"packets_received", static_cast<int>(stats.packets_received)},
                                   {"packets_lost", static_cast<int>(stats.packets_lost)},
                                   {"bytes_sent", static_cast<int>(stats.bytes_sent)},
                                   {"bytes_received", static_cast<int>(stats.bytes_received)},
                                   {"out_of_order_packets", static_cast<int>(stats.out_of_order_packets)},
                                   {"duplicate_packets", static_cast<int>(stats.duplicate_packets)},
                                   {"rejected_packets", static_cast<int>(stats.rejected_packets)},
                                   {"srtp_protected_packets", static_cast<int>(stats.srtp_protected_packets)},
                                   {"srtp_unprotected_packets", static_cast<int>(stats.srtp_unprotected_packets)},
                                   {"jitter_ms", stats.jitter_ms},
                                   {"last_extended_sequence_received",
                                    static_cast<int>(stats.last_extended_sequence_received)}};
}

util::json::Value MediaTelemetryToJson(const VoiceMediaTelemetry& telemetry) {
  return util::json::Value::Object{
      {"codec",
       util::json::Value::Object{{"encoded_packets", static_cast<int>(telemetry.codec.encoded_packets)},
                                 {"decoded_packets", static_cast<int>(telemetry.codec.decoded_packets)},
                                 {"total_encode_microseconds",
                                  static_cast<int>(telemetry.codec.total_encode_microseconds)},
                                 {"total_decode_microseconds",
                                  static_cast<int>(telemetry.codec.total_decode_microseconds)}}},
      {"playout_underruns", static_cast<int>(telemetry.playout_underruns)}};
}

}  // namespace

util::json::Value VoicePeerSessionStateToJson(const VoicePeerSessionStateSnapshot& state) {
  return util::json::Value::Object{{"room", state.room},
                                   {"peer_id", state.peer_id},
                                   {"target_peer_id", state.target_peer_id},
                                   {"ready", state.ready},
                                   {"transport_resets", static_cast<int>(state.transport_resets)},
                                   {"last_transport_reset_reason", state.last_transport_reset_reason},
                                   {"transport", TransportStateToJson(state.transport)}};
}

util::json::Value VoicePeerSessionTelemetryToJson(const VoicePeerSessionTelemetry& telemetry) {
  return util::json::Value::Object{
      {"media", MediaTelemetryToJson(telemetry.media)},
      {"outbound_transport", TransportStatsToJson(telemetry.outbound_transport)},
      {"inbound_transport", TransportStatsToJson(telemetry.inbound_transport)},
      {"signaling_messages_sent", static_cast<int>(telemetry.signaling_messages_sent)},
      {"signaling_messages_received", static_cast<int>(telemetry.signaling_messages_received)},
      {"playback_blocks_buffered", static_cast<int>(telemetry.playback_blocks_buffered)},
      {"playback_blocks_dropped", static_cast<int>(telemetry.playback_blocks_dropped)},
      {"transport_resets", static_cast<int>(telemetry.transport_resets)},
      {"last_transport_reset_reason", telemetry.last_transport_reset_reason},
      {"dtls_srtp_keying_ready", telemetry.dtls_srtp_keying_ready},
      {"dtls_role", telemetry.dtls_role},
      {"srtp_profile", telemetry.srtp_profile}};
}

struct VoicePeerSession::Impl {
  explicit Impl(VoicePeerSessionConfig session_config, VoiceMediaWorker media, LibDatachannelAudioPeer transport_peer)
      : config(std::move(session_config)), worker(std::move(media)), peer(std::move(transport_peer)) {}

  core::Status BindTransportPeer(LibDatachannelAudioPeer transport_peer) {
    peer = std::move(transport_peer);
    transport_state = peer.state();

    peer.SetLocalDescriptionCallback([this](const std::string& type, const std::string& sdp) {
      const auto message_type = type == "offer" ? signaling::MessageType::kOffer : signaling::MessageType::kAnswer;
      EmitSignaling(message_type, sdp, {}, {});
    });
    peer.SetLocalCandidateCallback(
        [this](const std::string& candidate, const std::string& mid) { EmitSignaling(signaling::MessageType::kIceCandidate,
                                                                                      {},
                                                                                      candidate,
                                                                                      mid); });
    peer.SetStateChangeCallback([this](const LibDatachannelStateSnapshot& state) {
      {
        std::lock_guard<std::mutex> lock(mutex);
        transport_state = state;
      }
      EmitState();
    });
    EmitState();
    return core::OkStatus();
  }

  core::Status ResetTransport(std::string reason) {
    auto rebuilt = LibDatachannelAudioPeer::Create(config.transport_config);
    if (!rebuilt.ok()) {
      return rebuilt.error();
    }
    auto worker_reset = worker.Reset();
    if (!worker_reset.ok()) {
      return worker_reset;
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      buffered_playback.clear();
      transport_committed = false;
      transport_state = {};
      ++transport_resets;
      last_transport_reset_reason = std::move(reason);
    }

    auto bound = BindTransportPeer(std::move(rebuilt.value()));
    if (!bound.ok()) {
      return bound;
    }
    return core::OkStatus();
  }

  void SetTargetPeerId(std::string target) {
    std::lock_guard<std::mutex> lock(mutex);
    config.target_peer_id = std::move(target);
  }

  std::string target_peer_id() const {
    std::lock_guard<std::mutex> lock(mutex);
    return config.target_peer_id;
  }

  VoicePeerSessionStateSnapshot snapshot() const {
    std::lock_guard<std::mutex> lock(mutex);
    VoicePeerSessionStateSnapshot snapshot;
    snapshot.room = config.room;
    snapshot.peer_id = config.peer_id;
    snapshot.target_peer_id = config.target_peer_id;
    snapshot.transport_resets = transport_resets;
    snapshot.last_transport_reset_reason = last_transport_reset_reason;
    snapshot.transport = transport_state;
    snapshot.ready = transport_state.connected && transport_state.local_track_open && transport_state.remote_track_open;
    return snapshot;
  }

  void EmitState() {
    StateChangeCallback callback;
    VoicePeerSessionStateSnapshot state_snapshot;
    {
      std::lock_guard<std::mutex> lock(mutex);
      callback = state_callback;
      state_snapshot.room = config.room;
      state_snapshot.peer_id = config.peer_id;
      state_snapshot.target_peer_id = config.target_peer_id;
      state_snapshot.transport_resets = transport_resets;
      state_snapshot.last_transport_reset_reason = last_transport_reset_reason;
      state_snapshot.transport = transport_state;
      state_snapshot.ready =
          transport_state.connected && transport_state.local_track_open && transport_state.remote_track_open;
    }
    if (callback) {
      callback(state_snapshot);
    }
  }

  void EmitSignaling(signaling::MessageType type,
                     std::string sdp,
                     std::string candidate,
                     std::string mid) {
    SignalingMessageCallback callback;
    signaling::Message message;
    {
      std::lock_guard<std::mutex> lock(mutex);
      callback = signaling_callback;
      message.type = type;
      message.room = config.room;
      message.peer_id = config.peer_id;
      message.target_peer_id = config.target_peer_id;
      message.sdp = std::move(sdp);
      message.candidate = std::move(candidate);
      message.mid = std::move(mid);
      ++signaling_messages_sent;
    }
    if (callback) {
      callback(message);
    }
  }

  VoicePeerSessionConfig config;
  VoiceMediaWorker worker;
  LibDatachannelAudioPeer peer;
  mutable std::mutex mutex;
  std::deque<DeviceAudioBlock> buffered_playback{};
  SignalingMessageCallback signaling_callback;
  StateChangeCallback state_callback;
  LibDatachannelStateSnapshot transport_state{};
  std::uint64_t signaling_messages_sent{0};
  std::uint64_t signaling_messages_received{0};
  std::uint64_t playback_blocks_dropped{0};
  std::uint64_t transport_resets{0};
  std::string last_transport_reset_reason;
  bool transport_committed{false};
};

VoicePeerSession::VoicePeerSession() = default;

VoicePeerSession::VoicePeerSession(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

VoicePeerSession::VoicePeerSession(VoicePeerSession&& other) noexcept = default;

VoicePeerSession& VoicePeerSession::operator=(VoicePeerSession&& other) noexcept = default;

VoicePeerSession::~VoicePeerSession() = default;

core::Result<VoicePeerSession> VoicePeerSession::Create(const VoicePeerSessionConfig& config) {
  if (config.room.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Voice peer session requires a room id"};
  }
  if (config.peer_id.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Voice peer session requires a peer id"};
  }

  auto worker = VoiceMediaWorker::Create(config.runtime_config, config.session_plan, config.opus_config);
  if (!worker.ok()) {
    return worker.error();
  }

  auto peer = LibDatachannelAudioPeer::Create(config.transport_config);
  if (!peer.ok()) {
    return peer.error();
  }

  auto impl = std::make_unique<Impl>(config, std::move(worker.value()), std::move(peer.value()));
  auto bound = impl->BindTransportPeer(std::move(impl->peer));
  if (!bound.ok()) {
    return bound.error();
  }

  return VoicePeerSession(std::move(impl));
}

core::Status VoicePeerSession::StartNegotiation(std::string target_peer_id) {
  if (impl_ == nullptr) {
    return BuildSessionError("Voice peer session is not initialized");
  }
  if (!target_peer_id.empty()) {
    impl_->SetTargetPeerId(std::move(target_peer_id));
  }
  if (impl_->target_peer_id().empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Voice peer session requires a target peer before offering"};
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->transport_committed = true;
  }
  return impl_->peer.StartOffer();
}

core::Status VoicePeerSession::HandleSignalingMessage(const signaling::Message& message) {
  if (impl_ == nullptr) {
    return BuildSessionError("Voice peer session is not initialized");
  }
  if (!message.room.empty() && message.room != impl_->config.room) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Signaling message room does not match voice peer session"};
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    ++impl_->signaling_messages_received;
  }

  switch (message.type) {
    case signaling::MessageType::kJoinAck: {
      if (const auto* peers = message.data.Find("peers"); peers != nullptr && peers->IsArray() &&
                                                     peers->AsArray().size() == 1) {
        const auto& peer = peers->AsArray().front();
        if (const auto* peer_id = peer.Find("peer_id"); peer_id != nullptr && peer_id->IsString()) {
          impl_->SetTargetPeerId(peer_id->AsString());
          impl_->EmitState();
        }
      }
      return core::OkStatus();
    }
    case signaling::MessageType::kPeerReady:
      if (!message.peer_id.empty() && message.peer_id != impl_->config.peer_id) {
        impl_->SetTargetPeerId(message.peer_id);
        impl_->EmitState();
      }
      return core::OkStatus();
    case signaling::MessageType::kPeerLeft:
      if (message.peer_id == impl_->target_peer_id()) {
        auto reset = impl_->ResetTransport("peer-left:" + message.peer_id);
        if (!reset.ok()) {
          return reset;
        }
        impl_->SetTargetPeerId({});
        impl_->EmitState();
      }
      return core::OkStatus();
    case signaling::MessageType::kOffer:
      if (!MessageTargetsPeer(message, impl_->config.peer_id) || message.peer_id.empty() || message.sdp.empty()) {
        return core::Error{core::ErrorCode::kInvalidArgument, "Offer is missing routing or SDP data"};
      }
      {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->transport_committed = true;
      }
      impl_->SetTargetPeerId(message.peer_id);
      return impl_->peer.SetRemoteDescription("offer", message.sdp);
    case signaling::MessageType::kAnswer:
      if (!MessageTargetsPeer(message, impl_->config.peer_id) || message.peer_id.empty() || message.sdp.empty()) {
        return core::Error{core::ErrorCode::kInvalidArgument, "Answer is missing routing or SDP data"};
      }
      {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->transport_committed = true;
      }
      impl_->SetTargetPeerId(message.peer_id);
      return impl_->peer.SetRemoteDescription("answer", message.sdp);
    case signaling::MessageType::kIceCandidate:
      if (!MessageTargetsPeer(message, impl_->config.peer_id) || message.peer_id.empty() || message.candidate.empty() ||
          message.mid.empty()) {
        return core::Error{core::ErrorCode::kInvalidArgument, "ICE candidate is missing routing or candidate data"};
      }
      {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->transport_committed = true;
      }
      impl_->SetTargetPeerId(message.peer_id);
      return impl_->peer.AddRemoteCandidate(message.candidate, message.mid);
    case signaling::MessageType::kJoin:
    case signaling::MessageType::kLeave:
      return core::Error{core::ErrorCode::kInvalidArgument, "Client-originated signaling messages cannot be applied here"};
    case signaling::MessageType::kError:
      return core::Error{core::ErrorCode::kUnavailable,
                         message.error.empty() ? "Received signaling error" : message.error};
  }

  return core::OkStatus();
}

core::Status VoicePeerSession::UpdateTransportConfig(const LibDatachannelPeerConfig& transport_config) {
  if (impl_ == nullptr) {
    return BuildSessionError("Voice peer session is not initialized");
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->transport_committed) {
      return core::Error{core::ErrorCode::kStateError,
                         "Voice peer transport config cannot be updated after negotiation has started"};
    }
  }

  auto rebuilt = LibDatachannelAudioPeer::Create(transport_config);
  if (!rebuilt.ok()) {
    return rebuilt.error();
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->config.transport_config = transport_config;
  }
  return impl_->BindTransportPeer(std::move(rebuilt.value()));
}

core::Result<std::size_t> VoicePeerSession::SendCapturedBlock(const DeviceAudioBlock& block) {
  if (impl_ == nullptr) {
    return BuildSessionError("Voice peer session is not initialized");
  }
  auto packets = impl_->worker.ProcessCapturedBlock(block);
  if (!packets.ok()) {
    return packets.error();
  }

  std::size_t sent = 0;
  for (const auto& packet : packets.value()) {
    auto status = impl_->peer.SendAudioPacket(packet);
    if (!status.ok()) {
      return status.error();
    }
    ++sent;
  }
  return sent;
}

core::Result<std::size_t> VoicePeerSession::PumpInboundAudio() {
  if (impl_ == nullptr) {
    return BuildSessionError("Voice peer session is not initialized");
  }

  std::size_t buffered = 0;
  OpusPacket packet;
  while (impl_->peer.TryPopAudioPacket(packet)) {
    auto blocks = impl_->worker.ProcessReceivedPacket(packet);
    if (!blocks.ok()) {
      return blocks.error();
    }

    std::lock_guard<std::mutex> lock(impl_->mutex);
    for (auto& block : blocks.value()) {
      if (impl_->buffered_playback.size() >= kBufferedPlaybackLimit) {
        impl_->buffered_playback.pop_front();
        ++impl_->playback_blocks_dropped;
      }
      impl_->buffered_playback.push_back(std::move(block));
      ++buffered;
    }
  }
  return buffered;
}

core::Result<std::size_t> VoicePeerSession::PumpCapture(PortAudioStreamSession& session) {
  if (impl_ == nullptr) {
    return BuildSessionError("Voice peer session is not initialized");
  }
  return impl_->worker.PumpCapture(session, impl_->peer);
}

core::Result<std::size_t> VoicePeerSession::PumpPlayback(PortAudioStreamSession& session) {
  if (impl_ == nullptr) {
    return BuildSessionError("Voice peer session is not initialized");
  }
  return impl_->worker.PumpPlayback(impl_->peer, session);
}

core::Result<VoicePeerSessionPumpStats> VoicePeerSession::PumpMedia(PortAudioStreamSession& session) {
  if (impl_ == nullptr) {
    return BuildSessionError("Voice peer session is not initialized");
  }

  auto capture = impl_->worker.PumpCapture(session, impl_->peer);
  if (!capture.ok()) {
    return capture.error();
  }
  auto playback = impl_->worker.PumpPlayback(impl_->peer, session);
  if (!playback.ok()) {
    return playback.error();
  }

  VoicePeerSessionPumpStats stats;
  stats.capture_packets_sent = capture.value();
  stats.playback_blocks_queued = playback.value();
  return stats;
}

bool VoicePeerSession::TryPopPlaybackBlock(DeviceAudioBlock& block) {
  if (impl_ == nullptr) {
    return false;
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->buffered_playback.empty()) {
    return false;
  }
  block = std::move(impl_->buffered_playback.front());
  impl_->buffered_playback.pop_front();
  return true;
}

void VoicePeerSession::SetSignalingMessageCallback(SignalingMessageCallback callback) {
  if (impl_ == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->signaling_callback = std::move(callback);
}

void VoicePeerSession::SetStateChangeCallback(StateChangeCallback callback) {
  if (impl_ == nullptr) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state_callback = std::move(callback);
  }
  impl_->EmitState();
}

const VoiceSessionPlan& VoicePeerSession::plan() const {
  static const VoiceSessionPlan kEmptyPlan{};
  return impl_ == nullptr ? kEmptyPlan : impl_->config.session_plan;
}

bool VoicePeerSession::IsReady() const { return impl_ != nullptr && impl_->snapshot().ready; }

VoicePeerSessionStateSnapshot VoicePeerSession::state() const {
  return impl_ == nullptr ? VoicePeerSessionStateSnapshot{} : impl_->snapshot();
}

VoicePeerSessionTelemetry VoicePeerSession::telemetry() const {
  VoicePeerSessionTelemetry telemetry;
  if (impl_ == nullptr) {
    return telemetry;
  }

  std::lock_guard<std::mutex> lock(impl_->mutex);
  telemetry.media = impl_->worker.telemetry();
  telemetry.outbound_transport = impl_->peer.outbound_transport_stats();
  telemetry.inbound_transport = impl_->peer.inbound_transport_stats();
  telemetry.signaling_messages_sent = impl_->signaling_messages_sent;
  telemetry.signaling_messages_received = impl_->signaling_messages_received;
  telemetry.playback_blocks_buffered = impl_->buffered_playback.size();
  telemetry.playback_blocks_dropped = impl_->playback_blocks_dropped;
  telemetry.transport_resets = impl_->transport_resets;
  telemetry.last_transport_reset_reason = impl_->last_transport_reset_reason;
  telemetry.dtls_srtp_keying_ready = impl_->peer.state().dtls_srtp_keying_ready;
  telemetry.dtls_role = impl_->peer.state().dtls_role;
  telemetry.srtp_profile = impl_->peer.state().srtp_profile;
  return telemetry;
}

}  // namespace daffy::voice
