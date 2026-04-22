#include "daffy/runtime/daemon_manager.hpp"

#include <chrono>
#include <cerrno>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>

#include "daffy/services/echo_service.hpp"
#include "daffy/services/event_bridge_service.hpp"
#include "daffy/services/health_service.hpp"
#include "daffy/services/room_ops_service.hpp"
#include "daffy/services/room_state_service.hpp"

namespace daffy::runtime {

namespace {

constexpr char kControlTopic[] = "daffydmd.control";
constexpr char kRequestType[] = "request";
constexpr char kReplyType[] = "reply";
constexpr auto kAutoRestartBackoff = std::chrono::milliseconds(250);

core::Result<ipc::MessageEnvelope> ProbeService(ipc::NngRequestReplyTransport& transport,
                                                const ManagedService& service) {
  if (service.metadata.name == "echo") {
    return transport.Request(service.service_url,
                             ipc::MessageEnvelope{"service.echo",
                                                  "request",
                                                  services::EchoRequestToJson({"daemon-startup-probe", "daffydmd"})});
  }

  if (service.metadata.name == "roomops") {
    return transport.Request(service.service_url,
                             ipc::MessageEnvelope{
                                 "service.roomops",
                                 "request",
                                 util::json::Value::Object{{"rpc", "Join"}, {"user", "daemon-startup-probe"}},
                             });
  }

  if (service.metadata.name == "health") {
    return transport.Request(service.service_url,
                             ipc::MessageEnvelope{
                                 "service.health",
                                 "request",
                                 util::json::Value::Object{{"rpc", "Ping"}},
                             });
  }

  if (service.metadata.name == "roomstate") {
    return transport.Request(service.service_url,
                             ipc::MessageEnvelope{
                                 "service.roomstate",
                                 "request",
                                 util::json::Value::Object{{"rpc", "ListRooms"}},
                             });
  }

  if (service.metadata.name == "eventbridge") {
    return transport.Request(service.service_url,
                             ipc::MessageEnvelope{
                                 "service.eventbridge",
                                 "request",
                                 util::json::Value::Object{{"rpc", "Status"}},
                             });
  }

  return core::Error{core::ErrorCode::kUnavailable,
                     "No daemon-manager readiness probe is defined for service: " + service.metadata.name};
}

std::string PidFilePath(const std::string& run_directory, std::string_view service_name) {
  return run_directory + "/" + std::string(service_name) + ".pid";
}

std::string StateFilePath(const std::string& run_directory) {
  return run_directory + "/services.json";
}

double RestartAttemptToEpochSeconds(const std::chrono::system_clock::time_point time_point) {
  if (time_point == std::chrono::system_clock::time_point{}) {
    return 0.0;
  }

  const auto epoch = time_point.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::duration<double>>(epoch).count();
}

std::chrono::system_clock::time_point RestartAttemptFromEpochSeconds(const double epoch_seconds) {
  if (epoch_seconds <= 0.0) {
    return {};
  }

  return std::chrono::system_clock::time_point{
      std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::duration<double>(epoch_seconds))};
}

bool ProcessExists(const std::int64_t pid) {
  if (pid <= 0) {
    return false;
  }
  return kill(static_cast<pid_t>(pid), 0) == 0;
}

core::Status WriteServicePidFile(const std::string& run_directory, const ManagedService& service) {
  std::filesystem::create_directories(run_directory);
  std::ofstream pid_file(PidFilePath(run_directory, service.metadata.name));
  if (!pid_file) {
    return core::Error{core::ErrorCode::kUnavailable, "Unable to write daemon manager pid file"};
  }
  pid_file << service.pid << "\n";
  return core::OkStatus();
}

}  // namespace

util::json::Value ManagedServiceToJson(const ManagedService& service) {
  return util::json::Value::Object{{"metadata", services::ServiceMetadataToJson(service.metadata)},
                                   {"service_url", service.service_url},
                                   {"pid", static_cast<double>(service.pid)},
                                   {"state", service.state},
                                   {"auto_restart", service.auto_restart},
                                   {"restart_count", static_cast<double>(service.restart_count)},
                                   {"last_exit_status", static_cast<double>(service.last_exit_status)},
                                   {"last_restart_attempt", RestartAttemptToEpochSeconds(service.last_restart_attempt)}};
}

