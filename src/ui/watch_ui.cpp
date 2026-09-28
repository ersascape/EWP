#include "ui/watch_ui.h"
#include "bsp/ampere_t1e/board_ampere_t1e.h"
#include "ersa/events/event_bus.h"
#include "ersa/app/application_manager.h"
#include "ersa/services/time_service.h"
#include "ersa/services/power_manager.h"
#include "ersa/services/display_manager.h"
#include "apps/apps_registry.h"
#include "apps/app_portal.h"
#include "core/watch_clock.h"
#include "core/debug_log.h"
#include "core/buttons.h"
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
uint8_t shownDay = 0;
bool shownRtcHealthy = false;
uint32_t lastFrameEnd = 0;
uint32_t lastActivityMs = 0;
bool panelPowered = false;
uint16_t partialFrames = 0;
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

    // 1. Hardware full refresh (waveform clear): ONLY on firstFrame after boot, or once every 360 partial updates (~6 hours), or day change at midnight.
    // NEVER on button press, scroll, or normal minute ticks!
    const bool dayChanged = (shownDay != 0 && time.day() != shownDay);
    const bool hardwareFull = firstFrame || (partialFrames >= 360) || dayChanged;

    DebugLog::log("EPD begin app=%s hwFull=%d time=%02u:%02u:%02u",
                  activeApp->getId(), hardwareFull,
                  unsigned(time.hour()), unsigned(time.minute()), unsigned(time.second()));

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

    panelPowered = true;
    lastActivityMs = millis();
    shownMinute = time.unixtime() / 60;
    shownDay = time.day();
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

    // Subscribe ApplicationManager to EventBus for decoupled event routing
    eventBus.subscribe(ersa::events::EventType::None, [](const ersa::events::Event& evt, void* user) {
        auto* mgr = static_cast<ersa::app::ApplicationManager*>(user);
        if (mgr) {
            mgr->handleEvent(evt);
        }
    }, &appManager);

    ersa::app::registerAllApps(appManager);
    appManager.switchTo("watchface_clock");

    NetSync::begin();

    renderCurrentApp();
    lastActivityMs = millis();
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

    // Auto-return to watchface after 60 seconds of inactivity on other screens
    if (appManager.getActiveApp() != nullptr &&
        strcmp(appManager.getActiveApp()->getId(), "watchface_clock") != 0 &&
        (nowMs - lastActivityMs >= 60000)) {
        DebugLog::log("UI: auto-returning to watchface after 60s idle");
        appManager.switchTo("watchface_clock");
        lastActivityMs = nowMs;
    }

    const uint32_t currentMinute = WatchClock::now().unixtime() / 60;
    if (currentMinute != shownMinute || WatchClock::healthy() != shownRtcHealthy) {
        appManager.markDirty(false);
    }

    const bool displayBusy = board.getEsp32Display().isBusy();
    if (appManager.isDirty() && !displayBusy && (nowMs - lastFrameEnd >= 150)) {
        renderCurrentApp();
        appManager.clearDirty();
    }

    // Power off EPD panel after 3 seconds of idle (drops panel draw to 0)
    if (panelPowered && (nowMs - lastActivityMs >= 3000)) {
        board.getEsp32Display().powerOff();
        panelPowered = false;
        DebugLog::log("EPD: powered off (idle timeout)");
    }

    // Low-power Light Sleep:
    // Conditions:
    // 1. Not dirty and display not busy
    // 2. No network sync active, no hotspot portal active, no active wake locks
    // 3. No buttons currently pressed or pending in queue
    // 4. Idle timeout elapsed:
    //    - On watchface: 4 seconds after last activity
    //    - On other apps: 12 seconds after last activity
    const bool onWatchface = (appManager.getActiveApp() != nullptr &&
                              strcmp(appManager.getActiveApp()->getId(), "watchface_clock") == 0);
    const uint32_t sleepIdleTimeout = onWatchface ? 4000 : 12000;

    const bool canSleep = !appManager.isDirty() &&
                          !board.getEsp32Display().isBusy() &&
                          !NetSync::isSyncing() &&
                          !AppPortal::isActive() &&
                          powerManager.canSleep() &&
                          !Buttons::isPressed() &&
                          !Buttons::hasPendingEvents() &&
                          ((nowMs - lastActivityMs) >= sleepIdleTimeout);

    if (canSleep) {
        if (panelPowered) {
            board.getEsp32Display().powerOff();
            panelPowered = false;
        }

        const DateTime now = WatchClock::now();
        const uint32_t sec = now.second();
        const uint32_t secRemaining = (sec < 60) ? (60 - sec) : 60;
        const uint64_t sleepUs = (uint64_t)secRemaining * 1000000ULL;

        powerManager.enterLightSleep(sleepUs);

        // Resume after wakeup
        lastActivityMs = millis();
    }
}
