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

    gx.setFullWindow();
    if (hardwareFull) {
        gx.display(false); // Hardware full refresh (clears ghosting)
        partialFrames = 0;
        firstFrame = false;
    } else {
        gx.display(true);  // Hardware fast partial refresh of full screen (differential, no flash)
        partialFrames++;
    }
    appManager.clearAppSwitched();

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

    uint32_t nowMs = millis();
    timeService.tick(nowMs);
    powerManager.tick(nowMs);

    const uint32_t idleMs = (nowMs >= lastActivityMs) ? (nowMs - lastActivityMs) : 0;

    // Auto-return to watchface after 60 seconds of inactivity on other screens
    if (appManager.getActiveApp() != nullptr &&
        strcmp(appManager.getActiveApp()->getId(), "watchface_clock") != 0 &&
        (idleMs >= 60000)) {
        DebugLog::log("UI: auto-returning to watchface after 60s idle");
        appManager.switchTo("watchface_clock");
        lastActivityMs = nowMs;
    }

    const uint32_t currentMinute = WatchClock::now().unixtime() / 60;
    if (currentMinute != shownMinute || WatchClock::healthy() != shownRtcHealthy) {
        appManager.markDirty(false);
    }

    const bool displayBusy = board.getEsp32Display().isBusy();
    const uint32_t timeSinceRender = (nowMs >= lastFrameEnd) ? (nowMs - lastFrameEnd) : 0;
    if (appManager.isDirty() && !displayBusy && (timeSinceRender >= 150)) {
        renderCurrentApp();
        appManager.clearDirty();
        nowMs = millis(); // Refresh timestamp immediately after rendering finishes
    }

#if defined(ARDUINO) && defined(CONFIG_IDF_TARGET_ESP32C3)
    // Keep CPU awake while USB serial terminal is connected for live monitoring/debugging
    if (Serial) {
        return;
    }
#endif

    // Only sleep on the watchface when idle
    // Interactive apps (calendar, agenda, todo, drawer, etc.) stay awake for instant button response
    const bool onWatchface = (appManager.getActiveApp() != nullptr &&
                              strcmp(appManager.getActiveApp()->getId(), "watchface_clock") == 0);
    if (!onWatchface) {
        return;
    }

    const uint32_t postIdleMs = (nowMs >= lastActivityMs) ? (nowMs - lastActivityMs) : 0;
    const uint32_t postRenderAge = (nowMs >= lastFrameEnd) ? (nowMs - lastFrameEnd) : 0;

    // Low-power Light Sleep on Watchface:
    // Conditions:
    // 1. Not dirty and display hardware controller not busy
    // 2. Physical settling guard: at least 2000 ms elapsed since last frame end
    //    (allows E-ink microcapsules and charge pumps to settle without electrical interruption)
    // 3. At least 8000 ms elapsed since last button activity
    // 4. No network sync active, no hotspot portal active, no active wake locks
    // 5. No buttons currently pressed or pending in queue
    const bool canSleep = !appManager.isDirty() &&
                          !board.getEsp32Display().isBusy() &&
                          (postRenderAge >= 2000) &&
                          (postIdleMs >= 8000) &&
                          !NetSync::isSyncing() &&
                          !AppPortal::isActive() &&
                          powerManager.canSleep() &&
                          !Buttons::isPressed() &&
                          !Buttons::hasPendingEvents();

    if (canSleep) {
        const DateTime now = WatchClock::now();
        const uint32_t sec = now.second();
        const uint32_t secRemaining = (sec < 60) ? (60 - sec) : 60;
        const uint64_t sleepUs = (uint64_t)secRemaining * 1000000ULL;

        powerManager.enterLightSleep(sleepUs);

        // Resume after wakeup
        lastActivityMs = millis();
    }
}
