#include "daffy/voice/native_voice_client.hpp"

#include <mutex>
#include <utility>
#include <vector>

namespace daffy::voice {
namespace {

core::Error BuildNativeVoiceClientError(const std::string& message) {
  return core::Error{core::ErrorCode::kStateError, message};
}

bool IsTurnServerUrl(const std::string& url) {
  return url.rfind("turn:", 0) == 0 || url.rfind("turns:", 0) == 0;
}

void AppendUniqueIceServer(std::vector<IceServerConfig>& servers, const IceServerConfig& server) {
  for (const auto& existing : servers) {
    if (existing.url == server.url && existing.username == server.username && existing.password == server.password) {
      return;
    }
  }
  servers.push_back(server);
}

LibDatachannelPeerConfig MergeIceServers(const LibDatachannelPeerConfig& base,
                                         const signaling::NativeSignalingIceBootstrap& bootstrap) {
  LibDatachannelPeerConfig merged = base;
  std::vector<IceServerConfig> resolved_servers;
  resolved_servers.reserve(base.ice_servers.size() + bootstrap.stun_servers.size() + 1);

  for (const auto& server : base.ice_servers) {
    if (!IsTurnServerUrl(server.url)) {
      AppendUniqueIceServer(resolved_servers, server);
    }
  }
  for (const auto& stun_server : bootstrap.stun_servers) {
    AppendUniqueIceServer(resolved_servers, IceServerConfig{stun_server, {}, {}});
  }
  if (!bootstrap.turn_uri.empty()) {
    AppendUniqueIceServer(
        resolved_servers, IceServerConfig{bootstrap.turn_uri, bootstrap.turn_username, bootstrap.turn_password});
  } else {
    for (const auto& server : base.ice_servers) {
      if (IsTurnServerUrl(server.url)) {
        AppendUniqueIceServer(resolved_servers, server);
      }
    }
  }

  merged.ice_servers = std::move(resolved_servers);
  return merged;
}

std::string SerializeIceServers(const std::vector<IceServerConfig>& servers) {
  std::string serialized;
  for (const auto& server : servers) {
    serialized += server.url;
    serialized.push_back('|');
    serialized += server.username;
    serialized.push_back('|');
    serialized += server.password;
    serialized.push_back('\n');
  }
  return serialized;
}

}  // namespace

util::json::Value NativeVoiceClientStateToJson(const NativeVoiceClientStateSnapshot& state) {
  return util::json::Value::Object{
      {"running", state.running},
      {"negotiation_started", state.negotiation_started},
      {"negotiation_resets", static_cast<int>(state.negotiation_resets)},
      {"negotiation_reoffers", static_cast<int>(state.negotiation_reoffers)},
      {"last_negotiation_reset_reason", state.last_negotiation_reset_reason},
      {"last_reoffer_reason", state.last_reoffer_reason},
      {"last_error", state.last_error},
      {"live", LiveVoiceSessionStateToJson(state.live)},
      {"signaling", signaling::NativeSignalingClientStateToJson(state.signaling)}};
}

util::json::Value NativeVoiceClientTelemetryToJson(const NativeVoiceClientTelemetry& telemetry) {
  return util::json::Value::Object{
      {"live", LiveVoiceSessionTelemetryToJson(telemetry.live)},
      {"signaling", signaling::NativeSignalingClientTelemetryToJson(telemetry.signaling)},
      {"negotiation_resets", static_cast<int>(telemetry.negotiation_resets)},
      {"negotiation_reoffers", static_cast<int>(telemetry.negotiation_reoffers)},
      {"last_negotiation_reset_reason", telemetry.last_negotiation_reset_reason},
      {"last_reoffer_reason", telemetry.last_reoffer_reason}};
}

struct NativeVoiceClient::Impl {
  explicit Impl(NativeVoiceClientConfig client_config,
                std::unique_ptr<LiveVoiceSessionFacade> live_session,
                std::unique_ptr<signaling::SignalingClientFacade> signaling_client)
      : config(std::move(client_config)), live(std::move(live_session)), signaling(std::move(signaling_client)) {}