core::Result<ManagedService> ParseManagedService(const util::json::Value& value) {
  if (!value.IsObject()) {
    return core::Error{core::ErrorCode::kParseError, "Managed service must be a JSON object"};
  }

  const auto* metadata_value = value.Find("metadata");
  const auto* service_url = value.Find("service_url");
  const auto* pid = value.Find("pid");
  const auto* state = value.Find("state");
  if (metadata_value == nullptr || service_url == nullptr || pid == nullptr || state == nullptr ||
      !service_url->IsString() || !pid->IsNumber() || !state->IsString()) {
    return core::Error{core::ErrorCode::kParseError, "Managed service is missing required fields"};
  }

  auto metadata = services::ParseServiceMetadata(*metadata_value);
  if (!metadata.ok()) {
    return metadata.error();
  }

  ManagedService service;
  service.metadata = std::move(metadata.value());
  service.service_url = service_url->AsString();
  service.pid = static_cast<std::int64_t>(pid->AsNumber());
  service.state = state->AsString();
  if (const auto* auto_restart = value.Find("auto_restart"); auto_restart != nullptr && auto_restart->IsBool()) {
    service.auto_restart = auto_restart->AsBool();
  }
  if (const auto* restart_count = value.Find("restart_count"); restart_count != nullptr && restart_count->IsNumber()) {
    service.restart_count = static_cast<std::int64_t>(restart_count->AsNumber());
  }
  if (const auto* last_exit_status = value.Find("last_exit_status");
      last_exit_status != nullptr && last_exit_status->IsNumber()) {
    service.last_exit_status = static_cast<std::int64_t>(last_exit_status->AsNumber());
  }
  if (const auto* last_restart_attempt = value.Find("last_restart_attempt");
      last_restart_attempt != nullptr && last_restart_attempt->IsNumber()) {
    service.last_restart_attempt = RestartAttemptFromEpochSeconds(last_restart_attempt->AsNumber());
  }
  return service;
}

DaemonManager::DaemonManager(std::string run_directory) : run_directory_(std::move(run_directory)) {
  std::filesystem::create_directories(run_directory_);
  static_cast<void>(LoadPersistedState());
}

core::Status DaemonManager::RegisterService(ManagedService service) {
  if (service.metadata.name.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Managed service name cannot be empty"};
  }
  if (service.service_url.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Managed service URL cannot be empty"};
  }
  auto valid_url = ipc::ValidateNngUrl(service.service_url);
  if (!valid_url.ok()) {
    return valid_url;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  auto it = services_.find(service.metadata.name);
  if (it != services_.end()) {
    auto reconcile = ReconcileServiceLocked(it->second);
    if (!reconcile.ok()) {
      return reconcile;
    }
    it->second = std::move(service);
    if (it->second.pid > 0 && it->second.state == "running") {
      auto pid_status = PersistPidFile(it->second);
      if (!pid_status.ok()) {
        return pid_status;
      }
    } else {
      auto remove_status = RemovePidFile(it->second.metadata.name);
      if (!remove_status.ok()) {
        return remove_status;
      }
    }
    return PersistStateLocked();
  }
  auto [inserted, was_inserted] = services_.emplace(service.metadata.name, std::move(service));
  static_cast<void>(was_inserted);
  auto reconcile = ReconcileServiceLocked(inserted->second);
  if (!reconcile.ok()) {
    return reconcile;
  }
  if (inserted->second.pid > 0 && inserted->second.state == "running") {
    auto pid_status = PersistPidFile(inserted->second);
    if (!pid_status.ok()) {
      return pid_status;
    }
  }
  return PersistStateLocked();
}

core::Status DaemonManager::UnregisterService(std::string_view name) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = services_.find(std::string(name));
    if (it == services_.end()) {
      return core::Error{core::ErrorCode::kNotFound, "Unknown managed service: " + std::string(name)};
    }
  }

  auto stop_status = StopService(name, true);
  if (!stop_status.ok() && stop_status.error().code != core::ErrorCode::kNotFound) {
    return stop_status;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  services_.erase(std::string(name));
  return PersistStateLocked();
}

core::Status DaemonManager::UpdateServiceState(std::string_view name, std::string state) {
  if (state.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Managed service state cannot be empty"};
  }

  std::lock_guard<std::mutex> lock(mutex_);
  const auto it = services_.find(std::string(name));
  if (it == services_.end()) {
    return core::Error{core::ErrorCode::kNotFound, "Unknown managed service: " + std::string(name)};
  }

  it->second.state = std::move(state);
  return PersistStateLocked();
}

