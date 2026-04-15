#include <cassert>
#include <memory>
#include <string>
#include <thread>

#include "daffy/signaling/signaling_client_facade.hpp"
#include "daffy/util/json.hpp"
#include "daffy/voice/native_voice_client.hpp"

namespace {

bool WaitUntil(const std::function<bool()>& predicate, const int attempts = 100, const int sleep_ms = 10) {
  for (int attempt = 0; attempt < attempts; ++attempt) {
    if (predicate()) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  }
  return false;
}

daffy::voice::VoiceSessionPlan BuildPlan() {
  daffy::voice::VoiceSessionPlan plan;
  plan.input_device = {0, "Fake Mic", "test", 1, 0, 48000.0, 0.01, 0.0, true, false};
  plan.output_device = {1, "Fake Speaker", "test", 0, 1, 48000.0, 0.0, 0.01, false, true};
  plan.capture_plan.device_format = {48000, 1};
  plan.capture_plan.pipeline_format = {48000, 1};
  plan.capture_plan.device_frames_per_buffer = 480;
  plan.playback_plan = plan.capture_plan;
  plan.input_stream = {0, 1, 48000.0, 480, 0.01};
  plan.output_stream = {1, 1, 48000.0, 480, 0.01};
  return plan;
}

struct FakeLiveState {
  daffy::voice::LiveVoiceSessionStateSnapshot snapshot{};
  daffy::voice::LiveVoiceSessionTelemetry telemetry{};
  daffy::voice::VoiceSessionPlan plan{BuildPlan()};
  daffy::voice::AudioDeviceInventory devices{};
  int update_transport_calls{0};
  int start_negotiation_calls{0};
  std::uint64_t action_counter{0};
  std::uint64_t update_transport_order{0};
  std::uint64_t start_negotiation_order{0};
  std::string last_negotiation_target;
  daffy::voice::LibDatachannelPeerConfig last_transport_config{};
};

struct FakeSignalingState {
  daffy::signaling::NativeSignalingClientStateSnapshot snapshot{};
  daffy::signaling::NativeSignalingClientTelemetry telemetry{};
  daffy::signaling::NativeSignalingIceBootstrap bootstrap{};
  class FakeSignalingClient* instance{nullptr};
};

class FakeLiveVoiceSession final : public daffy::voice::LiveVoiceSessionFacade {
 public:
  FakeLiveVoiceSession(const daffy::voice::LiveVoiceSessionConfig& config, std::shared_ptr<FakeLiveState> state)
      : config_(config), state_(std::move(state)) {
    state_->snapshot.peer.room = config.peer_config.room;
    state_->snapshot.peer.peer_id = config.peer_config.peer_id;
    state_->snapshot.peer.target_peer_id = config.peer_config.target_peer_id;
  }

  daffy::core::Status Start() override {
    state_->snapshot.running = true;
    Emit();
    return daffy::core::OkStatus();
  }

  daffy::core::Status Stop() override {
    state_->snapshot.running = false;
    Emit();
    return daffy::core::OkStatus();
  }

  daffy::core::Status StartNegotiation(std::string target_peer_id) override {
    state_->last_negotiation_target = std::move(target_peer_id);
    state_->snapshot.peer.target_peer_id = state_->last_negotiation_target;
    ++state_->start_negotiation_calls;
    state_->start_negotiation_order = ++state_->action_counter;
    Emit();
    return daffy::core::OkStatus();
  }

  daffy::core::Status HandleSignalingMessage(const daffy::signaling::Message& message) override {
    if (message.type == daffy::signaling::MessageType::kJoinAck) {
      if (const auto* peers = message.data.Find("peers"); peers != nullptr && peers->IsArray() && !peers->AsArray().empty()) {
        if (const auto* peer_id = peers->AsArray().front().Find("peer_id"); peer_id != nullptr && peer_id->IsString()) {
          state_->snapshot.peer.target_peer_id = peer_id->AsString();
        }
      }
    } else if (message.type == daffy::signaling::MessageType::kPeerLeft) {
      if (state_->snapshot.peer.target_peer_id == message.peer_id) {
        state_->snapshot.peer.target_peer_id.clear();
      }
    } else if (message.type == daffy::signaling::MessageType::kPeerReady) {
      state_->snapshot.peer.target_peer_id = message.peer_id;
    } else if (!message.peer_id.empty()) {
      state_->snapshot.peer.target_peer_id = message.peer_id;
    }
    Emit();
    return daffy::core::OkStatus();
  }

  daffy::core::Status UpdateTransportConfig(const daffy::voice::LibDatachannelPeerConfig& transport_config) override {
    state_->last_transport_config = transport_config;
    ++state_->update_transport_calls;
    state_->update_transport_order = ++state_->action_counter;
    return daffy::core::OkStatus();
  }

