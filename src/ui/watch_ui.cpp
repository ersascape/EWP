#include "ui/watch_ui.h"
#include "bsp/ampere_t1e/board_ampere_t1e.h"
#include "ersa/events/event_bus.h"
#include "ersa/app/application_manager.h"
#include "ersa/services/time_service.h"
#include "ersa/services/power_manager.h"
#include "ersa/services/display_manager.h"
#include "apps/apps_registry.h"
#include "core/watch_clock.h"
#include "core/debug_log.h"
#include "core/net_sync.h"
#include <Arduino.h>

namespace {

ersa::board::BoardAmpereT1e& board = ersa::board::BoardAmpereT1e::instance();
ersa::events::EventBus& eventBus = ersa::events::EventBus::instance();
ersa::app::ApplicationManager& appManager = ersa::app::ApplicationManager::instance();

ersa::services::TimeService timeService(board.getRtc(), eventBus);
ersa::services::PowerManager powerManager(board.getBattery(), eventBus);
ersa::services::DisplayManager displayManager(board.getDisplay());

uint32_t shownMinute = UINT32_MAX;
bool shownRtcHealthy = false;
uint32_t lastFrameEnd = 0;
uint32_t lastActivityMs = 0;
bool panelPowered = false;
uint8_t partialFrames = 0;
bool firstFrame = true;

ersa::events::Event toErsaInputEvent(Buttons::Event legacy) {
    using namespace ersa::events;
    switch (legacy) {
        case Buttons::Event::Next:
            return Event::createButton(EventType::ButtonClicked, ButtonId::Button1, millis());
        case Buttons::Event::Previous:
            return Event::createButton(EventType::ButtonDoubleClicked, ButtonId::Button1, millis());
        case Buttons::Event::Home:
            return Event::createButton(EventType::ButtonLongPressed, ButtonId::Button1, millis());
        case Buttons::Event::Action:
            return Event::createButton(EventType::ButtonClicked, ButtonId::Button2, millis());
        case Buttons::Event::ActionAlt:
            return Event::createButton(EventType::ButtonDoubleClicked, ButtonId::Button2, millis());
        case Buttons::Event::ActionLong:
            return Event::createButton(EventType::ButtonLongPressed, ButtonId::Button2, millis());
        default:
            return Event();
    }
}

void renderCurrentApp() {
    const uint32_t started = millis();
    const DateTime time = WatchClock::now();
    auto* activeApp = appManager.getActiveApp();
    if (!activeApp) return;

    auto& espDisp = board.getEsp32Display();
    auto& gx = espDisp.getGxDisplay();

    // 1. Hardware full refresh (waveform clear): firstFrame or periodic (every 30 partial updates)
    // 2. Full-canvas redraw (fast partial refresh): app switched or app requested full refresh
    // 3. Sub-window partial refresh: dynamic sub-region inside current app
    const bool hardwareFull = firstFrame || (partialFrames >= 30);
    const bool fullCanvas = hardwareFull || appManager.isFullRefreshNeeded() || appManager.isAppSwitched();

    DebugLog::log("EPD begin app=%s hwFull=%d fullCanvas=%d time=%02u:%02u:%02u",
                  activeApp->getId(), hardwareFull, fullCanvas,
                  unsigned(time.hour()), unsigned(time.minute()), unsigned(time.second()));

    if (fullCanvas) {
        gx.fillScreen(GxEPD_BLACK);
        gx.setTextColor(GxEPD_WHITE);
        gx.setTextWrap(false);
        activeApp->render(espDisp, true);

        if (hardwareFull) {
            gx.setFullWindow();
            gx.display(false); // Hardware full refresh (clears ghosting)
            partialFrames = 0;
            firstFrame = false;
        } else {
            gx.display(true);  // Hardware fast partial refresh of full screen (differential, no flash)
            partialFrames++;
        }
        appManager.clearAppSwitched();
    } else {
        const ersa::Rect bounds = activeApp->getPartialBounds();
        if (bounds.w >= gx.width() && bounds.h >= gx.height()) {
            gx.fillScreen(GxEPD_BLACK);
            gx.setTextColor(GxEPD_WHITE);
            gx.setTextWrap(false);
            activeApp->render(espDisp, false);
            gx.display(true);
        } else {
            // Windowed partial refresh: clear ONLY the sub-window in buffer
            gx.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, GxEPD_BLACK);
            gx.setTextColor(GxEPD_WHITE);
            gx.setTextWrap(false);
            activeApp->render(espDisp, false);
            gx.displayWindow(bounds.x, bounds.y, bounds.w, bounds.h);
        }
        partialFrames++;
    }

    panelPowered = true;
    lastActivityMs = millis();
    shownMinute = time.unixtime() / 60;
    shownRtcHealthy = WatchClock::healthy();
    lastFrameEnd = millis();

    DebugLog::log("EPD end duration=%lu ms BUSY=%d (partialFrames=%u)",
                  (unsigned long)(lastFrameEnd - started), espDisp.isBusy(), partialFrames);
}

} // namespace

void WatchUi::begin() {
    DebugLog::log("UI: initializing Ampere Works T1E board");
    board.init();

    timeService.init();
    ersa::services::TimeService::setInstance(&timeService);

    powerManager.init();
    ersa::services::PowerManager::setInstance(&powerManager);

    displayManager.init();
    ersa::services::DisplayManager::setInstance(&displayManager);

    ersa::app::registerAllApps(appManager);
    appManager.switchTo("watchface_clock");

    NetSync::begin();

    renderCurrentApp();
    DebugLog::log("UI: boot complete, active app: %s",
                  appManager.getActiveApp() ? appManager.getActiveApp()->getTitle() : "none");
}

void WatchUi::onButton(Buttons::Event legacyEvent) {
    if (legacyEvent == Buttons::Event::None) return;

    lastActivityMs = millis();
    displayManager.noteActivity(lastActivityMs);
    powerManager.noteActivity(lastActivityMs);

    const ersa::events::Event evt = toErsaInputEvent(legacyEvent);
    const bool handled = appManager.handleEvent(evt);

    if (handled) {
        appManager.markDirty(false);
    }
}

void WatchUi::tick() {
    board.getInput().poll();
    eventBus.dispatchQueue();
    appManager.tick();

    const uint32_t nowMs = millis();
    timeService.tick(nowMs);
    powerManager.tick(nowMs);

    const uint32_t currentMinute = WatchClock::now().unixtime() / 60;
    if (currentMinute != shownMinute || WatchClock::healthy() != shownRtcHealthy) {
        appManager.markDirty(false);
    }

    const bool displayBusy = board.getEsp32Display().isBusy();
    if (appManager.isDirty() && !displayBusy && (nowMs - lastFrameEnd >= 150)) {
        renderCurrentApp();
        appManager.clearDirty();
    }

    if (panelPowered && (nowMs - lastActivityMs >= 8000)) {
        board.getEsp32Display().powerOff();
        panelPowered = false;
        DebugLog::log("EPD: powered off (idle timeout)");
    }
}
