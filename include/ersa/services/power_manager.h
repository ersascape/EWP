#pragma once

#include "ersa/common/types.h"
#include "ersa/hal/battery.h"
#include "ersa/events/event_bus.h"
#include <stdint.h>
#include <stddef.h>

namespace ersa {
namespace services {

enum class PowerState : uint8_t {
    Active = 0,
    Idle,
    LightSleep,
    DeepSleep
};

class PowerManager;

class WakeLock {
public:
    explicit WakeLock(const char* tag);
    ~WakeLock();

    WakeLock(const WakeLock&) = delete;
    WakeLock& operator=(const WakeLock&) = delete;

    WakeLock(WakeLock&& other) noexcept;
    WakeLock& operator=(WakeLock&& other) noexcept;

    void release();

private:
    const char* tag_{nullptr};
    bool active_{false};
};

class PowerManager {
public:
    static constexpr size_t MAX_WAKE_LOCKS = 16;
    static constexpr uint32_t BATTERY_SAMPLE_INTERVAL_MS = 10000;
    static constexpr uint32_t DEEP_SLEEP_TIMEOUT_MS = 60000;

    explicit PowerManager(hal::IBattery& battery, events::EventBus& bus = events::EventBus::instance());

    Result<void> init();
    void tick(uint32_t currentUptimeMs);

    // WakeLock API (Section 10)
    WakeLock acquireWakeLock(const char* tag);
    bool acquireWakeLockRaw(const char* tag);
    bool releaseWakeLockRaw(const char* tag);
    size_t getActiveWakeLockCount() const;
    bool hasWakeLocks() const;

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

    const char* wakeLockTags_[MAX_WAKE_LOCKS];
    size_t activeWakeLocks_{0};
};

} // namespace services
} // namespace ersa
