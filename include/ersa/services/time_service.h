#pragma once

#include "ersa/common/types.h"
#include "ersa/hal/rtc.h"
#include "ersa/events/event_bus.h"
#include <stdint.h>

namespace ersa {
namespace services {

class TimeService {
public:
    explicit TimeService(hal::IRtc& rtc, events::EventBus& bus = events::EventBus::instance());

    Result<void> init();
    void tick(uint32_t currentUptimeMs);

    hal::TimePoint now();
    Result<void> setEpoch(uint32_t epochSeconds);
    bool submitTime(events::TimeSource source, uint32_t epochSeconds);
    Result<void> adjust(const hal::TimePoint& time);

    bool isRtcHealthy() const;
    uint32_t lastMinute() const;

    void setTimezoneOffset(int16_t offsetMinutes);
    int16_t getTimezoneOffset() const;

    static TimeService& instance();
    static void setInstance(TimeService* instance);

private:
    hal::IRtc& rtc_;
    events::EventBus& bus_;
    int16_t tzOffsetMin_{0};
    uint32_t lastMinute_{UINT32_MAX};
    uint32_t lastPollMs_{0};
    events::TimeSource selectedSource_{events::TimeSource::Unknown};
    events::SubscriptionId timeSyncSubscription_{0};
    static void onTimeSyncEvent(const events::Event& event, void* userData);
};

} // namespace services
} // namespace ersa