core::Result<ManagedService> DaemonManager::FindService(std::string_view name) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto service = FindServiceLocked(name);
  if (!service.ok()) {
    return service.error();
  }
  return service.value();
}

std::vector<ManagedService> DaemonManager::ListServices() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<ManagedService> services;
  services.reserve(services_.size());
  for (const auto& [name, service] : services_) {
    static_cast<void>(name);
    services.push_back(service);
  }
  return services;
}

core::Result<ipc::MessageEnvelope> DaemonManager::BrokerRequest(std::string_view service_name,
                                                                const ipc::MessageEnvelope& request) const {
  auto service = FindService(service_name);
  if (!service.ok()) {
    return service.error();
  }
  if (service.value().state == "stopped") {
    return core::Error{core::ErrorCode::kStateError,
                       "Managed service is not running: " + std::string(service_name)};
  }
  return transport_.Request(service.value().service_url, request);
}

core::Status DaemonManager::BindControlPlane(ipc::NngRequestReplyTransport& transport, std::string url) {
  auto status = transport.Bind(std::move(url),
                               [this](const ipc::MessageEnvelope& request) { return HandleControlMessage(request); });
  if (!status.ok()) {
    return status;
  }

  // The control plane is immediately queried in tests and bootstrap flows.
  std::this_thread::sleep_for(std::chrono::milliseconds(75));
  return core::OkStatus();
}

std::string DaemonManager::StateFilePath() const { return runtime::StateFilePath(run_directory_); }

core::Status DaemonManager::LoadPersistedState() {
  const auto parsed = util::json::ParseFile(StateFilePath());
  if (!parsed.ok()) {
    if (parsed.error().code == core::ErrorCode::kIoError) {
      return core::OkStatus();
    }
    return parsed.error();
  }
  if (!parsed.value().IsObject()) {
    return core::Error{core::ErrorCode::kParseError, "Daemon manager state file must be a JSON object"};
  }

  const auto* services = parsed.value().Find("services");
  if (services == nullptr) {
    return core::OkStatus();
  }
  if (!services->IsArray()) {
    return core::Error{core::ErrorCode::kParseError, "Daemon manager state `services` field must be an array"};
  }

  std::lock_guard<std::mutex> lock(mutex_);
  services_.clear();
  for (const auto& service_value : services->AsArray()) {
    auto parsed_service = ParseManagedService(service_value);
    if (!parsed_service.ok()) {
      return parsed_service.error();
    }
    auto reconcile = ReconcileServiceLocked(parsed_service.value());
    if (!reconcile.ok()) {
      return reconcile;
    }
    services_.emplace(parsed_service.value().metadata.name, parsed_service.value());
  }
  return core::OkStatus();
}

core::Status DaemonManager::StartService(std::string_view name) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto service = FindServiceLocked(name);
  if (!service.ok()) {
    return service.error();
  }

  auto& managed = services_.at(std::string(name));
  auto reconcile = ReconcileServiceLocked(managed);
  if (!reconcile.ok()) {
    return reconcile;
  }
  if (managed.state == "running" && ProcessExists(managed.pid)) {
    return core::OkStatus();
  }

  return LaunchServiceLocked(managed);
}

