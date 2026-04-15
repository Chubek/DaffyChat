#include "daffy/voice/libdatachannel_peer.hpp"

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#define protected public
#include <rtc/peerconnection.hpp>
#undef protected

#ifndef RTC_SYSTEM_SRTP
#define RTC_SYSTEM_SRTP 1
#define DAFFY_DEFINED_RTC_SYSTEM_SRTP 1
#endif

#define private public
#include "../../third_party/libdatachannel/src/impl/dtlssrtptransport.hpp"
#undef private

#include "../../third_party/libdatachannel/src/impl/peerconnection.hpp"

#ifdef DAFFY_DEFINED_RTC_SYSTEM_SRTP
#undef RTC_SYSTEM_SRTP
#undef DAFFY_DEFINED_RTC_SYSTEM_SRTP
#endif

namespace daffy::voice {
namespace {

std::string ToString(const rtc::PeerConnection::State state) {
  std::ostringstream stream;
  stream << state;
  return stream.str();
}

std::string ToString(const rtc::PeerConnection::IceState state) {
  std::ostringstream stream;
  stream << state;
  return stream.str();
}

std::string ToString(const rtc::PeerConnection::GatheringState state) {
  std::ostringstream stream;
  stream << state;
  return stream.str();
}

std::string ToString(const DtlsRole role) {
  return role == DtlsRole::kClient ? "client" : "server";
}

std::string ToString(const SrtpProfile profile) {
  switch (profile) {
    case SrtpProfile::kAes128CmSha1_80:
      return "SRTP_AES128_CM_SHA1_80";
    case SrtpProfile::kAes128CmSha1_32:
      return "SRTP_AES128_CM_SHA1_32";
    case SrtpProfile::kAeadAes128Gcm:
      return "SRTP_AEAD_AES_128_GCM";
    case SrtpProfile::kAeadAes256Gcm:
      return "SRTP_AEAD_AES_256_GCM";
  }
  return "unknown";
}

core::Error BuildTransportError(const std::string& message) {
  return core::Error{core::ErrorCode::kUnavailable, message};
}

struct FakeTransportEndpoint {
  LibDatachannelAudioPeer::Impl* owner{nullptr};
  std::weak_ptr<FakeTransportEndpoint> remote;
};

std::mutex& FakeRegistryMutex() {
  static std::mutex mutex;
  return mutex;
}

std::unordered_map<std::uint32_t, std::weak_ptr<FakeTransportEndpoint>>& FakeRegistry() {
  static std::unordered_map<std::uint32_t, std::weak_ptr<FakeTransportEndpoint>> registry;
  return registry;
}

void RegisterFakeEndpoint(const std::uint32_t id, const std::shared_ptr<FakeTransportEndpoint>& endpoint) {
  std::lock_guard<std::mutex> lock(FakeRegistryMutex());
  FakeRegistry()[id] = endpoint;
}

void UnregisterFakeEndpoint(const std::uint32_t id) {
  std::lock_guard<std::mutex> lock(FakeRegistryMutex());
  FakeRegistry().erase(id);
}

std::shared_ptr<FakeTransportEndpoint> FindFakeEndpoint(const std::uint32_t id) {
  std::lock_guard<std::mutex> lock(FakeRegistryMutex());
  const auto it = FakeRegistry().find(id);
  return it == FakeRegistry().end() ? nullptr : it->second.lock();
}

std::optional<std::uint32_t> ParseFakeDescriptionId(const std::string& sdp) {
  constexpr std::string_view kOfferPrefix = "fake-offer:";
  constexpr std::string_view kAnswerPrefix = "fake-answer:";
  std::string_view payload{sdp};
  if (payload.rfind(kOfferPrefix, 0) == 0) {
    payload.remove_prefix(kOfferPrefix.size());
  } else if (payload.rfind(kAnswerPrefix, 0) == 0) {
    payload.remove_prefix(kAnswerPrefix.size());
  } else {
    return std::nullopt;
  }

  try {
    return static_cast<std::uint32_t>(std::stoul(std::string(payload)));
  } catch (...) {
    return std::nullopt;
  }
}

class InspectablePeerConnection {
 public:
  explicit InspectablePeerConnection(rtc::Configuration config)
      : impl_(std::make_shared<rtc::impl::PeerConnection>(std::move(config))) {}

  ~InspectablePeerConnection() {
    try {
      if (impl_ != nullptr) {
        impl_->remoteClose();
      }
    } catch (...) {
    }
  }

  std::shared_ptr<rtc::Track> addTrack(rtc::Description::Media description) {
    return std::make_shared<rtc::Track>(impl_->emplaceTrack(std::move(description)));
  }

