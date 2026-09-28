#include "ersa/events/event_bus.h"

namespace ersa {
namespace events {

EventBus::EventBus() {
    clear();
}

EventBus& EventBus::instance() {
    static EventBus s_bus;
    return s_bus;
}

void EventBus::clear() {
    for (size_t i = 0; i < MAX_SUBSCRIBERS; ++i) {
        subscribers_[i].active = false;
        subscribers_[i].id = 0;
        subscribers_[i].callback = nullptr;
        subscribers_[i].listener = nullptr;
    }
    queueHead_ = 0;
    queueTail_ = 0;
    queueCount_ = 0;
}

SubscriptionId EventBus::subscribe(EventType type, EventCallback callback, void* userData) {
    if (!callback) return 0;

    for (size_t i = 0; i < MAX_SUBSCRIBERS; ++i) {
        if (!subscribers_[i].active) {
            subscribers_[i].active = true;
            subscribers_[i].id = nextSubId_++;
            if (nextSubId_ == 0) nextSubId_ = 1;
            subscribers_[i].type = type;
            subscribers_[i].callback = callback;
            subscribers_[i].listener = nullptr;
            subscribers_[i].userData = userData;
            return subscribers_[i].id;
        }
    }
    return 0;
}

SubscriptionId EventBus::subscribe(EventType type, IEventListener* listener) {
    if (!listener) return 0;

    for (size_t i = 0; i < MAX_SUBSCRIBERS; ++i) {
        if (!subscribers_[i].active) {
            subscribers_[i].active = true;
            subscribers_[i].id = nextSubId_++;
            if (nextSubId_ == 0) nextSubId_ = 1;
            subscribers_[i].type = type;
            subscribers_[i].callback = nullptr;
            subscribers_[i].listener = listener;
            subscribers_[i].userData = nullptr;
            return subscribers_[i].id;
        }
    }
    return 0;
}

bool EventBus::unsubscribe(SubscriptionId id) {
    if (id == 0) return false;
    for (size_t i = 0; i < MAX_SUBSCRIBERS; ++i) {
        if (subscribers_[i].active && subscribers_[i].id == id) {
            subscribers_[i].active = false;
            subscribers_[i].callback = nullptr;
            subscribers_[i].listener = nullptr;
            return true;
        }
    }
    return false;
}

void EventBus::publish(const Event& event) {
    for (size_t i = 0; i < MAX_SUBSCRIBERS; ++i) {
        if (subscribers_[i].active) {
            if (subscribers_[i].type == EventType::None ||
                subscribers_[i].type == event.type) {
                if (subscribers_[i].callback) {
                    subscribers_[i].callback(event, subscribers_[i].userData);
                } else if (subscribers_[i].listener) {
                    subscribers_[i].listener->onEvent(event);
                }
            }
        }
    }
}

bool EventBus::post(const Event& event) {
    if (queueCount_ >= MAX_QUEUE) {
        return false;
    }
    queue_[queueHead_] = event;
    queueHead_ = (queueHead_ + 1) % MAX_QUEUE;
    queueCount_++;
    return true;
}

size_t EventBus::dispatchQueue() {
    size_t processed = 0;
    while (queueCount_ > 0) {
        Event e = queue_[queueTail_];
        queueTail_ = (queueTail_ + 1) % MAX_QUEUE;
        queueCount_--;
        publish(e);
        processed++;
    }
    return processed;
}

} // namespace events
} // namespace ersa
