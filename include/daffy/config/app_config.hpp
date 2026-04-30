#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/util/json.hpp"

namespace daffy::config {

struct ServerConfig {
  std::string bind_address{"0.0.0.0"};
  int port{7000};
  std::string public_base_url{"http://127.0.0.1:7000"};
  std::string log_level{"info"};
};

struct SignalingConfig {
  std::string bind_address{"0.0.0.0"};
  int port{7001};
  int max_room_size{2};
  int ping_interval_ms{15000};
  int ping_timeout_ms{30000};
  int reconnect_grace_ms{45000};
  std::string health_endpoint{"/healthz"};
  std::string debug_rooms_endpoint{"/debug/rooms"};
  std::string turn_credentials_endpoint{"/debug/turn-credentials"};
  std::vector<std::string> stun_servers{"stun:stun.l.google.com:19302"};
  bool allow_browser_signaling{true};
};

struct TurnConfig {
  std::string uri{"turn:turn.example.com:3478?transport=udp"};
  std::string username{"daffy"};
  std::string password{"change-me"};
  std::string credential_mode{"static"};
};

struct RuntimeIsolationConfig {
  std::string provider{"process"};
  std::string workspace_root{"./var/rooms"};
  bool enable_lxc{false};
  std::string lxc_template{"busybox"};
};

struct ServicesConfig {
  std::string ipc_url{"ipc:///tmp/daffychat-services.ipc"};
  std::string builtin_service_dir{"./services/builtin"};
  std::string generated_service_dir{"./services/generated"};
  bool enable_webhooks{true};
};

struct FrontendBridgeConfig {
  bool enabled{true};
  std::string asset_root{"./frontend"};
  std::string bridge_endpoint{"/bridge"};
  bool allow_wasm_extensions{true};
  std::string voice_transport{"browser-socketio"};
};

struct VoiceConfig {
  std::string preferred_input_device{"default"};
  std::string preferred_output_device{"default"};
  int preferred_capture_sample_rate{48000};
  int preferred_playback_sample_rate{48000};
  int preferred_channels{1};
  int frames_per_buffer{480};
  int playout_buffer_frames{3};
  int max_playout_buffer_frames{8};
  bool enable_noise_suppression{true};
  bool enable_metrics{true};
};

struct AppConfig {
  ServerConfig server{};
  SignalingConfig signaling{};
  TurnConfig turn{};
  RuntimeIsolationConfig runtime_isolation{};
  ServicesConfig services{};
  FrontendBridgeConfig frontend_bridge{};
  VoiceConfig voice{};
};

AppConfig DefaultAppConfig();
std::string DescribeAppConfig(const AppConfig& config);
std::string ExampleConfigPath();
std::string SystemConfigPath();
std::string DefaultConfigPath();
core::Result<AppConfig> ParseAppConfigFromJson(std::string_view json_text);
core::Result<AppConfig> LoadAppConfigFromFile(const std::string& path);
util::json::Value AppConfigToJson(const AppConfig& config);
std::string SerializeAppConfig(const AppConfig& config);

}  // namespace daffy::config