core::Status DaemonManager::LaunchServiceLocked(ManagedService& managed) {
  auto executable = ResolveExecutablePath(managed.metadata.name);
  if (!executable.ok()) {
    return executable.error();
  }

  const pid_t child = fork();
  if (child < 0) {
    return core::Error{core::ErrorCode::kUnavailable, "Failed to fork child service"};
  }
  if (child == 0) {
    execl(executable.value().c_str(), executable.value().c_str(), "--url", managed.service_url.c_str(), nullptr);
    _exit(127);
  }

  managed.pid = child;
  managed.state = "starting";
  managed.last_restart_attempt = std::chrono::system_clock::now();
  auto pid_status = PersistPidFile(managed);
  if (!pid_status.ok()) {
    return pid_status;
  }
  auto state_status = PersistStateLocked();
  if (!state_status.ok()) {
    return state_status;
  }

  for (int attempt = 0; attempt < 100; ++attempt) {
    if (!ProcessExists(managed.pid)) {
      int child_status = 0;
      waitpid(child, &child_status, WNOHANG);
      managed.pid = 0;
      managed.state = "stopped";
      managed.last_exit_status = child_status;
      auto remove_status = RemovePidFile(managed.metadata.name);
      if (!remove_status.ok()) {
        return remove_status;
      }
      return PersistStateLocked();
    }

    auto ping = ProbeService(transport_, managed);
    if (ping.ok()) {
      managed.state = "running";
      return PersistStateLocked();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }

  managed.state = "starting";
  auto persist_status = PersistStateLocked();
  if (!persist_status.ok()) {
    return persist_status;
  }
  return core::Error{core::ErrorCode::kUnavailable,
                     "Managed service did not become ready: " + managed.metadata.name};
}

core::Status DaemonManager::StopService(std::string_view name, bool force) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto service = FindServiceLocked(name);
  if (!service.ok()) {
    return service.error();
  }

  auto& managed = services_.at(std::string(name));
  auto reconcile = ReconcileServiceLocked(managed);
  if (!reconcile.ok()) {
    return reconcile;
  }

  if (managed.pid <= 0) {
    managed.state = "stopped";
    auto remove_status = RemovePidFile(managed.metadata.name);
    if (!remove_status.ok()) {
      return remove_status;
    }
    return PersistStateLocked();
  }

  const int signal_number = force ? SIGKILL : SIGTERM;
  if (kill(static_cast<pid_t>(managed.pid), signal_number) != 0 && errno != ESRCH) {
    return core::Error{core::ErrorCode::kUnavailable, "Failed to signal managed service"};
  }

  for (int attempt = 0; attempt < 100; ++attempt) {
    int status = 0;
    const pid_t waited = waitpid(static_cast<pid_t>(managed.pid), &status, WNOHANG);
    if (waited == static_cast<pid_t>(managed.pid) || !ProcessExists(managed.pid)) {
      managed.pid = 0;
      managed.state = "stopped";
      managed.last_exit_status = status;
      auto remove_status = RemovePidFile(managed.metadata.name);
      if (!remove_status.ok()) {
        return remove_status;
      }
      return PersistStateLocked();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }

  if (!force) {
    if (kill(static_cast<pid_t>(managed.pid), SIGKILL) != 0 && errno != ESRCH) {
      return core::Error{core::ErrorCode::kUnavailable, "Failed to force-stop managed service"};
    }
    for (int attempt = 0; attempt < 100; ++attempt) {
      int status = 0;
      const pid_t waited = waitpid(static_cast<pid_t>(managed.pid), &status, WNOHANG);
      if (waited == static_cast<pid_t>(managed.pid) || !ProcessExists(managed.pid)) {
        managed.pid = 0;
        managed.state = "stopped";
        managed.last_exit_status = status;
        auto remove_status = RemovePidFile(managed.metadata.name);
        if (!remove_status.ok()) {
          return remove_status;
        }
        return PersistStateLocked();
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
  }
  return core::Error{core::ErrorCode::kUnavailable, "Timed out waiting for managed service to stop"};
}

core::Status DaemonManager::ReconcileServices() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto& [name, service] : services_) {
    static_cast<void>(name);
    auto status = ReconcileServiceLocked(service);
    if (!status.ok()) {
      return status;
    }
  }
  return PersistStateLocked();
}

core::Result<ipc::MessageEnvelope> DaemonManager::HandleControlMessage(const ipc::MessageEnvelope& request) {
  if (request.topic != kControlTopic || request.type != kRequestType) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Unexpected daemon manager control message"};
  }

  const auto* action = request.payload.Find("action");
  if (action == nullptr || !action->IsString()) {
    return core::Error{core::ErrorCode::kParseError, "Control request must include an action"};
  }

  if (action->AsString() == "list_services") {
    util::json::Value::Array services_json;
    for (const auto& service : ListServices()) {
      services_json.emplace_back(ManagedServiceToJson(service));
    }
    return ipc::MessageEnvelope{kControlTopic, kReplyType, util::json::Value::Object{{"services", services_json}}};
  }

  if (action->AsString() == "find_service") {
    const auto* service_name = request.payload.Find("service");
    if (service_name == nullptr || !service_name->IsString()) {
      return core::Error{core::ErrorCode::kParseError, "Find service requires a string `service` field"};
    }

    auto service = FindService(service_name->AsString());
    if (!service.ok()) {
      return service.error();
    }
    return ipc::MessageEnvelope{
        kControlTopic,
        kReplyType,
        util::json::Value::Object{{"service", ManagedServiceToJson(service.value())}},
    };
  }

  if (action->AsString() == "start_service") {
    const auto* service_name = request.payload.Find("service");
    if (service_name == nullptr || !service_name->IsString()) {
      return core::Error{core::ErrorCode::kParseError, "Start service requires a string `service` field"};
    }

    auto status = StartService(service_name->AsString());
    if (!status.ok()) {
      return status.error();
    }
    auto started = FindService(service_name->AsString());
    if (!started.ok()) {
      return started.error();
    }
    return ipc::MessageEnvelope{kControlTopic,
                                kReplyType,
                                util::json::Value::Object{{"service", ManagedServiceToJson(started.value())}}};
  }

  if (action->AsString() == "stop_service") {
    const auto* service_name = request.payload.Find("service");
    if (service_name == nullptr || !service_name->IsString()) {
      return core::Error{core::ErrorCode::kParseError, "Stop service requires a string `service` field"};
    }

    auto status = StopService(service_name->AsString());
    if (!status.ok()) {
      return status.error();
    }
    auto stopped = FindService(service_name->AsString());
    if (!stopped.ok()) {
      return stopped.error();
    }
    return ipc::MessageEnvelope{kControlTopic,
                                kReplyType,
                                util::json::Value::Object{{"service", ManagedServiceToJson(stopped.value())}}};
  }

  if (action->AsString() == "reconcile_services") {
    auto status = ReconcileServices();
    if (!status.ok()) {
      return status.error();
    }
    util::json::Value::Array services_json;
    for (const auto& service : ListServices()) {
      services_json.emplace_back(ManagedServiceToJson(service));
    }
    return ipc::MessageEnvelope{kControlTopic, kReplyType, util::json::Value::Object{{"services", services_json}}};
  }

  if (action->AsString() == "unregister_service") {
    const auto* service_name = request.payload.Find("service");
    if (service_name == nullptr || !service_name->IsString()) {
      return core::Error{core::ErrorCode::kParseError, "Unregister service requires a string `service` field"};
    }

    auto status = UnregisterService(service_name->AsString());
    if (!status.ok()) {
      return status.error();
    }
    return ipc::MessageEnvelope{
        kControlTopic,
        kReplyType,
        util::json::Value::Object{{"service", service_name->AsString()}, {"removed", true}},
    };
  }

  if (action->AsString() == "set_service_state") {
    const auto* service_name = request.payload.Find("service");
    const auto* state = request.payload.Find("state");
    if (service_name == nullptr || !service_name->IsString() || state == nullptr || !state->IsString()) {
      return core::Error{core::ErrorCode::kParseError, "Set service state requires string `service` and `state` fields"};
    }

    auto status = UpdateServiceState(service_name->AsString(), state->AsString());
    if (!status.ok()) {
      return status.error();
    }
    return ipc::MessageEnvelope{
        kControlTopic,
        kReplyType,
        util::json::Value::Object{{"service", service_name->AsString()}, {"state", state->AsString()}},
    };
  }

  if (action->AsString() == "set_service_restart_policy") {
    const auto* service_name = request.payload.Find("service");
    const auto* auto_restart = request.payload.Find("auto_restart");
    if (service_name == nullptr || !service_name->IsString() || auto_restart == nullptr || !auto_restart->IsBool()) {
      return core::Error{core::ErrorCode::kParseError,
                         "Set service restart policy requires `service` and bool `auto_restart` fields"};
    }

    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = services_.find(service_name->AsString());
    if (it == services_.end()) {
      return core::Error{core::ErrorCode::kNotFound, "Unknown managed service: " + service_name->AsString()};
    }
    it->second.auto_restart = auto_restart->AsBool();
    auto persist = PersistStateLocked();
    if (!persist.ok()) {
      return persist.error();
    }
    return ipc::MessageEnvelope{kControlTopic,
                                kReplyType,
                                util::json::Value::Object{{"service", ManagedServiceToJson(it->second)}}};
  }

  if (action->AsString() == "proxy_request") {
    const auto* service_name = request.payload.Find("service");
    const auto* proxied_request = request.payload.Find("request");
    if (service_name == nullptr || proxied_request == nullptr || !service_name->IsString()) {
      return core::Error{core::ErrorCode::kParseError, "Proxy request requires `service` and `request` fields"};
    }

    auto parsed_request = ipc::ParseMessageEnvelope(*proxied_request);
    if (!parsed_request.ok()) {
      return parsed_request.error();
    }

    auto reply = BrokerRequest(service_name->AsString(), parsed_request.value());
    if (!reply.ok()) {
      return reply.error();
    }

    return ipc::MessageEnvelope{
        kControlTopic,
        kReplyType,
        util::json::Value::Object{{"reply", ipc::MessageEnvelopeToJson(reply.value())}},
    };
  }

  return core::Error{core::ErrorCode::kInvalidArgument, "Unknown daemon manager action: " + action->AsString()};
}

