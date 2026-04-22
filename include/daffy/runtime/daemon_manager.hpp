#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/ipc/nng_transport.hpp"
#include "daffy/services/service_metadata.hpp"

namespace daffy::runtime {

struct ManagedService {
  services::ServiceMetadata metadata;
  std::string service_url;
  std::int64_t pid{0};
  std::string state{"running"};
  bool auto_restart{false};
  std::int64_t restart_count{0};
  std::int64_t last_exit_status{0};
  std::chrono::system_clock::time_point last_restart_attempt{};
};

util::json::Value ManagedServiceToJson(const ManagedService& service);
core::Result<ManagedService> ParseManagedService(const util::json::Value& value);

class DaemonManager {
 public:
  explicit DaemonManager(std::string run_directory = "/tmp/daffychat");

  core::Status RegisterService(ManagedService service);
  core::Status UnregisterService(std::string_view name);
  core::Status UpdateServiceState(std::string_view name, std::string state);
  core::Status StartService(std::string_view name);
  core::Status StopService(std::string_view name, bool force = false);
  core::Status ReconcileServices();
  core::Result<ManagedService> FindService(std::string_view name) const;
  std::vector<ManagedService> ListServices() const;

  core::Result<ipc::MessageEnvelope> BrokerRequest(std::string_view service_name,
                                                   const ipc::MessageEnvelope& request) const;
  core::Status BindControlPlane(ipc::NngRequestReplyTransport& transport, std::string url);
  [[nodiscard]] std::string StateFilePath() const;

 private:
  core::Status LoadPersistedState();
  core::Result<ipc::MessageEnvelope> HandleControlMessage(const ipc::MessageEnvelope& request);
  core::Status PersistPidFile(const ManagedService& service) const;
  core::Status RemovePidFile(std::string_view service_name) const;
  core::Status PersistState() const;
  core::Status PersistStateLocked() const;
  core::Result<std::string> ResolveExecutablePath(std::string_view service_name) const;
  core::Result<ManagedService> FindServiceLocked(std::string_view name) const;
  core::Status LaunchServiceLocked(ManagedService& service);
  core::Status ReconcileServiceLocked(ManagedService& service);

  std::string run_directory_;
  mutable std::mutex mutex_;
  std::unordered_map<std::string, ManagedService> services_;
  mutable ipc::NngRequestReplyTransport transport_;
};

}  // namespace daffy::runtime