  void SetSignalingMessageCallback(daffy::voice::VoicePeerSession::SignalingMessageCallback callback) override {
    signaling_callback_ = std::move(callback);
  }

  void SetStateChangeCallback(daffy::voice::LiveVoiceSession::StateChangeCallback callback) override {
    state_callback_ = std::move(callback);
    Emit();
  }

  [[nodiscard]] bool IsRunning() const override { return state_->snapshot.running; }
  [[nodiscard]] const daffy::voice::VoiceSessionPlan& plan() const override { return state_->plan; }
  [[nodiscard]] const daffy::voice::AudioDeviceInventory& devices() const override { return state_->devices; }
  [[nodiscard]] daffy::voice::LiveVoiceSessionStateSnapshot state() const override { return state_->snapshot; }
  [[nodiscard]] daffy::voice::LiveVoiceSessionTelemetry telemetry() const override { return state_->telemetry; }

 private:
  void Emit() {
    if (state_callback_) {
      state_callback_(state_->snapshot);
    }
  }

  daffy::voice::LiveVoiceSessionConfig config_;
  std::shared_ptr<FakeLiveState> state_;
  daffy::voice::VoicePeerSession::SignalingMessageCallback signaling_callback_;
  daffy::voice::LiveVoiceSession::StateChangeCallback state_callback_;
};

class FakeSignalingClient final : public daffy::signaling::SignalingClientFacade {
 public:
  explicit FakeSignalingClient(std::shared_ptr<FakeSignalingState> state) : state_(std::move(state)) {
    state_->instance = this;
  }

  daffy::core::Status Start() override {
    state_->snapshot.started = true;
    state_->snapshot.websocket_open = true;
    EmitState();
    return daffy::core::OkStatus();
  }

  daffy::core::Status Stop() override {
    state_->snapshot.started = false;
    state_->snapshot.websocket_open = false;
    state_->snapshot.joined = false;
    EmitState();
    return daffy::core::OkStatus();
  }

  daffy::core::Status Send(const daffy::signaling::Message&) override {
    ++state_->telemetry.outbound_messages;
    return daffy::core::OkStatus();
  }

  void SetMessageCallback(daffy::signaling::NativeSignalingClient::MessageCallback callback) override {
    message_callback_ = std::move(callback);
  }

  void SetStateChangeCallback(daffy::signaling::NativeSignalingClient::StateChangeCallback callback) override {
    state_callback_ = std::move(callback);
    EmitState();
  }

  [[nodiscard]] bool IsStarted() const override { return state_->snapshot.started; }
  [[nodiscard]] daffy::signaling::NativeSignalingClientStateSnapshot state() const override { return state_->snapshot; }
  [[nodiscard]] daffy::signaling::NativeSignalingClientTelemetry telemetry() const override {
    return state_->telemetry;
  }
  [[nodiscard]] daffy::signaling::NativeSignalingIceBootstrap bootstrap() const override { return state_->bootstrap; }

  void DeliverJoinAck(const std::string& peer_id, const daffy::signaling::NativeSignalingIceBootstrap& bootstrap) {
    state_->bootstrap = bootstrap;
    state_->snapshot.joined = true;
    state_->snapshot.has_turn_credentials = !bootstrap.turn_uri.empty();
    state_->snapshot.discovered_ice_servers =
        bootstrap.stun_servers.size() + (bootstrap.turn_uri.empty() ? std::size_t{0} : std::size_t{1});
    EmitState();

    daffy::signaling::Message join_ack;
    join_ack.type = daffy::signaling::MessageType::kJoinAck;
    join_ack.room = state_->snapshot.room;
    join_ack.peer_id = state_->snapshot.peer_id;
    join_ack.data = daffy::util::json::Value::Object{
        {"peers", daffy::util::json::Value::Array{daffy::util::json::Value::Object{{"peer_id", peer_id}}}}};
    ++state_->telemetry.inbound_messages;
    if (message_callback_) {
      message_callback_(join_ack);
    }
  }

  void DeliverPeerLeft(const std::string& peer_id) {
    daffy::signaling::Message peer_left;
    peer_left.type = daffy::signaling::MessageType::kPeerLeft;
    peer_left.room = state_->snapshot.room;
    peer_left.peer_id = peer_id;
    ++state_->telemetry.inbound_messages;
    if (message_callback_) {
      message_callback_(peer_left);
    }
  }

  void DeliverPeerReady(const std::string& peer_id) {
    daffy::signaling::Message peer_ready;
    peer_ready.type = daffy::signaling::MessageType::kPeerReady;
    peer_ready.room = state_->snapshot.room;
    peer_ready.peer_id = peer_id;
    ++state_->telemetry.inbound_messages;
    if (message_callback_) {
      message_callback_(peer_ready);
    }
  }

