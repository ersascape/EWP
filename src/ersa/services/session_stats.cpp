#include "ersa/services/session_stats.h"
#include "ersa/services/storage_service.h"

namespace ersa { namespace services {
namespace {
uint32_t previousUptimeSeconds = 0;
uint32_t lastCheckpointMs = 0;
constexpr uint32_t CHECKPOINT_INTERVAL_MS = 5UL * 60UL * 1000UL;
}

void SessionStats::begin() {
    auto& storage = StorageService::instance();
    previousUptimeSeconds = static_cast<uint32_t>(storage.getInt("last_session_s", 0));
    const int32_t priorCheckpoint = storage.getInt("sess_chk_s", 0);
    if (priorCheckpoint > 0) previousUptimeSeconds = static_cast<uint32_t>(priorCheckpoint);
    storage.setInt("last_session_s", static_cast<int32_t>(previousUptimeSeconds));
    storage.setInt("sess_chk_s", 0);
    lastCheckpointMs = 0;
}

void SessionStats::tick(uint32_t uptimeMs) {
    if (uptimeMs - lastCheckpointMs < CHECKPOINT_INTERVAL_MS) return;
    lastCheckpointMs = uptimeMs;
    StorageService::instance().setInt("sess_chk_s", static_cast<int32_t>(uptimeMs / 1000));
}

uint32_t SessionStats::previousSessionUptimeSeconds() {
    return previousUptimeSeconds;
}

} }
