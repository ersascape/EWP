#include "watchfaces/app_watchface.h"
#include "watchfaces/watchface_clock.h"
#include "ersa/app/application_manager.h"
#include "core/watch_clock.h"
#include "core/net_sync.h"
#include "core/debug_log.h"

#if defined(ARDUINO)
#include "hal/esp32/esp32_display.h"
#endif

namespace ersa {
namespace watchface {

AppWatchface& AppWatchface::instance() {
    static AppWatchface s_watchface;
    return s_watchface;
}

AppWatchface::AppWatchface() = default;

void AppWatchface::onEnter() {
    shownMinute_ = UINT32_MAX;
    DebugLog::log("APP: Watchface onEnter");
}

void AppWatchface::onExit() {
    DebugLog::log("APP: Watchface onExit");
}

void AppWatchface::onEvent(const events::Event& event) {
    if (event.type == events::EventType::ButtonClicked) {
        if (event.button.button == events::ButtonId::Button1) {
            app::ApplicationManager::instance().switchTo("app_drawer");
        } else if (event.button.button == events::ButtonId::Button2) {
            NetSync::syncAll();
            app::ApplicationManager::instance().markDirty(false);
        }
    } else if (event.type == events::EventType::ButtonLongPressed) {
        if (event.button.button == events::ButtonId::Button2) {
            app::ApplicationManager::instance().switchTo("app_drawer");
        }
    } else if (event.type == events::EventType::BatteryChanged) {
        app::ApplicationManager::instance().markDirty(true);
    } else if (event.type == events::EventType::MinuteTick) {
        DateTime time = WatchClock::now();
        bool needFull = (shownDay_ != 0 && time.day() != shownDay_);
        app::ApplicationManager::instance().markDirty(needFull);
    }
}

void AppWatchface::render(hal::IDisplay& display, bool fullRefresh) {
#if defined(ARDUINO)
    auto* espDisplay = static_cast<hal::Esp32Display*>(&display);
    if (espDisplay) {
        DateTime time = WatchClock::now();
        WatchfaceClock::render(espDisplay->getGfx(), time, fullRefresh);
        shownMinute_ = time.unixtime() / 60;
        shownDay_ = time.day();
        shownRtcHealthy_ = WatchClock::healthy();
    }
#else
    (void)display; (void)fullRefresh;
#endif
}

} // namespace watchface
} // namespace ersa
