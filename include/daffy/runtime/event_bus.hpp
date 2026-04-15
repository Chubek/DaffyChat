#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "daffy/core/error.hpp"
#include "daffy/util/json.hpp"

namespace daffy::runtime {

struct EventEnvelope {
  std::string topic;
  std::string name;
  std::string emitted_at;
  util::json::Value payload;
};

util::json::Value EventEnvelopeToJson(const EventEnvelope& event);

class EventBus {
 public:
  using SubscriptionId = std::size_t;
  using Handler = std::function<void(const EventEnvelope&)>;

  virtual ~EventBus() = default;
  virtual SubscriptionId Subscribe(std::string topic, Handler handler) = 0;
  virtual void Unsubscribe(SubscriptionId subscription_id) = 0;
  virtual std::size_t Publish(const EventEnvelope& event) = 0;
};

class InMemoryEventBus final : public EventBus {
 public:
  SubscriptionId Subscribe(std::string topic, Handler handler) override;
  void Unsubscribe(SubscriptionId subscription_id) override;
  std::size_t Publish(const EventEnvelope& event) override;

 private:
  struct Subscription {
    std::string topic;
    Handler handler;
  };

  SubscriptionId next_subscription_id_{1};
  std::unordered_map<SubscriptionId, Subscription> subscriptions_;
};

}  // namespace daffy::runtime
