#include "daffy/web/webhook.hpp"

#include "daffy/core/time.hpp"

namespace daffy::web {

util::json::Value WebhookDeliveryToJson(const WebhookDelivery& delivery) {
  return util::json::Value::Object{{"subscription_id", delivery.subscription_id},
                                   {"target_url", delivery.target_url},
                                   {"delivered_at", delivery.delivered_at},
                                   {"event", runtime::EventEnvelopeToJson(delivery.event)}};
}

core::Result<WebhookDelivery> RecordingWebhookDispatcher::Dispatch(const WebhookSubscription& subscription,
                                                                   const runtime::EventEnvelope& event) {
  if (!subscription.enabled) {
    return core::Error{core::ErrorCode::kUnavailable, "Webhook subscription is disabled"};
  }

  WebhookDelivery delivery;
  delivery.subscription_id = subscription.id;
  delivery.target_url = subscription.url;
  delivery.event = event;
  delivery.delivered_at = daffy::core::UtcNowIso8601();
  deliveries_.push_back(delivery);
  return delivery;
}

const std::vector<WebhookDelivery>& RecordingWebhookDispatcher::deliveries() const { return deliveries_; }

}  // namespace daffy::web
