#include "watch_clock.h"
#include "board_pins.h"
#include "debug_log.h"
#include <Wire.h>
#include <esp_timer.h>

namespace {
RTC_DS3231 rtc;
bool online = false, adjusted = false;
uint32_t epoch = 0, lastPoll = 0;
int64_t anchorUs = 0;

// Set true for ONE upload if you need to correct an already-running RTC.
// Restore false afterwards. Build time is approximate local wall time.
constexpr bool SET_FROM_BUILD = false;

bool valid(const DateTime& time) {
    return time.isValid() && time.year() >= 2024 && time.year() <= 2099;
}
void anchor(const DateTime& time) {
    epoch = time.unixtime();
    anchorUs = esp_timer_get_time();
}
bool responds() {
    Wire.beginTransmission(0x68);
    const uint8_t error = Wire.endTransmission();
    if (error) DebugLog::log("RTC address=0x68 I2C error=%u", unsigned(error));
    return error == 0;
}
}

void WatchClock::begin() {
    anchor(DateTime(F(__DATE__), F(__TIME__)));
    Wire.begin(Pins::SDA, Pins::SCL);
    Wire.setClock(100000);
    Wire.setTimeOut(50);
    if (!rtc.begin(&Wire)) {
        DebugLog::log("RTC not found; software time fallback");
        return;
    }
    if (SET_FROM_BUILD || rtc.lostPower() || !valid(rtc.now())) {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        adjusted = true;
        DebugLog::log("RTC adjusted to build time");
    }
    const DateTime value = rtc.now();
    online = valid(value);
    if (online) anchor(value);
    DebugLog::log("RTC init valid=%d set_at_boot=%d", online, adjusted);
}

void WatchClock::tick() {
    if (uint32_t(millis() - lastPoll) < 1000) return;
    lastPoll = millis();
    const bool wasOnline = online;
    online = false;
    // Probe before reading, and retry initialization if absent at boot.
    if (responds() && (wasOnline || rtc.begin(&Wire))) {
        const DateTime value = rtc.now();
        const bool lostPower = rtc.lostPower();
        online = !lostPower && valid(value);
        DebugLog::log("RTC raw=%04u-%02u-%02u %02u:%02u:%02u valid=%d lostPower=%d",
                      unsigned(value.year()), unsigned(value.month()), unsigned(value.day()),
                      unsigned(value.hour()), unsigned(value.minute()), unsigned(value.second()),
                      valid(value), lostPower);
        if (online) anchor(value);
    }
}

DateTime WatchClock::now() {
    // Keep ticking from the last good RTC value if I2C fails. Use a 64-bit
    // monotonic clock, so an uptime longer than millis() wrap is safe.
    return DateTime(epoch + uint32_t((esp_timer_get_time() - anchorUs) / 1000000));
}
bool WatchClock::healthy() { return online; }
bool WatchClock::setAtBoot() { return adjusted; }

void WatchClock::adjust(const DateTime& time) {
    if (!valid(time)) return;
    anchor(time);
    if (responds()) {
        rtc.adjust(time);
        online = true;
        DebugLog::log("RTC adjusted to %04u-%02u-%02u %02u:%02u:%02u",
                      unsigned(time.year()), unsigned(time.month()), unsigned(time.day()),
                      unsigned(time.hour()), unsigned(time.minute()), unsigned(time.second()));
    } else {
        DebugLog::log("RTC adjust I2C unreachable; software time anchored");
    }
}

void WatchClock::setEpoch(uint32_t epochSeconds) {
    adjust(DateTime(epochSeconds));
}
