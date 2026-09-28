#pragma once

#include "event.h"
#include "ersa/common/types.h"
#include <stdint.h>
#include <stddef.h>

namespace ersa {
namespace events {

using SubscriptionId = uint16_t;
using EventCallback = void (*)(const Event& event, void* userData);

struct Subscription {
    SubscriptionId id{0};
    EventCategory category{EventCategory::None}; // None = match all
    EventCallback callback{nullptr};
    void* userData{nullptr};
    bool active{false};
};

class EventBus {
public:
    static constexpr size_t MAX_SUBSCRIBERS = 16;
    static constexpr size_t MAX_QUEUE = 32;

    EventBus();

    // Subscribe to events. Pass category == EventCategory::None to listen to all events.
    SubscriptionId subscribe(EventCategory category, EventCallback callback, void* userData = nullptr);
    bool unsubscribe(SubscriptionId id);

    // Synchronous immediate dispatch to matching subscribers
    void publish(const Event& event);

    // Asynchronous queue: posts event to ring buffer to be processed by dispatchQueue()
    bool post(const Event& event);

    // Dispatches all queued events
    size_t dispatchQueue();

    // Clear queue and subscriptions
    void clear();

    // Global default bus instance accessor
    static EventBus& instance();

private:
    Subscription subscribers_[MAX_SUBSCRIBERS];
    SubscriptionId nextSubId_{1};

    Event queue_[MAX_QUEUE];
    size_t queueHead_{0};
    size_t queueTail_{0};
    size_t queueCount_{0};
};

} // namespace events
} // namespace ersa
