#pragma once

#include <string>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/util/json.hpp"

namespace daffy::services {

struct ServiceMetadata {
  std::string name;
  std::string version;
  std::string description;
  std::string entrypoint;
  std::vector<std::string> protocols;
  bool enabled{true};
};

util::json::Value ServiceMetadataToJson(const ServiceMetadata& metadata);
core::Result<ServiceMetadata> ParseServiceMetadata(const util::json::Value& value);

}  // namespace daffy::services