  void onTrack(std::function<void(std::shared_ptr<rtc::Track>)> callback) {
    impl_->trackCallback = std::move(callback);
    impl_->flushPendingTracks();
  }

  void onLocalDescription(std::function<void(rtc::Description)> callback) {
    impl_->localDescriptionCallback = std::move(callback);
  }

  void onLocalCandidate(std::function<void(rtc::Candidate)> callback) {
    impl_->localCandidateCallback = std::move(callback);
  }

  void onStateChange(std::function<void(rtc::PeerConnection::State)> callback) {
    impl_->stateChangeCallback = std::move(callback);
  }

  void onIceStateChange(std::function<void(rtc::PeerConnection::IceState)> callback) {
    impl_->iceStateChangeCallback = std::move(callback);
  }

  void onGatheringStateChange(std::function<void(rtc::PeerConnection::GatheringState)> callback) {
    impl_->gatheringStateChangeCallback = std::move(callback);
  }

  void resetCallbacks() { impl_->resetCallbacks(); }

  void setLocalDescription(const rtc::Description::Type type) {
    std::unique_lock signaling_lock(impl_->signalingMutex);
    auto signaling_state = impl_->signalingState.load();

    rtc::Description::Type resolved_type = type;
    if (resolved_type == rtc::Description::Type::Unspec) {
      resolved_type = signaling_state == rtc::PeerConnection::SignalingState::HaveRemoteOffer
                          ? rtc::Description::Type::Answer
                          : rtc::Description::Type::Offer;
    }

    rtc::PeerConnection::SignalingState new_signaling_state;
    switch (signaling_state) {
      case rtc::PeerConnection::SignalingState::Stable:
        if (resolved_type != rtc::Description::Type::Offer) {
          throw std::logic_error("Unexpected local description type in stable signaling state");
        }
        new_signaling_state = rtc::PeerConnection::SignalingState::HaveLocalOffer;
        break;
      case rtc::PeerConnection::SignalingState::HaveRemoteOffer:
      case rtc::PeerConnection::SignalingState::HaveLocalPranswer:
        if (resolved_type != rtc::Description::Type::Answer &&
            resolved_type != rtc::Description::Type::Pranswer) {
          throw std::logic_error("Unexpected local description type while answering");
        }
        new_signaling_state = rtc::PeerConnection::SignalingState::Stable;
        break;
      default:
        return;
    }

    auto ice_transport = impl_->initIceTransport();
    if (!ice_transport) {
      return;
    }

    rtc::Description local = ice_transport->getLocalDescription(resolved_type);
    impl_->populateLocalDescription(local);
    if (local.mediaCount() == 0) {
      throw std::runtime_error("No track to negotiate");
    }

    impl_->processLocalDescription(std::move(local));
    impl_->changeSignalingState(new_signaling_state);
    signaling_lock.unlock();

    if (!impl_->config.disableAutoNegotiation &&
        new_signaling_state == rtc::PeerConnection::SignalingState::Stable &&
        impl_->negotiationNeeded()) {
      setLocalDescription(rtc::Description::Type::Offer);
    }

    if (impl_->gatheringState == rtc::PeerConnection::GatheringState::New &&
        !impl_->config.disableAutoGathering) {
      ice_transport->gatherLocalCandidates(impl_->localBundleMid());
    }
  }

  void setRemoteDescription(rtc::Description description) {
    std::unique_lock signaling_lock(impl_->signalingMutex);
    impl_->validateRemoteDescription(description);

    auto signaling_state = impl_->signalingState.load();
    rtc::PeerConnection::SignalingState new_signaling_state;
    switch (signaling_state) {
      case rtc::PeerConnection::SignalingState::Stable:
        description.hintType(rtc::Description::Type::Offer);
        if (description.type() != rtc::Description::Type::Offer) {
          throw std::logic_error("Unexpected remote description type in stable signaling state");
        }
        new_signaling_state = rtc::PeerConnection::SignalingState::HaveRemoteOffer;
        break;
      case rtc::PeerConnection::SignalingState::HaveLocalOffer:
        description.hintType(rtc::Description::Type::Answer);
        if (description.type() == rtc::Description::Type::Offer) {
          impl_->rollbackLocalDescription();
          impl_->changeSignalingState(rtc::PeerConnection::SignalingState::Stable);
          new_signaling_state = rtc::PeerConnection::SignalingState::HaveRemoteOffer;
          break;
        }
        if (description.type() != rtc::Description::Type::Answer &&
            description.type() != rtc::Description::Type::Pranswer) {
          throw std::logic_error("Unexpected remote answer type");
        }
        new_signaling_state = rtc::PeerConnection::SignalingState::Stable;
        break;
      case rtc::PeerConnection::SignalingState::HaveRemotePranswer:
        description.hintType(rtc::Description::Type::Answer);
        if (description.type() != rtc::Description::Type::Answer &&
            description.type() != rtc::Description::Type::Pranswer) {
          throw std::logic_error("Unexpected remote pranswer type");
        }
        new_signaling_state = rtc::PeerConnection::SignalingState::Stable;
        break;
      default:
        throw std::logic_error("Unexpected remote description in current signaling state");
    }

    auto remote_candidates = description.extractCandidates();
    auto ice_transport = impl_->initIceTransport();
    if (!ice_transport) {
      return;
    }

    ice_transport->setRemoteDescription(description);
    impl_->processRemoteDescription(std::move(description));
    impl_->changeSignalingState(new_signaling_state);
    signaling_lock.unlock();

    for (const auto& candidate : remote_candidates) {
      addRemoteCandidate(candidate);
    }

    if (!impl_->config.disableAutoNegotiation) {
      if (new_signaling_state == rtc::PeerConnection::SignalingState::Stable && impl_->negotiationNeeded()) {
        setLocalDescription(rtc::Description::Type::Offer);
      } else if (new_signaling_state == rtc::PeerConnection::SignalingState::HaveRemoteOffer) {
        setLocalDescription(rtc::Description::Type::Answer);
      }
    }
  }

