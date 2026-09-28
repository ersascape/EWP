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
    bool onEvent(const events::Event& event) override;
    void render(hal::IDisplay& display, bool fullRefresh) override;

    static AppWatchface& instance();

private:
    uint32_t shownMinute_{UINT32_MAX};
    bool shownRtcHealthy_{false};
};

} // namespace watchface
} // namespace ersa
