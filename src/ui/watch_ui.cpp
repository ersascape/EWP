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
            return Event::createInput(ButtonId::Button1, ButtonAction::Click, millis());
        case Buttons::Event::Previous:
            return Event::createInput(ButtonId::Button1, ButtonAction::DoubleClick, millis());
        case Buttons::Event::Home:
            return Event::createInput(ButtonId::Button1, ButtonAction::LongPress, millis());
        case Buttons::Event::Action:
            return Event::createInput(ButtonId::Button2, ButtonAction::Click, millis());
        case Buttons::Event::ActionAlt:
            return Event::createInput(ButtonId::Button2, ButtonAction::DoubleClick, millis());
        case Buttons::Event::ActionLong:
            return Event::createInput(ButtonId::Button2, ButtonAction::LongPress, millis());
        default:
            return Event();
    }
}

void renderCurrentApp() {
    const uint32_t started = millis();
    const DateTime time = WatchClock::now();
    const bool full = firstFrame || partialFrames >= 25;

    DebugLog::log("EPD begin app=%s mode=%s time=%02u:%02u:%02u",
                  appManager.getActiveApp() ? appManager.getActiveApp()->getId() : "none",
                  full ? "full" : "partial",
                  unsigned(time.hour()), unsigned(time.minute()), unsigned(time.second()));

    auto& espDisp = board.getEsp32Display();
    auto& gx = espDisp.getGxDisplay();

    if (full) gx.setFullWindow();
    else gx.setPartialWindow(0, 0, gx.width(), gx.height());

    gx.firstPage();
    do {
        gx.fillScreen(GxEPD_BLACK);
        gx.setTextColor(GxEPD_WHITE);
        gx.setTextWrap(false);

        if (appManager.getActiveApp()) {
            appManager.getActiveApp()->render(espDisp, full);
        }
    } while (gx.nextPage());

    panelPowered = true;
    lastActivityMs = millis();
    partialFrames = full ? 0 : partialFrames + 1;
    shownMinute = time.unixtime() / 60;
    shownRtcHealthy = WatchClock::healthy();
    firstFrame = false;
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

    if (appManager.isDirty() && (nowMs - lastFrameEnd >= 550)) {
        renderCurrentApp();
        appManager.markDirty(false);
    }

    if (panelPowered && (nowMs - lastActivityMs >= 8000)) {
        board.getEsp32Display().powerOff();
        panelPowered = false;
        DebugLog::log("EPD: powered off (idle timeout)");
    }
}
