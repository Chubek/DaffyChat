#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/services/service_metadata.hpp"

namespace daffy::services {

class ServiceRegistry {
 public:
  core::Status Register(ServiceMetadata metadata);
  core::Result<ServiceMetadata> Find(std::string_view name) const;
  std::vector<ServiceMetadata> List() const;

 private:
  std::unordered_map<std::string, ServiceMetadata> services_;
};

}  // namespace daffy::services