  void EmitState() {
    StateChangeCallback callback;
    NativeVoiceClientStateSnapshot snapshot_copy;
    {
      std::lock_guard<std::mutex> lock(mutex);
      snapshot_copy = state_snapshot;
      callback = state_callback;
    }
    if (callback) {
      callback(snapshot_copy);
    }
  }

  void SetLastError(std::string error) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      state_snapshot.last_error = std::move(error);
    }
    EmitState();
  }

  void ResetNegotiationState(std::string reason) {
    std::lock_guard<std::mutex> lock(mutex);
    if (state_snapshot.negotiation_started || !active_negotiation_target.empty()) {
      ++state_snapshot.negotiation_resets;
      state_snapshot.last_negotiation_reset_reason = std::move(reason);
    }
    state_snapshot.negotiation_started = false;
    active_negotiation_target.clear();
  }

  void ReconcileNegotiationState(const LiveVoiceSessionStateSnapshot& live_state) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!live_state.running || live_state.peer.target_peer_id.empty()) {
      if (state_snapshot.negotiation_started || !active_negotiation_target.empty()) {
        ++state_snapshot.negotiation_resets;
        state_snapshot.last_negotiation_reset_reason =
            !live_state.running ? "live-session-stopped" : "target-peer-cleared";
      }
      state_snapshot.negotiation_started = false;
      active_negotiation_target.clear();
      return;
    }
    if (!active_negotiation_target.empty() && live_state.peer.target_peer_id != active_negotiation_target) {
      ++state_snapshot.negotiation_resets;
      state_snapshot.last_negotiation_reset_reason = "target-peer-changed:" + live_state.peer.target_peer_id;
      state_snapshot.negotiation_started = false;
      active_negotiation_target.clear();
    }
  }

  void MaybeApplyIceBootstrap() {
    signaling::NativeSignalingIceBootstrap bootstrap;
    LibDatachannelPeerConfig base_transport;
    std::string signature;
    bool should_apply = false;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (!state_snapshot.running || !state_snapshot.signaling.joined || state_snapshot.negotiation_started ||
          signaling == nullptr || live == nullptr) {
        return;
      }
      bootstrap = signaling->bootstrap();
      base_transport = config.live_config.peer_config.transport_config;
      auto merged = MergeIceServers(base_transport, bootstrap);
      signature = SerializeIceServers(merged.ice_servers);
      should_apply = !signature.empty() && signature != applied_ice_signature;
    }
    if (!should_apply) {
      return;
    }

    auto merged = MergeIceServers(base_transport, bootstrap);
    auto applied = live->UpdateTransportConfig(merged);
    if (!applied.ok()) {
      SetLastError(applied.error().ToString());
      return;
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      config.live_config.peer_config.transport_config = merged;
      applied_ice_signature = signature;
    }
  }

  void MaybeStartNegotiation() {
    std::string target_peer_id;
    std::string local_peer_id;
    bool should_offer = false;
    bool explicit_target = false;
    {
      std::lock_guard<std::mutex> lock(mutex);
      should_offer = config.auto_offer && state_snapshot.running && state_snapshot.signaling.joined &&
                     !state_snapshot.negotiation_started;
      target_peer_id = state_snapshot.live.peer.target_peer_id;
      local_peer_id = config.live_config.peer_config.peer_id;
      explicit_target = !config.live_config.peer_config.target_peer_id.empty();
    }
    if (!should_offer || target_peer_id.empty()) {
      return;
    }
    if (!explicit_target && local_peer_id >= target_peer_id) {
      return;
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      if (state_snapshot.negotiation_started) {
        return;
      }
      state_snapshot.negotiation_started = true;
      ++state_snapshot.negotiation_reoffers;
      state_snapshot.last_reoffer_reason = state_snapshot.negotiation_reoffers == 1 ? "initial-auto-offer"
                                                                                   : "reconnect-auto-offer";
    }
    EmitState();

    auto started = live->StartNegotiation(target_peer_id);
    if (!started.ok()) {
      ResetNegotiationState("offer-start-failed");
      EmitState();
      SetLastError(started.error().ToString());
      return;
    }

    {
      std::lock_guard<std::mutex> lock(mutex);
      active_negotiation_target = target_peer_id;
    }
  }

  NativeVoiceClientConfig config;
  std::unique_ptr<LiveVoiceSessionFacade> live;
  std::unique_ptr<signaling::SignalingClientFacade> signaling;
  mutable std::mutex mutex;
  StateChangeCallback state_callback;
  NativeVoiceClientStateSnapshot state_snapshot{};
  std::string applied_ice_signature;
  std::string active_negotiation_target;
};