core::Status DaemonManager::PersistPidFile(const ManagedService& service) const {
  return WriteServicePidFile(run_directory_, service);
}

core::Status DaemonManager::RemovePidFile(std::string_view service_name) const {
  std::error_code ec;
  std::filesystem::remove(PidFilePath(run_directory_, service_name), ec);
  if (ec) {
    return core::Error{core::ErrorCode::kIoError, "Unable to remove daemon manager pid file"};
  }
  return core::OkStatus();
}

core::Status DaemonManager::PersistState() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return PersistStateLocked();
}

core::Status DaemonManager::PersistStateLocked() const {
  util::json::Value::Array services_json;
  services_json.reserve(services_.size());
  for (const auto& [name, service] : services_) {
    static_cast<void>(name);
    services_json.emplace_back(ManagedServiceToJson(service));
  }

  std::ofstream state_file(StateFilePath());
  if (!state_file) {
    return core::Error{core::ErrorCode::kUnavailable, "Unable to write daemon manager state file"};
  }

  state_file << util::json::Serialize(util::json::Value::Object{{"services", services_json}});
  if (!state_file) {
    return core::Error{core::ErrorCode::kUnavailable, "Failed while writing daemon manager state file"};
  }
  return core::OkStatus();
}

core::Result<std::string> DaemonManager::ResolveExecutablePath(std::string_view service_name) const {
  const auto local_bin = std::filesystem::current_path() / "build" / ("daffy-" + std::string(service_name) + "-service");
  if (std::filesystem::exists(local_bin)) {
    return local_bin.string();
  }

  const auto cwd_bin = std::filesystem::current_path() / ("daffy-" + std::string(service_name) + "-service");
  if (std::filesystem::exists(cwd_bin)) {
    return cwd_bin.string();
  }

  return core::Error{core::ErrorCode::kNotFound,
                     "Unable to find executable for managed service: " + std::string(service_name)};
}