  void addRemoteCandidate(rtc::Candidate candidate) {
    std::unique_lock signaling_lock(impl_->signalingMutex);
    impl_->processRemoteCandidate(std::move(candidate));
  }

  [[nodiscard]] std::shared_ptr<rtc::impl::PeerConnection> impl_handle() const { return impl_; }

 private:
  std::shared_ptr<rtc::impl::PeerConnection> impl_;
};

struct SrtpProfileParameters {
  SrtpProfile profile{SrtpProfile::kAes128CmSha1_80};
  std::size_t key_size{0};
  std::size_t salt_size{0};
};

std::optional<SrtpProfileParameters> ResolveSrtpProfileParameters(const std::size_t material_size) {
  switch (material_size) {
    case 30:
      return SrtpProfileParameters{SrtpProfile::kAes128CmSha1_80, 16, 14};
    case 28:
      return SrtpProfileParameters{SrtpProfile::kAeadAes128Gcm, 16, 12};
    case 44:
      return SrtpProfileParameters{SrtpProfile::kAeadAes256Gcm, 32, 12};
    default:
      return std::nullopt;
  }
}

core::Result<DtlsSrtpKeyBlock> ExportDtlsSrtpKeyBlock(const InspectablePeerConnection& peer) {
  auto peer_impl = peer.impl_handle();
  if (peer_impl == nullptr) {
    return BuildTransportError("libdatachannel peer implementation is not available");
  }

  auto transport = std::dynamic_pointer_cast<rtc::impl::DtlsSrtpTransport>(peer_impl->getDtlsTransport());
  if (transport == nullptr) {
    return BuildTransportError("libdatachannel DTLS-SRTP transport is not ready");
  }
  if (transport->mClientSessionKey.empty() || transport->mServerSessionKey.empty()) {
    return BuildTransportError("libdatachannel has not exported DTLS-SRTP keys yet");
  }
  if (transport->mClientSessionKey.size() != transport->mServerSessionKey.size()) {
    return BuildTransportError("libdatachannel exported mismatched DTLS-SRTP key sizes");
  }

  const auto parameters = ResolveSrtpProfileParameters(transport->mClientSessionKey.size());
  if (!parameters.has_value()) {
    return BuildTransportError("libdatachannel exported an unsupported DTLS-SRTP profile");
  }

  DtlsSrtpKeyBlock key_block;
  key_block.profile = parameters->profile;
  key_block.local_role = transport->isClient() ? DtlsRole::kClient : DtlsRole::kServer;
  key_block.key_block.reserve(DtlsSrtpKeyBlockSize(key_block.profile));
  key_block.key_block.insert(key_block.key_block.end(),
                             transport->mClientSessionKey.begin(),
                             transport->mClientSessionKey.begin() + static_cast<std::ptrdiff_t>(parameters->key_size));
  key_block.key_block.insert(key_block.key_block.end(),
                             transport->mServerSessionKey.begin(),
                             transport->mServerSessionKey.begin() + static_cast<std::ptrdiff_t>(parameters->key_size));
  key_block.key_block.insert(key_block.key_block.end(),
                             transport->mClientSessionKey.begin() + static_cast<std::ptrdiff_t>(parameters->key_size),
                             transport->mClientSessionKey.end());
  key_block.key_block.insert(key_block.key_block.end(),
                             transport->mServerSessionKey.begin() + static_cast<std::ptrdiff_t>(parameters->key_size),
                             transport->mServerSessionKey.end());
  return key_block;
}

}  // namespace

struct LibDatachannelAudioPeer::Impl {
  explicit Impl(LibDatachannelPeerConfig peer_config)
      : config(std::move(peer_config)),
        outbound_rtp(RtpStreamConfig{config.local_ssrc,
                                     config.payload_type,
                                     static_cast<std::uint32_t>(kPipelineSampleRate),
                                     1,
                                     0,
                                     static_cast<std::uint32_t>(kPipelineFrameSamples)}),
        inbound_rtp(RtpStreamConfig{std::nullopt,
                                    config.payload_type,
                                    static_cast<std::uint32_t>(kPipelineSampleRate),
                                    1,
                                    0,
                                    static_cast<std::uint32_t>(kPipelineFrameSamples)}) {
    if (config.use_fake_transport) {
      state_snapshot.local_track_open = true;
      state_snapshot.connection_state = "new";
      state_snapshot.ice_state = "new";
      state_snapshot.gathering_state = "complete";
      fake_endpoint = std::make_shared<FakeTransportEndpoint>();
      fake_endpoint->owner = this;
      RegisterFakeEndpoint(config.local_ssrc, fake_endpoint);
    }
  }

