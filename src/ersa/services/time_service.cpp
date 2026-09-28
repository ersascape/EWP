#include "ersa/services/time_service.h"

namespace ersa {
namespace services {

static TimeService* s_timeServiceInstance = nullptr;

TimeService& TimeService::instance() {
    return *s_timeServiceInstance;
}

void TimeService::setInstance(TimeService* instance) {
    s_timeServiceInstance = instance;
}

TimeService::TimeService(hal::IRtc& rtc, events::EventBus& bus)
    : rtc_(rtc), bus_(bus) {}

Result<void> TimeService::init() {
    Result<void> res = rtc_.init();
    hal::TimePoint current = rtc_.now();
    lastMinute_ = current.epoch / 60;
    return res;
}

void TimeService::tick(uint32_t currentUptimeMs) {
    // Poll RTC periodically (every 1000ms)
    if (currentUptimeMs - lastPollMs_ >= 1000) {
        lastPollMs_ = currentUptimeMs;
        hal::TimePoint tp = rtc_.now();
        uint32_t currentMin = tp.epoch / 60;

        if (currentMin != lastMinute_) {
            lastMinute_ = currentMin;

            events::TimePayload td;
            td.epoch = tp.epoch;
            td.year = tp.year;
            td.month = tp.month;
            td.day = tp.day;
            td.hour = tp.hour;
            td.minute = tp.minute;
            td.second = tp.second;

            bus_.publish(events::Event::createMinuteTick(td, currentUptimeMs));
        }
    }
}

hal::TimePoint TimeService::now() {
    return rtc_.now();
}

Result<void> TimeService::setEpoch(uint32_t epochSeconds) {
    Result<void> res = rtc_.setEpoch(epochSeconds);
    if (res.isOk()) {
        hal::TimePoint tp = rtc_.now();
        lastMinute_ = tp.epoch / 60;
    }
    return res;
}

Result<void> TimeService::adjust(const hal::TimePoint& time) {
    Result<void> res = rtc_.adjust(time);
    if (res.isOk()) {
        lastMinute_ = time.epoch / 60;
    }
    return res;
}

bool TimeService::isRtcHealthy() const {
    return rtc_.isHealthy();
}

uint32_t TimeService::lastMinute() const {
    return lastMinute_;
}

void TimeService::setTimezoneOffset(int16_t offsetMinutes) {
    tzOffsetMin_ = offsetMinutes;
}

int16_t TimeService::getTimezoneOffset() const {
    return tzOffsetMin_;
}

} // namespace services
} // namespace ersa
