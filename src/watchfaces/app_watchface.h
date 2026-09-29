#pragma once

#include "ersa/app/application.h"
#include <stdint.h>

namespace ersa {
namespace watchface {

class AppWatchface : public app::Application {
public:
    AppWatchface();
    ~AppWatchface() override = default;

    const char* getId() const override { return "watchface_clock"; }
    const char* getTitle() const override { return "Watchface"; }

    void onEnter() override;
    void onExit() override;
    void onEvent(const events::Event& event) override;
    void render(hal::IDisplay& display, bool fullRefresh) override;

    // Clock/media/call content changes above the fixed weekday/date footer.
    // Midnight already forces a full refresh in WatchUi.
    Rect getPartialBounds() const override { return Rect{0, 0, 200, 160}; }

    static AppWatchface& instance();

private:
    uint32_t shownMinute_{UINT32_MAX};
    uint8_t shownDay_{0};
    bool shownRtcHealthy_{false};
};

} // namespace watchface
} // namespace ersa
