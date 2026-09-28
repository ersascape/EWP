#pragma once

#include "ersa/common/types.h"
#include "ersa/hal/battery.h"
#include "ersa/events/event_bus.h"
#include <stdint.h>

namespace ersa {
namespace services {

enum class PowerState : uint8_t {
    Active = 0,
    Idle,
    LightSleep,
    DeepSleep
};

class PowerManager {
public:
    static constexpr uint32_t BATTERY_SAMPLE_INTERVAL_MS = 10000;
    static constexpr uint32_t DEEP_SLEEP_TIMEOUT_MS = 60000; // Sleep after 60s idle

    explicit PowerManager(hal::IBattery& battery, events::EventBus& bus = events::EventBus::instance());

    Result<void> init();
    void tick(uint32_t currentUptimeMs);

    void noteActivity(uint32_t currentUptimeMs);
    uint32_t getIdleTimeMs(uint32_t currentUptimeMs) const;

    uint16_t getBatteryMv() const;
    uint8_t getBatteryPercent() const;
    bool isBatteryConnected() const;
    bool isCharging() const;

    PowerState getState() const;
    void requestState(PowerState state);

    static PowerManager& instance();
    static void setInstance(PowerManager* instance);

private:
    hal::IBattery& battery_;
    events::EventBus& bus_;

    PowerState state_{PowerState::Active};
    uint32_t lastActivityMs_{0};
    uint32_t lastBatterySampleMs_{0};

    uint16_t cachedMv_{0};
    uint8_t cachedPercent_{100};
    bool cachedConnected_{false};
    bool cachedCharging_{false};
};

} // namespace services
} // namespace ersa
