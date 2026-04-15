#include "daffy/services/service_metadata.hpp"

namespace daffy::services {

util::json::Value ServiceMetadataToJson(const ServiceMetadata& metadata) {
  util::json::Value::Array protocols;
  for (const auto& protocol : metadata.protocols) {
    protocols.emplace_back(protocol);
  }

  return util::json::Value::Object{{"name", metadata.name},
                                   {"version", metadata.version},
                                   {"description", metadata.description},
                                   {"entrypoint", metadata.entrypoint},
                                   {"protocols", protocols},
                                   {"enabled", metadata.enabled}};
}

core::Result<ServiceMetadata> ParseServiceMetadata(const util::json::Value& value) {
  if (!value.IsObject()) {
    return core::Error{core::ErrorCode::kParseError, "Service metadata must be a JSON object"};
  }

  const auto* name = value.Find("name");
  const auto* version = value.Find("version");
  const auto* description = value.Find("description");
  const auto* entrypoint = value.Find("entrypoint");
  if (name == nullptr || version == nullptr || description == nullptr || entrypoint == nullptr ||
      !name->IsString() || !version->IsString() || !description->IsString() || !entrypoint->IsString()) {
    return core::Error{core::ErrorCode::kParseError, "Service metadata is missing required string fields"};
  }

  ServiceMetadata metadata;
  metadata.name = name->AsString();
  metadata.version = version->AsString();
  metadata.description = description->AsString();
  metadata.entrypoint = entrypoint->AsString();

  if (const auto* protocols = value.Find("protocols"); protocols != nullptr && protocols->IsArray()) {
    for (const auto& entry : protocols->AsArray()) {
      if (entry.IsString()) {
        metadata.protocols.push_back(entry.AsString());
      }
    }
  }
  if (const auto* enabled = value.Find("enabled"); enabled != nullptr && enabled->IsBool()) {
    metadata.enabled = enabled->AsBool();
  }

  return metadata;
}

}  // namespace daffy::services