core::Result<ManagedService> DaemonManager::FindServiceLocked(std::string_view name) const {
  const auto it = services_.find(std::string(name));
  if (it == services_.end()) {
    return core::Error{core::ErrorCode::kNotFound, "Unknown managed service: " + std::string(name)};
  }
  return it->second;
}

core::Status DaemonManager::ReconcileServiceLocked(ManagedService& service) {
  if (service.pid <= 0) {
    service.pid = 0;
    if (service.state == "running" || service.state == "starting") {
      service.state = "stopped";
    }
    return RemovePidFile(service.metadata.name);
  }

  if (ProcessExists(service.pid)) {
    auto probe = ProbeService(transport_, service);
    if (probe.ok()) {
      if (service.state == "starting" || service.state.empty()) {
        service.state = "running";
      }
      return PersistPidFile(service);
    }

    // A live PID without a responding service endpoint should not be trusted after reload.
    // This catches stale pidfiles that happen to point at an unrelated process.
    if (service.state == "running" || service.state == "starting") {
      service.pid = 0;
      service.state = "stopped";
      auto remove_status = RemovePidFile(service.metadata.name);
      if (!remove_status.ok()) {
        return remove_status;
      }
      return core::OkStatus();
    }
    return PersistPidFile(service);
  }

  const bool should_restart = service.auto_restart && (service.state == "running" || service.state == "starting");
  bool child_was_supervised = false;
  if (service.pid > 0) {
    int status = 0;
    const auto waited = waitpid(static_cast<pid_t>(service.pid), &status, WNOHANG);
    if (waited == static_cast<pid_t>(service.pid)) {
      service.last_exit_status = status;
      child_was_supervised = true;
    }
  }
  service.pid = 0;
  service.state = "stopped";
  auto remove_status = RemovePidFile(service.metadata.name);
  if (!remove_status.ok()) {
    return remove_status;
  }

  if (!should_restart || !child_was_supervised) {
    return core::OkStatus();
  }

  const auto now = std::chrono::system_clock::now();
  if (service.last_restart_attempt != std::chrono::system_clock::time_point{} &&
      now - service.last_restart_attempt < kAutoRestartBackoff) {
    return core::OkStatus();
  }

  ++service.restart_count;
  return LaunchServiceLocked(service);
}

}  // namespace daffy::runtime
