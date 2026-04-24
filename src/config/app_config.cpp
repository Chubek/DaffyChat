#include "daffy/config/app_config.hpp"

#include <fstream>
#include <sstream>

#ifndef DAFFY_SOURCE_DIR
#define DAFFY_SOURCE_DIR "."
#endif

#ifndef DAFFY_SYSCONFDIR
#define DAFFY_SYSCONFDIR "/etc/daffychat"
#endif

namespace daffy::config {
namespace {

core::Result<const util::json::Value*> RequireObjectField(const util::json::Value& object,
                                                          std::string_view field_name) {
  const auto* field = object.Find(field_name);
  if (field == nullptr) {
    return core::Error{core::ErrorCode::kParseError, "Missing required config object field: " + std::string(field_name)};
  }
  if (!field->IsObject()) {
    return core::Error{core::ErrorCode::kParseError, "Config field is not a JSON object: " + std::string(field_name)};
  }
  return field;
}

void ReadString(const util::json::Value& object, std::string_view key, std::string& target) {
  if (const auto* value = object.Find(key); value != nullptr && value->IsString()) {
    target = value->AsString();
  }
}

void ReadBool(const util::json::Value& object, std::string_view key, bool& target) {
  if (const auto* value = object.Find(key); value != nullptr && value->IsBool()) {
    target = value->AsBool();
  }
}

void ReadInt(const util::json::Value& object, std::string_view key, int& target) {
  if (const auto* value = object.Find(key); value != nullptr && value->IsNumber()) {
    target = static_cast<int>(value->AsNumber());
  }
}

std::vector<std::string> ReadStringArray(const util::json::Value& object, std::string_view key) {
  std::vector<std::string> values;
  const auto* field = object.Find(key);
  if (field == nullptr || !field->IsArray()) {
    return values;
  }

  for (const auto& entry : field->AsArray()) {
    if (entry.IsString()) {
      values.push_back(entry.AsString());
    }
  }
  return values;
}

}  // namespace

AppConfig DefaultAppConfig() { return AppConfig{}; }

std::string ExampleConfigPath() { 
  return std::string(DAFFY_SOURCE_DIR) + "/config/daffychat.example.json"; 
}

std::string SystemConfigPath() {
  // Check for system config files in order of preference
  std::vector<std::string> candidates = {
    std::string(DAFFY_SYSCONFDIR) + "/daffychat.json",
    std::string(DAFFY_SYSCONFDIR) + "/daffychat.conf",
    "/etc/daffychat/daffychat.json",
    "/etc/daffychat/daffychat.conf",
  };
  
  for (const auto& path : candidates) {
    std::ifstream file(path);
    if (file.good()) {
      return path;
    }
  }
  
  // Fallback to development path if no system config exists
  return ExampleConfigPath();
}

std::string DefaultConfigPath() {
  // For installed binaries, prefer system config
  // For development, fall back to example config
  std::string sys_path = SystemConfigPath();
  if (sys_path != ExampleConfigPath()) {
    return sys_path;
  }
  return ExampleConfigPath();
}

std::string DescribeAppConfig(const AppConfig& config) {
  std::ostringstream stream;
  stream << "server=" << config.server.bind_address << ':' << config.server.port;
  stream << ", signaling=" << config.signaling.bind_address << ':' << config.signaling.port;
  stream << ", rooms=" << config.signaling.max_room_size << " peers max";
  stream << ", stun_servers=" << config.signaling.stun_servers.size();
  stream << ", reconnect_grace_ms=" << config.signaling.reconnect_grace_ms;
  stream << ", isolation=" << config.runtime_isolation.provider;
  stream << ", services=" << config.services.ipc_url;
  stream << ", frontend_bridge=" << (config.frontend_bridge.enabled ? "enabled" : "disabled");
  stream << ", voice_transport=" << config.frontend_bridge.voice_transport;
  stream << ", capture_rate=" << config.voice.preferred_capture_sample_rate;
  stream << ", playback_rate=" << config.voice.preferred_playback_sample_rate;
  stream << ", playout_buffer_frames=" << config.voice.playout_buffer_frames;
  return stream.str();
}

core::Result<AppConfig> ParseAppConfigFromJson(std::string_view json_text) {
  auto parsed = util::json::Parse(json_text);
  if (!parsed.ok()) {
    return parsed.error();
  }
  if (!parsed.value().IsObject()) {
    return core::Error{core::ErrorCode::kParseError, "App config root must be a JSON object"};
  }

  AppConfig config = DefaultAppConfig();
  const auto& root = parsed.value();

  auto server = RequireObjectField(root, "server");
  if (!server.ok()) {
    return server.error();
  }
  ReadString(*server.value(), "bind_address", config.server.bind_address);
  ReadInt(*server.value(), "port", config.server.port);
  ReadString(*server.value(), "public_base_url", config.server.public_base_url);
  ReadString(*server.value(), "log_level", config.server.log_level);

  auto signaling = RequireObjectField(root, "signaling");
  if (!signaling.ok()) {
    return signaling.error();
  }
  ReadString(*signaling.value(), "bind_address", config.signaling.bind_address);
  ReadInt(*signaling.value(), "port", config.signaling.port);
  ReadInt(*signaling.value(), "max_room_size", config.signaling.max_room_size);
  ReadInt(*signaling.value(), "ping_interval_ms", config.signaling.ping_interval_ms);
  ReadInt(*signaling.value(), "ping_timeout_ms", config.signaling.ping_timeout_ms);
  ReadInt(*signaling.value(), "reconnect_grace_ms", config.signaling.reconnect_grace_ms);
  ReadString(*signaling.value(), "health_endpoint", config.signaling.health_endpoint);
  ReadString(*signaling.value(), "debug_rooms_endpoint", config.signaling.debug_rooms_endpoint);
  ReadString(*signaling.value(), "turn_credentials_endpoint", config.signaling.turn_credentials_endpoint);
  ReadBool(*signaling.value(), "allow_browser_signaling", config.signaling.allow_browser_signaling);
  const auto stun_servers = ReadStringArray(*signaling.value(), "stun_servers");
  if (!stun_servers.empty()) {
    config.signaling.stun_servers = stun_servers;
  }

  auto turn = RequireObjectField(root, "turn");
  if (!turn.ok()) {
    return turn.error();
  }
  ReadString(*turn.value(), "uri", config.turn.uri);
  ReadString(*turn.value(), "username", config.turn.username);
  ReadString(*turn.value(), "password", config.turn.password);
  ReadString(*turn.value(), "credential_mode", config.turn.credential_mode);

  auto runtime = RequireObjectField(root, "runtime_isolation");
  if (!runtime.ok()) {
    return runtime.error();
  }
  ReadString(*runtime.value(), "provider", config.runtime_isolation.provider);
  ReadString(*runtime.value(), "workspace_root", config.runtime_isolation.workspace_root);
  ReadBool(*runtime.value(), "enable_lxc", config.runtime_isolation.enable_lxc);
  ReadString(*runtime.value(), "lxc_template", config.runtime_isolation.lxc_template);

  auto services = RequireObjectField(root, "services");
  if (!services.ok()) {
    return services.error();
  }
  ReadString(*services.value(), "ipc_url", config.services.ipc_url);
  ReadString(*services.value(), "builtin_service_dir", config.services.builtin_service_dir);
  ReadString(*services.value(), "generated_service_dir", config.services.generated_service_dir);
  ReadBool(*services.value(), "enable_webhooks", config.services.enable_webhooks);

  auto frontend_bridge = RequireObjectField(root, "frontend_bridge");
  if (!frontend_bridge.ok()) {
    return frontend_bridge.error();
  }
  ReadBool(*frontend_bridge.value(), "enabled", config.frontend_bridge.enabled);
  ReadString(*frontend_bridge.value(), "asset_root", config.frontend_bridge.asset_root);
  ReadString(*frontend_bridge.value(), "bridge_endpoint", config.frontend_bridge.bridge_endpoint);
  ReadBool(*frontend_bridge.value(), "allow_wasm_extensions", config.frontend_bridge.allow_wasm_extensions);
  ReadString(*frontend_bridge.value(), "voice_transport", config.frontend_bridge.voice_transport);

  auto voice = RequireObjectField(root, "voice");
  if (!voice.ok()) {
    return voice.error();
  }
  ReadString(*voice.value(), "preferred_input_device", config.voice.preferred_input_device);
  ReadString(*voice.value(), "preferred_output_device", config.voice.preferred_output_device);
  ReadInt(*voice.value(), "preferred_capture_sample_rate", config.voice.preferred_capture_sample_rate);
  ReadInt(*voice.value(), "preferred_playback_sample_rate", config.voice.preferred_playback_sample_rate);
  ReadInt(*voice.value(), "preferred_channels", config.voice.preferred_channels);
  ReadInt(*voice.value(), "frames_per_buffer", config.voice.frames_per_buffer);
  ReadInt(*voice.value(), "playout_buffer_frames", config.voice.playout_buffer_frames);
  ReadInt(*voice.value(), "max_playout_buffer_frames", config.voice.max_playout_buffer_frames);
  ReadBool(*voice.value(), "enable_noise_suppression", config.voice.enable_noise_suppression);
  ReadBool(*voice.value(), "enable_metrics", config.voice.enable_metrics);

  return config;
}

core::Result<AppConfig> LoadAppConfigFromFile(const std::string& path) {
  auto parsed = util::json::ParseFile(path);
  if (!parsed.ok()) {
    return parsed.error();
  }
  return ParseAppConfigFromJson(util::json::Serialize(parsed.value()));
}

util::json::Value AppConfigToJson(const AppConfig& config) {
  using util::json::Value;

  Value::Array stun_servers;
  for (const auto& stun_server : config.signaling.stun_servers) {
    stun_servers.emplace_back(stun_server);
  }

  return Value::Object{
      {"server", Value::Object{{"bind_address", config.server.bind_address},
                                {"port", config.server.port},
                                {"public_base_url", config.server.public_base_url},
                                {"log_level", config.server.log_level}}},
      {"signaling", Value::Object{{"bind_address", config.signaling.bind_address},
                                   {"port", config.signaling.port},
                                   {"max_room_size", config.signaling.max_room_size},
                                   {"ping_interval_ms", config.signaling.ping_interval_ms},
                                   {"ping_timeout_ms", config.signaling.ping_timeout_ms},
                                   {"reconnect_grace_ms", config.signaling.reconnect_grace_ms},
                                   {"health_endpoint", config.signaling.health_endpoint},
                                   {"debug_rooms_endpoint", config.signaling.debug_rooms_endpoint},
                                   {"turn_credentials_endpoint", config.signaling.turn_credentials_endpoint},
                                   {"stun_servers", stun_servers},
                                   {"allow_browser_signaling", config.signaling.allow_browser_signaling}}},
      {"turn", Value::Object{{"uri", config.turn.uri},
                              {"username", config.turn.username},
                              {"password", config.turn.password},
                              {"credential_mode", config.turn.credential_mode}}},
      {"runtime_isolation", Value::Object{{"provider", config.runtime_isolation.provider},
                                           {"workspace_root", config.runtime_isolation.workspace_root},
                                           {"enable_lxc", config.runtime_isolation.enable_lxc},
                                           {"lxc_template", config.runtime_isolation.lxc_template}}},
      {"services", Value::Object{{"ipc_url", config.services.ipc_url},
                                  {"builtin_service_dir", config.services.builtin_service_dir},
                                  {"generated_service_dir", config.services.generated_service_dir},
                                  {"enable_webhooks", config.services.enable_webhooks}}},
      {"frontend_bridge", Value::Object{{"enabled", config.frontend_bridge.enabled},
                                         {"asset_root", config.frontend_bridge.asset_root},
                                         {"bridge_endpoint", config.frontend_bridge.bridge_endpoint},
                                         {"allow_wasm_extensions", config.frontend_bridge.allow_wasm_extensions},
                                         {"voice_transport", config.frontend_bridge.voice_transport}}},
      {"voice", Value::Object{{"preferred_input_device", config.voice.preferred_input_device},
                               {"preferred_output_device", config.voice.preferred_output_device},
                               {"preferred_capture_sample_rate", config.voice.preferred_capture_sample_rate},
                               {"preferred_playback_sample_rate", config.voice.preferred_playback_sample_rate},
                               {"preferred_channels", config.voice.preferred_channels},
                               {"frames_per_buffer", config.voice.frames_per_buffer},
                               {"playout_buffer_frames", config.voice.playout_buffer_frames},
                               {"max_playout_buffer_frames", config.voice.max_playout_buffer_frames},
                               {"enable_noise_suppression", config.voice.enable_noise_suppression},
                               {"enable_metrics", config.voice.enable_metrics}}}};
}

std::string SerializeAppConfig(const AppConfig& config) { return util::json::Serialize(AppConfigToJson(config)); }

}  // namespace daffy::config
