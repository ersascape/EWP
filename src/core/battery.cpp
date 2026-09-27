#include "battery.h"
#include "board_pins.h"
#include "ui/watch_icons.h"
#include "core/debug_log.h"
#include <Arduino.h>

namespace Battery {

namespace {
uint16_t cachedMv = 0;
uint8_t cachedPercent = 100;
bool connected = false;
uint32_t lastSampleTime = 0;

uint8_t mvToPercent(uint16_t mv) {
    if (mv >= 4200) return 100;
    if (mv <= 3500) return 0;
    if (mv >= 4050) return 90 + ((mv - 4050) * 10) / 150;
    if (mv >= 3920) return 75 + ((mv - 3920) * 15) / 130;
    if (mv >= 3820) return 50 + ((mv - 3820) * 25) / 100;
    if (mv >= 3740) return 25 + ((mv - 3740) * 25) / 80;
    if (mv >= 3650) return 10 + ((mv - 3650) * 15) / 90;
    return ((mv - 3500) * 10) / 150;
}

void sample() {
    uint32_t sumMv = 0;
    constexpr uint8_t SAMPLES = 16;
    for (uint8_t i = 0; i < SAMPLES; ++i) {
        sumMv += analogReadMilliVolts(Pins::BATTERY_ADC);
        delayMicroseconds(50);
    }
    const uint32_t rawMv = sumMv / SAMPLES;

    // Assuming standard 1:1 (half-voltage) divider: V_batt = V_adc * 2
    const uint32_t battMv = rawMv * 2;

    if (battMv >= 2800 && battMv <= 4500) {
        connected = true;
        cachedMv = battMv;
        cachedPercent = mvToPercent(cachedMv);
    } else {
        // Divider not present on GPIO2, or running on pure USB without divider
        connected = false;
        cachedMv = battMv;
        cachedPercent = 100;
    }
}
} // namespace

void begin() {
    pinMode(Pins::BATTERY_ADC, INPUT);
    analogSetAttenuation(ADC_11db); // Full 0 - 2.5V ADC range
    sample();
    DebugLog::log("BATTERY init: raw_mv=%u connected=%d pct=%u%%",
                  cachedMv, connected, cachedPercent);
}

void tick() {
    if (millis() - lastSampleTime >= 10000) { // Sample every 10 seconds
        lastSampleTime = millis();
        sample();
    }
}

uint16_t millivolts() { return cachedMv; }
uint8_t percentage() { return cachedPercent; }
bool isConnected() { return connected; }

const uint8_t* iconBitmap() {
    if (!connected) return WatchIcons::batteryFull;
    if (cachedMv >= 4250) return WatchIcons::batteryCharging;
    if (cachedPercent >= 85) return WatchIcons::batteryFull;
    if (cachedPercent >= 70) return WatchIcons::battery83;
    if (cachedPercent >= 55) return WatchIcons::battery67;
    if (cachedPercent >= 35) return WatchIcons::battery50;
    if (cachedPercent >= 20) return WatchIcons::battery33;
    return WatchIcons::battery17;
}

} // namespace Battery
