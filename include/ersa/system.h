#pragma once

#include "ersa/services/time_service.h"
#include "ersa/services/power_manager.h"
#include "ersa/services/network_manager.h"
#include "ersa/services/storage_service.h"
#include "ersa/services/settings_service.h"
#include "ersa/services/logging_service.h"
#include "ersa/events/event_bus.h"

namespace ersa {
namespace system {

inline services::TimeService& time() {
    return services::TimeService::instance();
}

inline services::PowerManager& power() {
    return services::PowerManager::instance();
}

inline services::NetworkManager& network() {
    return services::NetworkManager::instance();
}

inline services::StorageService& storage() {
    return services::StorageService::instance();
}

inline services::SettingsService& settings() {
    return services::SettingsService::instance();
}

inline services::LoggingService& logger() {
    return services::LoggingService::instance();
}

inline events::EventBus& events() {
    return events::EventBus::instance();
}

} // namespace system
} // namespace ersa
