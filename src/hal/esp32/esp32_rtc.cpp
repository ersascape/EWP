#if defined(ARDUINO)

#include "hal/esp32/esp32_rtc.h"
#include "core/watch_clock.h"

namespace ersa {
namespace hal {

Esp32Rtc::Esp32Rtc(int sda, int scl)
    : sda_(sda), scl_(scl) {}

TimePoint Esp32Rtc::toTimePoint(const DateTime& dt) {
    return TimePoint(
        dt.year(), dt.month(), dt.day(),
        dt.hour(), dt.minute(), dt.second(),
        dt.dayOfTheWeek(), dt.unixtime()
    );
}

DateTime Esp32Rtc::toDateTime(const TimePoint& tp) {
    if (tp.epoch > 0) {
        return DateTime(tp.epoch);
    }
    return DateTime(tp.year, tp.month, tp.day, tp.hour, tp.minute, tp.second);
}

Result<void> Esp32Rtc::init() {
    WatchClock::begin();
    return Result<void>();
}

TimePoint Esp32Rtc::now() {
    return toTimePoint(WatchClock::now());
}

Result<void> Esp32Rtc::adjust(const TimePoint& time) {
    WatchClock::adjust(toDateTime(time));
    return Result<void>();
}

Result<void> Esp32Rtc::setEpoch(uint32_t epochSeconds) {
    WatchClock::setEpoch(epochSeconds);
    return Result<void>();
}

bool Esp32Rtc::isHealthy() const {
    return WatchClock::healthy();
}

DateTime Esp32Rtc::getRtcLibNow() {
    return WatchClock::now();
}

void Esp32Rtc::adjustRtcLib(const DateTime& time) {
    WatchClock::adjust(time);
}

} // namespace hal
} // namespace ersa

#endif // ARDUINO
