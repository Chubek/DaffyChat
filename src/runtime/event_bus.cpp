#include "daffy/runtime/event_bus.hpp"

namespace daffy::runtime {

util::json::Value EventEnvelopeToJson(const EventEnvelope& event) {
  return util::json::Value::Object{{"topic", event.topic},
                                   {"name", event.name},
                                   {"emitted_at", event.emitted_at},
                                   {"payload", event.payload}};
}

EventBus::SubscriptionId InMemoryEventBus::Subscribe(std::string topic, Handler handler) {
  const auto subscription_id = next_subscription_id_++;
  subscriptions_.emplace(subscription_id, Subscription{std::move(topic), std::move(handler)});
  return subscription_id;
}

void InMemoryEventBus::Unsubscribe(const SubscriptionId subscription_id) { subscriptions_.erase(subscription_id); }

std::size_t InMemoryEventBus::Publish(const EventEnvelope& event) {
  std::size_t delivered = 0;
  for (const auto& [subscription_id, subscription] : subscriptions_) {
    static_cast<void>(subscription_id);
    if (subscription.topic == event.topic) {
      subscription.handler(event);
      ++delivered;
    }
  }
  return delivered;
}

}  // namespace daffy::runtime