 private:
  void EmitState() {
    if (state_callback_) {
      state_callback_(state_->snapshot);
    }
  }

  std::shared_ptr<FakeSignalingState> state_;
  daffy::signaling::NativeSignalingClient::MessageCallback message_callback_;
  daffy::signaling::NativeSignalingClient::StateChangeCallback state_callback_;
};

bool ContainsServer(const std::vector<daffy::voice::IceServerConfig>& servers,
                    const std::string& url,
                    const std::string& username,
                    const std::string& password) {
  for (const auto& server : servers) {
    if (server.url == url && server.username == username && server.password == password) {
      return true;
    }
  }
  return false;
}

}  // namespace

int main() {
  auto live_state = std::make_shared<FakeLiveState>();
  auto signaling_state = std::make_shared<FakeSignalingState>();

  daffy::voice::NativeVoiceClientConfig config;
  config.live_config.peer_config.room = "alpha";
  config.live_config.peer_config.peer_id = "peer-a";
  config.live_config.peer_config.transport_config.ice_servers = {
      {"stun:static.example.org:3478", {}, {}},
      {"turn:old.example.org:3478?transport=udp", "old-user", "old-pass"},
  };
  config.signaling_config.websocket_url = "ws://127.0.0.1:7813/";
  config.signaling_config.room = "alpha";
  config.signaling_config.peer_id = "peer-a";
  config.live_factory = [live_state](const daffy::voice::LiveVoiceSessionConfig& live_config) {
    return daffy::core::Result<std::unique_ptr<daffy::voice::LiveVoiceSessionFacade>>(
        std::make_unique<FakeLiveVoiceSession>(live_config, live_state));
  };
  config.signaling_factory = [signaling_state](const daffy::signaling::NativeSignalingClientConfig&) {
    return daffy::core::Result<std::unique_ptr<daffy::signaling::SignalingClientFacade>>(
        std::make_unique<FakeSignalingClient>(signaling_state));
  };

  auto client = daffy::voice::NativeVoiceClient::Create(config);
  assert(client.ok());
  assert(client.value().Start().ok());

  daffy::signaling::NativeSignalingIceBootstrap bootstrap;
  bootstrap.stun_servers = {"stun:join.example.org:19302"};
  bootstrap.turn_uri = "turn:fresh.example.org:3478?transport=udp";
  bootstrap.turn_username = "fresh-user";
  bootstrap.turn_password = "fresh-pass";
  signaling_state->instance->DeliverJoinAck("peer-b", bootstrap);

  assert(WaitUntil([&live_state]() { return live_state->start_negotiation_calls >= 1; }));
  assert(live_state->update_transport_calls >= 1);
  assert(live_state->update_transport_order > 0);
  assert(live_state->start_negotiation_order > live_state->update_transport_order);
  assert(live_state->last_negotiation_target == "peer-b");

  signaling_state->instance->DeliverPeerLeft("peer-b");
  assert(WaitUntil([&client]() {
    const auto state = client.value().state();
    return !state.negotiation_started && state.live.peer.target_peer_id.empty();
  }));
  assert(client.value().state().negotiation_resets >= 1);
  assert(client.value().state().last_negotiation_reset_reason == "target-peer-cleared");

  signaling_state->instance->DeliverPeerReady("peer-b");
  assert(WaitUntil([&live_state]() { return live_state->start_negotiation_calls >= 2; }));
  assert(live_state->last_negotiation_target == "peer-b");
  assert(client.value().telemetry().negotiation_reoffers == 2);
  assert(client.value().telemetry().last_reoffer_reason == "reconnect-auto-offer");

  const auto state_json = daffy::util::json::Serialize(daffy::voice::NativeVoiceClientStateToJson(client.value().state()));
  const auto telemetry_json =
      daffy::util::json::Serialize(daffy::voice::NativeVoiceClientTelemetryToJson(client.value().telemetry()));
  assert(state_json.find("\"last_negotiation_reset_reason\":\"target-peer-cleared\"") != std::string::npos);
  assert(telemetry_json.find("\"last_reoffer_reason\":\"reconnect-auto-offer\"") != std::string::npos);
  assert(telemetry_json.find("\"turn_fetch_retry_attempts\":0") != std::string::npos);

  const auto& servers = live_state->last_transport_config.ice_servers;
  assert(ContainsServer(servers, "stun:static.example.org:3478", {}, {}));
  assert(ContainsServer(servers, "stun:join.example.org:19302", {}, {}));
  assert(ContainsServer(servers, "turn:fresh.example.org:3478?transport=udp", "fresh-user", "fresh-pass"));
  assert(!ContainsServer(servers, "turn:old.example.org:3478?transport=udp", "old-user", "old-pass"));

  assert(client.value().Stop().ok());
  return 0;
}
