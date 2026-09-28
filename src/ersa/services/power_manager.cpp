#include "ersa/services/power_manager.h"
#include <string.h>

namespace ersa {
namespace services {

static PowerManager* s_powerManagerInstance = nullptr;

WakeLock::WakeLock(const char* tag)
    : tag_(tag), active_(false) {
    if (s_powerManagerInstance && tag_) {
        active_ = s_powerManagerInstance->acquireWakeLockRaw(tag_);
    }
}

WakeLock::~WakeLock() {
    release();
}

WakeLock::WakeLock(WakeLock&& other) noexcept
    : tag_(other.tag_), active_(other.active_) {
    other.tag_ = nullptr;
    other.active_ = false;
}

WakeLock& WakeLock::operator=(WakeLock&& other) noexcept {
    if (this != &other) {
        release();
        tag_ = other.tag_;
        active_ = other.active_;
        other.tag_ = nullptr;
        other.active_ = false;
    }
    return *this;
}

void WakeLock::release() {
    if (active_ && s_powerManagerInstance && tag_) {
        s_powerManagerInstance->releaseWakeLockRaw(tag_);
        active_ = false;
        tag_ = nullptr;
    }
}

PowerManager& PowerManager::instance() {
    return *s_powerManagerInstance;
}

void PowerManager::setInstance(PowerManager* instance) {
    s_powerManagerInstance = instance;
}

PowerManager::PowerManager(hal::IBattery& battery, events::EventBus& bus)
    : battery_(battery), bus_(bus) {
    for (size_t i = 0; i < MAX_WAKE_LOCKS; ++i) {
        wakeLockTags_[i] = nullptr;
    }
}

Result<void> PowerManager::init() {
    Result<void> res = battery_.init();
    battery_.sample();
    cachedMv_ = battery_.millivolts();
    cachedPercent_ = battery_.percentage();
    cachedConnected_ = battery_.isConnected();
    cachedCharging_ = battery_.isCharging();
    return res;
}

WakeLock PowerManager::acquireWakeLock(const char* tag) {
    return WakeLock(tag);
}

bool PowerManager::acquireWakeLockRaw(const char* tag) {
    if (!tag) return false;
    for (size_t i = 0; i < MAX_WAKE_LOCKS; ++i) {
        if (wakeLockTags_[i] && strcmp(wakeLockTags_[i], tag) == 0) {
            return true; // Already acquired
        }
    }
    for (size_t i = 0; i < MAX_WAKE_LOCKS; ++i) {
        if (!wakeLockTags_[i]) {
            wakeLockTags_[i] = tag;
            activeWakeLocks_++;
            return true;
        }
    }
    return false;
}

bool PowerManager::releaseWakeLockRaw(const char* tag) {
    if (!tag) return false;
    for (size_t i = 0; i < MAX_WAKE_LOCKS; ++i) {
        if (wakeLockTags_[i] && strcmp(wakeLockTags_[i], tag) == 0) {
            wakeLockTags_[i] = nullptr;
            if (activeWakeLocks_ > 0) activeWakeLocks_--;
            return true;
        }
    }
    return false;
}

size_t PowerManager::getActiveWakeLockCount() const {
    return activeWakeLocks_;
}

bool PowerManager::hasWakeLocks() const {
    return activeWakeLocks_ > 0;
}

void PowerManager::noteActivity(uint32_t currentUptimeMs) {
    lastActivityMs_ = currentUptimeMs;
    state_ = PowerState::Active;
}

uint32_t PowerManager::getIdleTimeMs(uint32_t currentUptimeMs) const {
    if (currentUptimeMs >= lastActivityMs_) {
        return currentUptimeMs - lastActivityMs_;
    }
    return 0;
}

void PowerManager::tick(uint32_t currentUptimeMs) {
    if (currentUptimeMs - lastBatterySampleMs_ >= BATTERY_SAMPLE_INTERVAL_MS) {
        lastBatterySampleMs_ = currentUptimeMs;
        battery_.sample();

        const uint16_t mv = battery_.millivolts();
        const uint8_t pct = battery_.percentage();
        const bool conn = battery_.isConnected();
        const bool chg = battery_.isCharging();

        if (mv != cachedMv_ || pct != cachedPercent_ || conn != cachedConnected_ || chg != cachedCharging_) {
            cachedMv_ = mv;
            cachedPercent_ = pct;
            cachedConnected_ = conn;
            cachedCharging_ = chg;

            bus_.publish(events::Event::createBatteryChanged(mv, pct, conn, chg, currentUptimeMs));
        }
    }
}

uint16_t PowerManager::getBatteryMv() const {
    return cachedMv_;
}

uint8_t PowerManager::getBatteryPercent() const {
    return cachedPercent_;
}

bool PowerManager::isBatteryConnected() const {
    return cachedConnected_;
}

bool PowerManager::isCharging() const {
    return cachedCharging_;
}

PowerState PowerManager::getState() const {
    return state_;
}

void PowerManager::requestState(PowerState state) {
    state_ = state;
}

} // namespace services
} // namespace ersa