NativeVoiceClient::NativeVoiceClient() = default;

NativeVoiceClient::NativeVoiceClient(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

NativeVoiceClient::NativeVoiceClient(NativeVoiceClient&& other) noexcept = default;

NativeVoiceClient& NativeVoiceClient::operator=(NativeVoiceClient&& other) noexcept = default;

NativeVoiceClient::~NativeVoiceClient() {
  if (impl_ != nullptr) {
    static_cast<void>(Stop());
  }
}

core::Result<NativeVoiceClient> NativeVoiceClient::Create(const NativeVoiceClientConfig& config) {
  NativeVoiceClientConfig resolved_config = config;
  if (resolved_config.live_config.peer_config.room.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Native voice client requires a room id"};
  }
  if (resolved_config.live_config.peer_config.peer_id.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Native voice client requires a peer id"};
  }
  if (resolved_config.signaling_config.room.empty()) {
    resolved_config.signaling_config.room = resolved_config.live_config.peer_config.room;
  }
  if (resolved_config.signaling_config.peer_id.empty()) {
    resolved_config.signaling_config.peer_id = resolved_config.live_config.peer_config.peer_id;
  }
  if (resolved_config.signaling_config.room != resolved_config.live_config.peer_config.room ||
      resolved_config.signaling_config.peer_id != resolved_config.live_config.peer_config.peer_id) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Native voice client room/peer must match live session config"};
  }

  auto live_factory = resolved_config.live_factory;
  if (!live_factory) {
    live_factory = CreateDefaultLiveVoiceSessionFacade;
  }
  auto signaling_factory = resolved_config.signaling_factory;
  if (!signaling_factory) {
    signaling_factory = signaling::CreateDefaultSignalingClientFacade;
  }

  auto live = live_factory(resolved_config.live_config);
  if (!live.ok()) {
    return live.error();
  }

  auto signaling = signaling_factory(resolved_config.signaling_config);
  if (!signaling.ok()) {
    return signaling.error();
  }

  auto impl =
      std::make_unique<Impl>(std::move(resolved_config), std::move(live.value()), std::move(signaling.value()));
  impl->state_snapshot.live = impl->live->state();
  impl->state_snapshot.signaling = impl->signaling->state();

  impl->live->SetSignalingMessageCallback([ptr = impl.get()](const signaling::Message& message) {
    auto sent = ptr->signaling->Send(message);
    if (!sent.ok()) {
      ptr->SetLastError(sent.error().ToString());
    }
  });
  impl->live->SetStateChangeCallback([ptr = impl.get()](const LiveVoiceSessionStateSnapshot& state) {
    ptr->ReconcileNegotiationState(state);
    {
      std::lock_guard<std::mutex> lock(ptr->mutex);
      ptr->state_snapshot.live = state;
      if (!state.last_error.empty()) {
        ptr->state_snapshot.last_error = state.last_error;
      }
    }
    ptr->EmitState();
    ptr->MaybeStartNegotiation();
  });
  impl->signaling->SetMessageCallback([ptr = impl.get()](const signaling::Message& message) {
    ptr->MaybeApplyIceBootstrap();
    auto applied = ptr->live->HandleSignalingMessage(message);
    if (!applied.ok()) {
      ptr->SetLastError(applied.error().ToString());
      return;
    }
    ptr->MaybeStartNegotiation();
  });
  impl->signaling->SetStateChangeCallback(
      [ptr = impl.get()](const signaling::NativeSignalingClientStateSnapshot& state) {
    if (!state.joined) {
      ptr->ResetNegotiationState("signaling-left");
    }
    {
      std::lock_guard<std::mutex> lock(ptr->mutex);
      ptr->state_snapshot.signaling = state;
      if (!state.last_error.empty()) {
        ptr->state_snapshot.last_error = state.last_error;
      }
    }
    ptr->EmitState();
    ptr->MaybeApplyIceBootstrap();
    ptr->MaybeStartNegotiation();
  });

  return NativeVoiceClient(std::move(impl));
}

