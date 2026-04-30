#include <cassert>
#include <string>

#include "daffy/config/app_config.hpp"
#include "daffy/core/build_info.hpp"

int main() {
  const auto config = daffy::config::DefaultAppConfig();
  const std::string summary = daffy::config::DescribeAppConfig(config);

  assert(!daffy::core::ProjectVersion().empty());
  assert(summary.find("server=") != std::string::npos);
  assert(summary.find("signaling=") != std::string::npos);
  assert(summary.find("voice_transport=browser-socketio") != std::string::npos);
  return 0;
}
