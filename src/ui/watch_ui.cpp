#include "ui/watch_ui.h"
#include "bsp/ampere_t1e/board_ampere_t1e.h"
#include "ersa/events/event_bus.h"
#include "ersa/app/application_manager.h"
#include "ersa/services/time_service.h"
#include "ersa/services/power_manager.h"
#include "ersa/services/display_manager.h"
#include "ersa/services/bluetooth_manager.h"
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
ersa::services::BluetoothManager bluetoothManager(board.getBluetooth(), eventBus);

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

    bluetoothManager.init();
    ersa::services::BluetoothManager::setInstance(&bluetoothManager);

    // Subscribe ApplicationManager to EventBus for decoupled event routing
    eventBus.subscribe(ersa::events::EventType::None, [](const ersa::events::Event& evt, void* user) {
        auto* mgr = static_cast<ersa::app::ApplicationManager*>(user);
        if (mgr) {
            mgr->handleEvent(evt);
        }
    }, &appManager);

    // Auto-switch to call screen on incoming call
    eventBus.subscribe(ersa::events::EventType::CallIncoming, [](const ersa::events::Event&, void* user) {
        auto* mgr = static_cast<ersa::app::ApplicationManager*>(user);
        if (mgr) {
            mgr->switchTo("app_call");
            mgr->markDirty(false);
        }
    }, &appManager);

    // Re-render when track or playback state changes (for NowPlaying app and Watchface complication)
    eventBus.subscribe(ersa::events::EventType::MediaTrackChanged, [](const ersa::events::Event&, void* user) {
        auto* mgr = static_cast<ersa::app::ApplicationManager*>(user);
        if (mgr && mgr->getActiveApp()) {
            const char* id = mgr->getActiveApp()->getId();
            if (strcmp(id, "app_media") == 0 || strcmp(id, "watchface_clock") == 0) {
                mgr->markDirty(false);
            }
        }
    }, &appManager);

    eventBus.subscribe(ersa::events::EventType::MediaStateChanged, [](const ersa::events::Event&, void* user) {
        auto* mgr = static_cast<ersa::app::ApplicationManager*>(user);
        if (mgr && mgr->getActiveApp()) {
            const char* id = mgr->getActiveApp()->getId();
            if (strcmp(id, "app_media") == 0 || strcmp(id, "watchface_clock") == 0) {
                mgr->markDirty(false);
            }
        }
    }, &appManager);

    // Auto-switch to notification screen on incoming message (unless currently on a call)
    eventBus.subscribe(ersa::events::EventType::NotificationReceived, [](const ersa::events::Event&, void* user) {
        auto* mgr = static_cast<ersa::app::ApplicationManager*>(user);
        if (mgr && mgr->getActiveApp()) {
            const char* id = mgr->getActiveApp()->getId();
            if (strcmp(id, "app_call") != 0 &&
                bluetoothManager.getCallState() != ersa::services::CallState::Incoming &&
                bluetoothManager.getCallState() != ersa::services::CallState::Active) {
                mgr->switchTo("app_notifications");
                mgr->markDirty(false);
            }
        }
    }, &appManager);

    for (auto type : {ersa::events::EventType::CallEnded, ersa::events::EventType::CallAccepted,
                      ersa::events::EventType::BleConnected, ersa::events::EventType::BleDisconnected,
                      ersa::events::EventType::NotificationRemoved, ersa::events::EventType::NotificationsCleared}) {
        eventBus.subscribe(type, [](const ersa::events::Event&, void* user) {
            static_cast<ersa::app::ApplicationManager*>(user)->markDirty(false);
        }, &appManager);
    }

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

    // Quick dial on watchface: holding B1 dials top recent contact
    if (appManager.getActiveApp() != nullptr &&
        strcmp(appManager.getActiveApp()->getId(), "watchface_clock") == 0 &&
        legacyEvent == Buttons::Event::Home) {
        if (bluetoothManager.canDial() && bluetoothManager.getRecentCallCount() > 0) {
            DebugLog::log("UI: Hold B1 on watchface -> Quick dial recent %s (%s)",
                          bluetoothManager.getRecentCall(0).name,
                          bluetoothManager.getRecentCall(0).number);
            bluetoothManager.dialRecent(0);
            appManager.switchTo("app_call");
            appManager.markDirty(false);
            return;
        }
    }

    const ersa::events::Event evt = toErsaInputEvent(legacyEvent);
    const bool handled = appManager.handleEvent(evt);

    if (handled) {
        appManager.markDirty(false);
    }
}

void WatchUi::tick() {
    bluetoothManager.tick();
    board.getInput().poll();
    eventBus.dispatchQueue();
    appManager.tick();

    uint32_t nowMs = millis();
    timeService.tick(nowMs);
    powerManager.tick(nowMs);

    const uint32_t idleMs = (nowMs >= lastActivityMs) ? (nowMs - lastActivityMs) : 0;

    // Auto-return to watchface after 60 seconds of inactivity on other screens (except during calls)
    if (appManager.getActiveApp() != nullptr &&
        strcmp(appManager.getActiveApp()->getId(), "watchface_clock") != 0 &&
        strcmp(appManager.getActiveApp()->getId(), "app_call") != 0 &&
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
    if (appManager.isDirty() && !displayBusy && (timeSinceRender >= 20)) {
        renderCurrentApp();
        appManager.clearDirty();
        nowMs = millis();
    }

    if (!appManager.isDirty() && !Buttons::hasPendingEvents()) {
        delay(10); // Yield to FreeRTOS idle task when idle
    } else {
        delay(1);  // Ultra-fast 1ms loop response during button/UI interaction
    }
}
