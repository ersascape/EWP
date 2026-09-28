#pragma once

#include "ersa/common/types.h"
#include "ersa/hal/display.h"
#include <stdint.h>

namespace ersa {
namespace services {

class DisplayManager {
public:
    static constexpr uint32_t MIN_REFRESH_INTERVAL_MS = 500;
    static constexpr uint32_t IDLE_POWEROFF_TIMEOUT_MS = 8000;
    static constexpr uint8_t FULL_REFRESH_FRAME_COUNT = 25;

    explicit DisplayManager(hal::IDisplay& display);

    Result<void> init();
    void tick(uint32_t currentUptimeMs);

    void markDirty(bool fullRefresh = false);
    bool isDirty() const;

    // Trigger a refresh if dirty and interval has elapsed
    bool updateIfDirty(uint32_t currentUptimeMs);

    // Explicit force refresh
    void refresh(bool full = false, uint32_t currentUptimeMs = 0);

    // User activity notification (keeps panel powered)
    void noteActivity(uint32_t currentUptimeMs);

    uint8_t getPartialFrameCount() const;
    void resetPartialFrameCount();

    hal::IDisplay& getDisplay();

    static DisplayManager& instance();
    static void setInstance(DisplayManager* instance);

private:
    hal::IDisplay& display_;
    bool dirty_{true};
    bool fullNeeded_{true};
    bool panelPowered_{false};
    uint8_t partialFrames_{0};
    uint32_t lastRefreshTime_{0};
    uint32_t lastActivityTime_{0};
};

} // namespace services
} // namespace ersa
