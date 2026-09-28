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

bool AppWatchface::onEvent(const events::Event& event) {
    if (event.category == events::EventCategory::Input) {
        if (event.input.button == events::ButtonId::Button1 &&
            event.input.action == events::ButtonAction::Click) {
            // Button 1 single click opens Drawer
            app::ApplicationManager::instance().switchTo("app_drawer");
            return true;
        } else if (event.input.button == events::ButtonId::Button2 &&
                   event.input.action == events::ButtonAction::LongPress) {
            // Button 2 long press opens Drawer
            app::ApplicationManager::instance().switchTo("app_drawer");
            return true;
        } else if (event.input.button == events::ButtonId::Button2 &&
                   event.input.action == events::ButtonAction::Click) {
            // Button 2 single click triggers instant CalDAV & NTP sync
            NetSync::syncAll();
            return true;
        }
    } else if (event.category == events::EventCategory::Time) {
        // Redraw on minute tick
        return true;
    } else if (event.category == events::EventCategory::Battery) {
        return true;
    }
    return false;
}

void AppWatchface::render(hal::IDisplay& display, bool fullRefresh) {
    (void)fullRefresh;
#if defined(ARDUINO)
    auto* espDisplay = static_cast<hal::Esp32Display*>(&display);
    if (espDisplay) {
        DateTime time = WatchClock::now();
        WatchfaceClock::render(espDisplay->getGfx(), time);
        shownMinute_ = time.unixtime() / 60;
        shownRtcHealthy_ = WatchClock::healthy();
    }
#else
    (void)display;
#endif
}

} // namespace watchface
} // namespace ersa