  ~Impl() {
    if (config.use_fake_transport) {
      UnregisterFakeEndpoint(config.local_ssrc);
      if (fake_endpoint != nullptr) {
        fake_endpoint->owner = nullptr;
      }
    }
    if (local_track != nullptr) {
      local_track->resetCallbacks();
    }
    if (remote_track != nullptr) {
      remote_track->resetCallbacks();
    }
    local_track.reset();
    remote_track.reset();
    if (peer != nullptr) {
      peer->resetCallbacks();
      peer.reset();
    }
  }

  void AttachIncomingTrack(const std::shared_ptr<rtc::Track>& track, const bool update_remote_state) {
    if (track == nullptr) {
      if (update_remote_state) {
        {
          std::lock_guard<std::mutex> lock(callback_mutex);
          state_snapshot.remote_track_open = false;
        }
        EmitState();
      }
      return;
    }

    if (update_remote_state) {
      remote_track = track;
      {
        std::lock_guard<std::mutex> lock(callback_mutex);
        state_snapshot.remote_track_open = track->isOpen();
      }
    }

    track->onOpen([this, update_remote_state]() {
      {
        std::lock_guard<std::mutex> lock(callback_mutex);
        if (update_remote_state) {
          state_snapshot.remote_track_open = true;
        } else if (!state_snapshot.remote_track_open) {
          state_snapshot.remote_track_open = true;
        }
      }
      EmitState();
    });
    track->onClosed([this, update_remote_state]() {
      {
        std::lock_guard<std::mutex> lock(callback_mutex);
        if (update_remote_state) {
          state_snapshot.remote_track_open = false;
        } else if (remote_track == nullptr) {
          state_snapshot.remote_track_open = false;
        }
      }
      EmitState();
    });
    track->onMessage(
        [this](rtc::binary data) {
          auto packet = inbound_rtp.Parse(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
          if (!packet.ok()) {
            return;
          }

          std::lock_guard<std::mutex> lock(queue_mutex);
          if (incoming_packets.size() >= kIncomingQueueLimit) {
            incoming_packets.pop_front();
          }
          incoming_packets.push_back(std::move(packet.value()));
        },
        nullptr);
  }

  void EmitState() {
    StateChangeCallback callback_copy;
    LibDatachannelStateSnapshot snapshot_copy;
    {
      std::lock_guard<std::mutex> lock(callback_mutex);
      callback_copy = state_callback;
      snapshot_copy = state_snapshot;
    }
    if (callback_copy) {
      callback_copy(snapshot_copy);
    }
  }

  void SetState(const rtc::PeerConnection::State state) {
    {
      std::lock_guard<std::mutex> lock(callback_mutex);
      state_snapshot.connection_state = ToString(state);
      state_snapshot.connected = state == rtc::PeerConnection::State::Connected;
      if (state != rtc::PeerConnection::State::Connected) {
        negotiated_dtls_srtp_key_block.reset();
        state_snapshot.dtls_srtp_keying_ready = false;
        state_snapshot.dtls_role = "unknown";
        state_snapshot.srtp_profile = "unknown";
        state_snapshot.dtls_srtp_key_block_bytes = 0;
      }
    }
    if (state == rtc::PeerConnection::State::Connected) {
      RefreshDtlsSrtpKeying();
    }
    EmitState();
  }

  void SetIceState(const rtc::PeerConnection::IceState state) {
    {
      std::lock_guard<std::mutex> lock(callback_mutex);
      state_snapshot.ice_state = ToString(state);
    }
    EmitState();
  }

  void SetGatheringState(const rtc::PeerConnection::GatheringState state) {
    {
      std::lock_guard<std::mutex> lock(callback_mutex);
      state_snapshot.gathering_state = ToString(state);
    }
    EmitState();
  }

  void OnRemoteTrack(const std::shared_ptr<rtc::Track>& track) {
    AttachIncomingTrack(track, true);
    RefreshDtlsSrtpKeying();
    EmitState();
  }

  void RefreshDtlsSrtpKeying() {
    if (peer == nullptr) {
      return;
    }

    auto exported = ExportDtlsSrtpKeyBlock(*peer);
    if (!exported.ok()) {
      return;
    }

    std::lock_guard<std::mutex> lock(callback_mutex);
    negotiated_dtls_srtp_key_block = exported.value();
    state_snapshot.dtls_srtp_keying_ready = true;
    state_snapshot.dtls_role = ToString(exported.value().local_role);
    state_snapshot.srtp_profile = ToString(exported.value().profile);
    state_snapshot.dtls_srtp_key_block_bytes = exported.value().key_block.size();
  }

  core::Status FlushPendingRemoteCandidates() {
    if (config.use_fake_transport) {
      pending_remote_candidates.clear();
      return core::OkStatus();
    }
    if (!remote_description_set) {
      return core::OkStatus();
    }

    for (const auto& candidate : pending_remote_candidates) {
      peer->addRemoteCandidate(rtc::Candidate(candidate.first, candidate.second));
    }
    pending_remote_candidates.clear();
    return core::OkStatus();
  }

  void SetFakeConnected(const bool connected, const DtlsRole role) {
    {
      std::lock_guard<std::mutex> lock(callback_mutex);
      state_snapshot.connected = connected;
      state_snapshot.local_track_open = connected;
      state_snapshot.remote_track_open = connected;
      state_snapshot.connection_state = connected ? "connected" : "new";
      state_snapshot.ice_state = connected ? "completed" : "new";
      state_snapshot.gathering_state = "complete";
      state_snapshot.dtls_srtp_keying_ready = connected;
      state_snapshot.dtls_role = connected ? ToString(role) : "unknown";
      state_snapshot.srtp_profile = connected ? ToString(SrtpProfile::kAes128CmSha1_80) : "unknown";
      state_snapshot.dtls_srtp_key_block_bytes = connected ? DtlsSrtpKeyBlockSize(SrtpProfile::kAes128CmSha1_80) : 0;
      if (connected) {
        negotiated_dtls_srtp_key_block = BuildFakeDtlsSrtpKeyBlock(role);
      } else {
        negotiated_dtls_srtp_key_block.reset();
      }
    }
    EmitState();
  }

  static DtlsSrtpKeyBlock BuildFakeDtlsSrtpKeyBlock(const DtlsRole role) {
    DtlsSrtpKeyBlock block;
    block.profile = SrtpProfile::kAes128CmSha1_80;
    block.local_role = role;
    block.key_block.resize(DtlsSrtpKeyBlockSize(block.profile));
    for (std::size_t index = 0; index < block.key_block.size(); ++index) {
      block.key_block[index] = static_cast<std::uint8_t>(index + 1);
    }
    return block;
  }

  core::Status ConnectFakeRemote(const std::uint32_t remote_id, const DtlsRole local_role) {
    auto remote_endpoint = FindFakeEndpoint(remote_id);
    if (remote_endpoint == nullptr || remote_endpoint->owner == nullptr) {
      return BuildTransportError("Fake transport could not resolve remote peer");
    }

    {
      std::lock_guard<std::mutex> lock(callback_mutex);
      fake_endpoint->remote = remote_endpoint;
      remote_description_set = true;
    }
    {
      std::lock_guard<std::mutex> lock(remote_endpoint->owner->callback_mutex);
      remote_endpoint->remote = fake_endpoint;
      remote_endpoint->owner->remote_description_set = true;
    }

    SetFakeConnected(true, local_role);
    remote_endpoint->owner->SetFakeConnected(true, local_role == DtlsRole::kClient ? DtlsRole::kServer
                                                                                   : DtlsRole::kClient);
    return core::OkStatus();
  }

  static constexpr std::size_t kIncomingQueueLimit = 128;

  LibDatachannelPeerConfig config;
  std::shared_ptr<InspectablePeerConnection> peer;
  std::shared_ptr<rtc::Track> local_track;
  std::shared_ptr<rtc::Track> remote_track;
  RtpPacketizer outbound_rtp;
  RtpPacketizer inbound_rtp;
  mutable std::mutex callback_mutex;
  mutable std::mutex queue_mutex;
  LocalDescriptionCallback local_description_callback;
  LocalCandidateCallback local_candidate_callback;
  StateChangeCallback state_callback;
  LibDatachannelStateSnapshot state_snapshot{};
  std::deque<OpusPacket> incoming_packets{};
  std::vector<std::pair<std::string, std::string>> pending_remote_candidates{};
  std::optional<DtlsSrtpKeyBlock> negotiated_dtls_srtp_key_block{};
  bool remote_description_set{false};
  std::shared_ptr<FakeTransportEndpoint> fake_endpoint;
};

LibDatachannelAudioPeer::LibDatachannelAudioPeer() = default;

LibDatachannelAudioPeer::LibDatachannelAudioPeer(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

LibDatachannelAudioPeer::LibDatachannelAudioPeer(LibDatachannelAudioPeer&& other) noexcept = default;

LibDatachannelAudioPeer& LibDatachannelAudioPeer::operator=(LibDatachannelAudioPeer&& other) noexcept = default;

LibDatachannelAudioPeer::~LibDatachannelAudioPeer() = default;

core::Result<LibDatachannelAudioPeer> LibDatachannelAudioPeer::Create(const LibDatachannelPeerConfig& config) {
  auto impl = std::make_unique<Impl>(config);
  if (config.use_fake_transport) {
    return LibDatachannelAudioPeer(std::move(impl));
  }

  rtc::Configuration rtc_config;
  rtc_config.forceMediaTransport = true;
  rtc_config.enableIceTcp = config.enable_ice_tcp;
  rtc_config.disableAutoGathering = config.disable_auto_gathering;
  rtc_config.iceTransportPolicy =
      config.relay_only ? rtc::TransportPolicy::Relay : rtc::TransportPolicy::All;
  if (config.mtu.has_value()) {
    rtc_config.mtu = config.mtu;
  }

  for (const auto& server_config : config.ice_servers) {
    rtc::IceServer server(server_config.url);
    server.username = server_config.username;
    server.password = server_config.password;
    rtc_config.iceServers.push_back(server);
  }

  impl->peer = std::make_shared<InspectablePeerConnection>(rtc_config);
  if (impl->peer == nullptr) {
    return BuildTransportError("Unable to allocate libdatachannel peer");
  }

  impl->peer->onStateChange([ptr = impl.get()](const rtc::PeerConnection::State state) { ptr->SetState(state); });
  impl->peer->onIceStateChange(
      [ptr = impl.get()](const rtc::PeerConnection::IceState state) { ptr->SetIceState(state); });
  impl->peer->onGatheringStateChange([ptr = impl.get()](const rtc::PeerConnection::GatheringState state) {
    ptr->SetGatheringState(state);
  });
  impl->peer->onLocalDescription([ptr = impl.get()](rtc::Description description) {
    LocalDescriptionCallback callback;
    {
      std::lock_guard<std::mutex> lock(ptr->callback_mutex);
      callback = ptr->local_description_callback;
    }
    if (callback) {
      callback(description.typeString(), std::string(description));
    }
  });
  impl->peer->onLocalCandidate([ptr = impl.get()](rtc::Candidate candidate) {
    LocalCandidateCallback callback;
    {
      std::lock_guard<std::mutex> lock(ptr->callback_mutex);
      callback = ptr->local_candidate_callback;
    }
    if (callback) {
      callback(candidate.candidate(), candidate.mid());
    }
  });
  impl->peer->onTrack([ptr = impl.get()](const std::shared_ptr<rtc::Track>& track) { ptr->OnRemoteTrack(track); });

  rtc::Description::Audio audio(config.mid, rtc::Description::Direction::SendRecv);
  audio.addOpusCodec(config.payload_type);
  audio.addSSRC(config.local_ssrc, config.cname, config.stream_id, config.track_id);
  impl->local_track = impl->peer->addTrack(audio);
  if (impl->local_track == nullptr) {
    return BuildTransportError("Unable to add audio track to libdatachannel peer");
  }
  impl->AttachIncomingTrack(impl->local_track, false);

  impl->local_track->onOpen([ptr = impl.get()]() {
    {
      std::lock_guard<std::mutex> lock(ptr->callback_mutex);
      ptr->state_snapshot.local_track_open = true;
      if (ptr->remote_track == nullptr) {
        ptr->state_snapshot.remote_track_open = true;
      }
    }
    ptr->RefreshDtlsSrtpKeying();
    ptr->EmitState();
  });
  impl->local_track->onClosed([ptr = impl.get()]() {
    {
      std::lock_guard<std::mutex> lock(ptr->callback_mutex);
      ptr->state_snapshot.local_track_open = false;
      if (ptr->remote_track == nullptr) {
        ptr->state_snapshot.remote_track_open = false;
      }
    }
    ptr->EmitState();
  });

  return LibDatachannelAudioPeer(std::move(impl));
}

core::Status LibDatachannelAudioPeer::StartOffer() {
  if (impl_ == nullptr) {
    return BuildTransportError("Peer is not initialized");
  }
  if (impl_->config.use_fake_transport) {
    LocalDescriptionCallback callback;
    {
      std::lock_guard<std::mutex> lock(impl_->callback_mutex);
      callback = impl_->local_description_callback;
    }
    if (callback) {
      callback("offer", "fake-offer:" + std::to_string(impl_->config.local_ssrc));
    }
    return core::OkStatus();
  }
  if (impl_->peer == nullptr) {
    return BuildTransportError("Peer is not initialized");
  }
  impl_->peer->setLocalDescription(rtc::Description::Type::Offer);
  return core::OkStatus();
}

core::Status LibDatachannelAudioPeer::SetRemoteDescription(const std::string& type, const std::string& sdp) {
  if (impl_ == nullptr) {
    return BuildTransportError("Peer is not initialized");
  }
  if (impl_->config.use_fake_transport) {
    const auto remote_id = ParseFakeDescriptionId(sdp);
    if (!remote_id.has_value()) {
      return BuildTransportError("Fake transport received an invalid SDP description");
    }
    const auto connected = impl_->ConnectFakeRemote(*remote_id, type == "offer" ? DtlsRole::kServer : DtlsRole::kClient);
    if (!connected.ok()) {
      return connected;
    }
    if (type == "offer") {
      LocalDescriptionCallback callback;
      {
        std::lock_guard<std::mutex> lock(impl_->callback_mutex);
        callback = impl_->local_description_callback;
      }
      if (callback) {
        callback("answer", "fake-answer:" + std::to_string(impl_->config.local_ssrc));
      }
    }
    return core::OkStatus();
  }
  if (impl_->peer == nullptr) {
    return BuildTransportError("Peer is not initialized");
  }

  impl_->peer->setRemoteDescription(rtc::Description(sdp, type));
  impl_->remote_description_set = true;
  auto flush = impl_->FlushPendingRemoteCandidates();
  if (!flush.ok()) {
    return flush;
  }
  return core::OkStatus();
}

core::Status LibDatachannelAudioPeer::AddRemoteCandidate(const std::string& candidate, const std::string& mid) {
  if (impl_ == nullptr) {
    return BuildTransportError("Peer is not initialized");
  }
  if (impl_->config.use_fake_transport) {
    static_cast<void>(candidate);
    static_cast<void>(mid);
    return core::OkStatus();
  }
  if (impl_->peer == nullptr) {
    return BuildTransportError("Peer is not initialized");
  }

  if (!impl_->remote_description_set) {
    impl_->pending_remote_candidates.emplace_back(candidate, mid);
    return core::OkStatus();
  }

  impl_->peer->addRemoteCandidate(rtc::Candidate(candidate, mid));
  return core::OkStatus();
}

void LibDatachannelAudioPeer::SetLocalDescriptionCallback(LocalDescriptionCallback callback) {
  if (impl_ == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->callback_mutex);
  impl_->local_description_callback = std::move(callback);
}

void LibDatachannelAudioPeer::SetLocalCandidateCallback(LocalCandidateCallback callback) {
  if (impl_ == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> lock(impl_->callback_mutex);
  impl_->local_candidate_callback = std::move(callback);
}

void LibDatachannelAudioPeer::SetStateChangeCallback(StateChangeCallback callback) {
  if (impl_ == nullptr) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->callback_mutex);
    impl_->state_callback = std::move(callback);
  }
  impl_->EmitState();
}

bool LibDatachannelAudioPeer::IsReady() const {
  if (impl_ != nullptr && impl_->config.use_fake_transport) {
    const auto snapshot = state();
    return snapshot.connected && snapshot.local_track_open && snapshot.remote_track_open;
  }
  return impl_ != nullptr && impl_->local_track != nullptr && impl_->local_track->isOpen();
}

LibDatachannelStateSnapshot LibDatachannelAudioPeer::state() const {
  if (impl_ == nullptr) {
    return {};
  }
  std::lock_guard<std::mutex> lock(impl_->callback_mutex);
  return impl_->state_snapshot;
}

const RtpTelemetryStats& LibDatachannelAudioPeer::outbound_transport_stats() const {
  static const RtpTelemetryStats kEmpty;
  return impl_ == nullptr ? kEmpty : impl_->outbound_rtp.stats();
}

const RtpTelemetryStats& LibDatachannelAudioPeer::inbound_transport_stats() const {
  static const RtpTelemetryStats kEmpty;
  return impl_ == nullptr ? kEmpty : impl_->inbound_rtp.stats();
}

bool LibDatachannelAudioPeer::HasNegotiatedDtlsSrtpKeyBlock() const {
  if (impl_ == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->callback_mutex);
  return impl_->negotiated_dtls_srtp_key_block.has_value();
}

core::Result<DtlsSrtpKeyBlock> LibDatachannelAudioPeer::NegotiatedDtlsSrtpKeyBlock() const {
  if (impl_ == nullptr) {
    return BuildTransportError("Peer is not initialized");
  }
  if (impl_->config.use_fake_transport) {
    std::lock_guard<std::mutex> lock(impl_->callback_mutex);
    if (!impl_->negotiated_dtls_srtp_key_block.has_value()) {
      return BuildTransportError("Fake transport DTLS-SRTP keys are not ready");
    }
    return *impl_->negotiated_dtls_srtp_key_block;
  }
  if (impl_->peer == nullptr) {
    return BuildTransportError("Peer is not initialized");
  }

  {
    std::lock_guard<std::mutex> lock(impl_->callback_mutex);
    if (impl_->negotiated_dtls_srtp_key_block.has_value()) {
      return *impl_->negotiated_dtls_srtp_key_block;
    }
  }

  auto exported = ExportDtlsSrtpKeyBlock(*impl_->peer);
  if (!exported.ok()) {
    return exported.error();
  }

  {
    std::lock_guard<std::mutex> lock(impl_->callback_mutex);
    impl_->negotiated_dtls_srtp_key_block = exported.value();
    impl_->state_snapshot.dtls_srtp_keying_ready = true;
    impl_->state_snapshot.dtls_role = ToString(exported.value().local_role);
    impl_->state_snapshot.srtp_profile = ToString(exported.value().profile);
    impl_->state_snapshot.dtls_srtp_key_block_bytes = exported.value().key_block.size();
  }
  return exported.value();
}

core::Result<SrtpSession> LibDatachannelAudioPeer::CreateExplicitSrtpSession() const {
  auto key_block = NegotiatedDtlsSrtpKeyBlock();
  if (!key_block.ok()) {
    return key_block.error();
  }
  return SrtpSession::CreateFromDtlsKeyBlock(key_block.value());
}

core::Status LibDatachannelAudioPeer::SendAudioPacket(const OpusPacket& packet) {
  if (impl_ == nullptr) {
    return BuildTransportError("Audio track is not open");
  }
  if (impl_->config.use_fake_transport) {
    std::shared_ptr<FakeTransportEndpoint> remote_endpoint;
    {
      std::lock_guard<std::mutex> lock(impl_->callback_mutex);
      remote_endpoint = impl_->fake_endpoint == nullptr ? nullptr : impl_->fake_endpoint->remote.lock();
      if (!impl_->state_snapshot.connected) {
        return BuildTransportError("Audio track is not open");
      }
    }
    if (remote_endpoint == nullptr || remote_endpoint->owner == nullptr) {
      return BuildTransportError("Fake transport is not connected to a remote peer");
    }

    auto raw = impl_->outbound_rtp.Packetize(packet);
    if (!raw.ok()) {
      return raw.error();
    }
    auto parsed = remote_endpoint->owner->inbound_rtp.Parse(raw.value());
    if (!parsed.ok()) {
      return parsed.error();
    }
    std::lock_guard<std::mutex> lock(remote_endpoint->owner->queue_mutex);
    if (remote_endpoint->owner->incoming_packets.size() >= Impl::kIncomingQueueLimit) {
      remote_endpoint->owner->incoming_packets.pop_front();
    }
    remote_endpoint->owner->incoming_packets.push_back(std::move(parsed.value()));
    return core::OkStatus();
  }
  if (impl_->local_track == nullptr || !impl_->local_track->isOpen()) {
    return BuildTransportError("Audio track is not open");
  }

  auto rtp_packet = impl_->outbound_rtp.Packetize(packet);
  if (!rtp_packet.ok()) {
    return rtp_packet.error();
  }

  rtc::binary payload;
  payload.reserve(rtp_packet.value().size());
  for (const auto byte : rtp_packet.value()) {
    payload.push_back(static_cast<std::byte>(byte));
  }
  if (!impl_->local_track->send(std::move(payload))) {
    return BuildTransportError("libdatachannel buffered the audio RTP packet");
  }
  return core::OkStatus();
}

bool LibDatachannelAudioPeer::TryPopAudioPacket(OpusPacket& packet) {
  if (impl_ == nullptr) {
    return false;
  }

  std::lock_guard<std::mutex> lock(impl_->queue_mutex);
  if (impl_->incoming_packets.empty()) {
    return false;
  }

  packet = std::move(impl_->incoming_packets.front());
  impl_->incoming_packets.pop_front();
  return true;
}

}  // namespace daffy::voice
