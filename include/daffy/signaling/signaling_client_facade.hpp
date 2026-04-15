#pragma once

#include <functional>
#include <memory>

#include "daffy/core/error.hpp"
#include "daffy/signaling/native_signaling_client.hpp"

namespace daffy::signaling {

class SignalingClientFacade {
 public:
  virtual ~SignalingClientFacade() = default;

  virtual core::Status Start() = 0;
  virtual core::Status Stop() = 0;
  virtual core::Status Send(const Message& message) = 0;

  virtual void SetMessageCallback(NativeSignalingClient::MessageCallback callback) = 0;
  virtual void SetStateChangeCallback(NativeSignalingClient::StateChangeCallback callback) = 0;

  [[nodiscard]] virtual bool IsStarted() const = 0;
  [[nodiscard]] virtual NativeSignalingClientStateSnapshot state() const = 0;
  [[nodiscard]] virtual NativeSignalingClientTelemetry telemetry() const = 0;
  [[nodiscard]] virtual NativeSignalingIceBootstrap bootstrap() const = 0;
};

using SignalingClientFactory =
    std::function<core::Result<std::unique_ptr<SignalingClientFacade>>(const NativeSignalingClientConfig& config)>;

core::Result<std::unique_ptr<SignalingClientFacade>> CreateDefaultSignalingClientFacade(
    const NativeSignalingClientConfig& config);

}  // namespace daffy::signaling
