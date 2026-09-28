#pragma once

#if defined(ARDUINO)

#include "ersa/hal/rtc.h"
#include <RTClib.h>

namespace ersa {
namespace hal {

class Esp32Rtc : public IRtc {
public:
    Esp32Rtc(int sda = 6, int scl = 7);
    ~Esp32Rtc() override = default;

    Result<void> init() override;
    TimePoint now() override;
    Result<void> adjust(const TimePoint& time) override;
    Result<void> setEpoch(uint32_t epochSeconds) override;
    bool isHealthy() const override;

    DateTime getRtcLibNow();
    void adjustRtcLib(const DateTime& dt);

private:
    int sda_, scl_;

    static TimePoint toTimePoint(const DateTime& dt);
    static DateTime toDateTime(const TimePoint& tp);
};

} // namespace hal
} // namespace ersa

#endif // ARDUINO