core::Status NativeVoiceClient::Start() {
  if (impl_ == nullptr) {
    return BuildNativeVoiceClientError("Native voice client is not initialized");
  }

  auto started = impl_->live->Start();
  if (!started.ok()) {
    return started;
  }
  auto signaling_started = impl_->signaling->Start();
  if (!signaling_started.ok()) {
    static_cast<void>(impl_->live->Stop());
    return signaling_started;
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state_snapshot.running = true;
    impl_->state_snapshot.negotiation_started = false;
    impl_->state_snapshot.negotiation_resets = 0;
    impl_->state_snapshot.negotiation_reoffers = 0;
    impl_->state_snapshot.last_negotiation_reset_reason.clear();
    impl_->state_snapshot.last_reoffer_reason.clear();
    impl_->state_snapshot.live = impl_->live->state();
    impl_->state_snapshot.signaling = impl_->signaling->state();
    impl_->state_snapshot.last_error.clear();
    impl_->active_negotiation_target.clear();
  }
  impl_->EmitState();
  impl_->MaybeApplyIceBootstrap();
  impl_->MaybeStartNegotiation();
  return core::OkStatus();
}

core::Status NativeVoiceClient::Stop() {
  if (impl_ == nullptr) {
    return BuildNativeVoiceClientError("Native voice client is not initialized");
  }

  auto signaling_stopped = impl_->signaling->Stop();
  auto live_stopped = impl_->live->Stop();

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state_snapshot.running = false;
    impl_->state_snapshot.negotiation_started = false;
    impl_->state_snapshot.live = impl_->live->state();
    impl_->state_snapshot.signaling = impl_->signaling->state();
    impl_->applied_ice_signature.clear();
    impl_->active_negotiation_target.clear();
    if (!signaling_stopped.ok()) {
      impl_->state_snapshot.last_error = signaling_stopped.error().ToString();
    } else if (!live_stopped.ok()) {
      impl_->state_snapshot.last_error = live_stopped.error().ToString();
    }
  }
  impl_->EmitState();

  if (!signaling_stopped.ok()) {
    return signaling_stopped;
  }
  if (!live_stopped.ok()) {
    return live_stopped;
  }
  return core::OkStatus();
}

void NativeVoiceClient::SetStateChangeCallback(StateChangeCallback callback) {
  if (impl_ == nullptr) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state_callback = std::move(callback);
  }
  impl_->EmitState();
}

bool NativeVoiceClient::IsRunning() const {
  if (impl_ == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->state_snapshot.running;
}

NativeVoiceClientStateSnapshot NativeVoiceClient::state() const {
  if (impl_ == nullptr) {
    return {};
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->state_snapshot;
}

NativeVoiceClientTelemetry NativeVoiceClient::telemetry() const {
  NativeVoiceClientTelemetry telemetry;
  if (impl_ == nullptr) {
    return telemetry;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    telemetry.negotiation_resets = impl_->state_snapshot.negotiation_resets;
    telemetry.negotiation_reoffers = impl_->state_snapshot.negotiation_reoffers;
    telemetry.last_negotiation_reset_reason = impl_->state_snapshot.last_negotiation_reset_reason;
    telemetry.last_reoffer_reason = impl_->state_snapshot.last_reoffer_reason;
  }
  telemetry.live = impl_->live->telemetry();
  telemetry.signaling = impl_->signaling->telemetry();
  return telemetry;
}

const VoiceSessionPlan& NativeVoiceClient::plan() const {
  static const VoiceSessionPlan kEmptyPlan{};
  return impl_ == nullptr ? kEmptyPlan : impl_->live->plan();
}

const AudioDeviceInventory& NativeVoiceClient::devices() const {
  static const AudioDeviceInventory kEmptyInventory{};
  return impl_ == nullptr ? kEmptyInventory : impl_->live->devices();
}

}  // namespace daffy::voice
