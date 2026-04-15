#pragma once

#include <string>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/runtime/event_bus.hpp"
#include "daffy/util/json.hpp"

namespace daffy::web {

struct WebhookSubscription {
  std::string id;
  std::string topic;
  std::string url;
  bool enabled{true};
};

struct WebhookDelivery {
  std::string subscription_id;
  std::string target_url;
  runtime::EventEnvelope event;
  std::string delivered_at;
};

util::json::Value WebhookDeliveryToJson(const WebhookDelivery& delivery);

class WebhookDispatcher {
 public:
  virtual ~WebhookDispatcher() = default;
  virtual core::Result<WebhookDelivery> Dispatch(const WebhookSubscription& subscription,
                                                 const runtime::EventEnvelope& event) = 0;
};

class RecordingWebhookDispatcher final : public WebhookDispatcher {
 public:
  core::Result<WebhookDelivery> Dispatch(const WebhookSubscription& subscription,
                                         const runtime::EventEnvelope& event) override;

  const std::vector<WebhookDelivery>& deliveries() const;

 private:
  std::vector<WebhookDelivery> deliveries_;
};

}  // namespace daffy::web
