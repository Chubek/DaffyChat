#include "daffy/services/service_registry.hpp"

namespace daffy::services {

core::Status ServiceRegistry::Register(ServiceMetadata metadata) {
  if (metadata.name.empty()) {
    return core::Error{core::ErrorCode::kInvalidArgument, "Service name cannot be empty"};
  }
  const auto [it, inserted] = services_.emplace(metadata.name, std::move(metadata));
  if (!inserted) {
    return core::Error{core::ErrorCode::kAlreadyExists, "Service already registered: " + it->first};
  }
  return core::OkStatus();
}

core::Result<ServiceMetadata> ServiceRegistry::Find(std::string_view name) const {
  const auto it = services_.find(std::string(name));
  if (it == services_.end()) {
    return core::Error{core::ErrorCode::kNotFound, "Unknown service: " + std::string(name)};
  }
  return it->second;
}

std::vector<ServiceMetadata> ServiceRegistry::List() const {
  std::vector<ServiceMetadata> services;
  services.reserve(services_.size());
  for (const auto& [name, metadata] : services_) {
    static_cast<void>(name);
    services.push_back(metadata);
  }
  return services;
}

}  // namespace daffy::services
