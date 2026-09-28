#include "ersa/services/power_manager.h"

namespace ersa {
namespace services {

static PowerManager* s_powerManagerInstance = nullptr;

PowerManager& PowerManager::instance() {
    return *s_powerManagerInstance;
}

void PowerManager::setInstance(PowerManager* instance) {
    s_powerManagerInstance = instance;
}

PowerManager::PowerManager(hal::IBattery& battery, events::EventBus& bus)
    : battery_(battery), bus_(bus) {}

Result<void> PowerManager::init() {
    Result<void> res = battery_.init();
    battery_.sample();
    cachedMv_ = battery_.millivolts();
    cachedPercent_ = battery_.percentage();
    cachedConnected_ = battery_.isConnected();
    cachedCharging_ = battery_.isCharging();
    return res;
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

            bus_.publish(events::Event::createBatteryUpdate(mv, pct, conn, chg, currentUptimeMs));
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
