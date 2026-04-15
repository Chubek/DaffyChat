#include "daffy/signaling/signaling_client_facade.hpp"

#include <utility>

namespace daffy::signaling {
namespace {

class NativeSignalingClientFacade final : public SignalingClientFacade {
 public:
  explicit NativeSignalingClientFacade(NativeSignalingClient client) : client_(std::move(client)) {}

  core::Status Start() override { return client_.Start(); }
  core::Status Stop() override { return client_.Stop(); }
  core::Status Send(const Message& message) override { return client_.Send(message); }

  void SetMessageCallback(NativeSignalingClient::MessageCallback callback) override {
    client_.SetMessageCallback(std::move(callback));
  }

  void SetStateChangeCallback(NativeSignalingClient::StateChangeCallback callback) override {
    client_.SetStateChangeCallback(std::move(callback));
  }

  [[nodiscard]] bool IsStarted() const override { return client_.IsStarted(); }
  [[nodiscard]] NativeSignalingClientStateSnapshot state() const override { return client_.state(); }
  [[nodiscard]] NativeSignalingClientTelemetry telemetry() const override { return client_.telemetry(); }
  [[nodiscard]] NativeSignalingIceBootstrap bootstrap() const override { return client_.bootstrap(); }

 private:
  NativeSignalingClient client_;
};

}  // namespace

core::Result<std::unique_ptr<SignalingClientFacade>> CreateDefaultSignalingClientFacade(
    const NativeSignalingClientConfig& config) {
  auto client = NativeSignalingClient::Create(config);
  if (!client.ok()) {
    return client.error();
  }
  std::unique_ptr<SignalingClientFacade> facade =
      std::make_unique<NativeSignalingClientFacade>(std::move(client.value()));
  return facade;
}

}  // namespace daffy::signaling
