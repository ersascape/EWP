#include "ui/watch_ui.h"
#include "core/watch_clock.h"
#include "core/debug_log.h"
#include "core/buttons.h"
#include "core/net_sync.h"
#include "board_pins.h"
#include "watchfaces/watchface_clock.h"
#include "apps/app_drawer.h"
#include "apps/app_calendar.h"
#include "apps/app_agenda.h"
#include "apps/app_todo.h"
#include "apps/app_portal.h"
#include "apps/app_status.h"
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <esp_system.h>

namespace {
GxEPD2_BW<GxEPD2_154_GDEY0154D67, GxEPD2_154_GDEY0154D67::HEIGHT>
    display(GxEPD2_154_GDEY0154D67(Pins::EPD_CS, Pins::EPD_DC,
                                 Pins::EPD_RST, Pins::EPD_BUSY));

enum Screen : uint8_t { Clock, Drawer, Calendar, Agenda, Todo, Hotspot, Status, Count };
Screen screen = Clock;

bool dirty = true, firstFrame = true, shownRtcHealthy = false;
uint32_t shownMinute = UINT32_MAX, lastFrameEnd = 0;
uint8_t partialFrames = 0;

void busyCallback(const void*) {
    // Sample buttons during panel BUSY wait; never draw here.
    Buttons::tick();
    static uint32_t lastReport = 0;
    if (uint32_t(millis() - lastReport) >= 2000) {
        lastReport = millis();
        DebugLog::log("EPD waiting: BUSY=%d", digitalRead(Pins::EPD_BUSY));
    }
    delay(1);
}

void centered(const char* text, int16_t top, const GFXfont* font = nullptr) {
    display.setFont(font);
    display.setTextSize(1);
    int16_t x, y;
    uint16_t width, height;
    display.getTextBounds(text, 0, 0, &x, &y, &width, &height);
    display.setCursor((display.width() - int16_t(width)) / 2 - x, top - y);
    display.print(text);
}

void render() {
    const uint32_t started = millis();
    const DateTime time = WatchClock::now();

    const bool full = firstFrame || partialFrames >= 20;
    DebugLog::log("EPD begin screen=%u mode=%s time=%02u:%02u:%02u",
                  unsigned(screen), full ? "full" : "partial",
                  unsigned(time.hour()), unsigned(time.minute()), unsigned(time.second()));

    if (full) display.setFullWindow();
    else display.setPartialWindow(0, 0, display.width(), display.height());

    display.firstPage();
    do {
        display.fillScreen(GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setTextWrap(false);

        // Delegate rendering to modular screen components
        if (screen == Clock) {
            WatchfaceClock::render(display, time);
        } else if (screen == Drawer) {
            AppDrawer::render(display);
        } else if (screen == Calendar) {
            AppCalendar::render(display, time);
        } else if (screen == Agenda) {
            AppAgenda::render(display, time);
        } else if (screen == Todo) {
            AppTodo::render(display);
        } else if (screen == Hotspot) {
            AppPortal::render(display);
        } else if (screen == Status) {
            AppStatus::render(display);
        }
    } while (display.nextPage());

    display.powerOff();
    partialFrames = full ? 0 : partialFrames + 1;
    shownMinute = time.unixtime() / 60;
    shownRtcHealthy = WatchClock::healthy();
    firstFrame = false;
    dirty = false;
    lastFrameEnd = millis();

    DebugLog::log("EPD end duration=%lu ms BUSY=%d",
                  (unsigned long)(lastFrameEnd - started), digitalRead(Pins::EPD_BUSY));
}
} // namespace

void WatchUi::begin() {
    AppDrawer::begin();
    AppCalendar::begin();
    AppAgenda::begin();
    AppTodo::begin();
    AppPortal::begin();
    NetSync::begin();

    SPI.begin(Pins::SCK, Pins::MISO, Pins::MOSI, Pins::EPD_CS);
    display.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display.init(0, true, 10, false);
    display.epd2.setBusyCallback(busyCallback);
    display.setRotation(0);
    render();
}

void WatchUi::onButton(Buttons::Event event) {
    const Screen before = screen;
    const DateTime time = WatchClock::now();

    // Universal navigation: Hold B1 from ANY app exits back to App Drawer (or from Drawer to Clock)
    if (event == Buttons::Event::Home) {
        if (screen == Drawer || screen == Clock) {
            screen = Clock;
        } else {
            screen = Drawer;
        }
    } else if (screen == Clock) {
        if (event == Buttons::Event::Next || event == Buttons::Event::ActionLong) {
            screen = Drawer;
        } else if (event == Buttons::Event::Action) {
            // Quick sync on watchface
            NetSync::syncAll();
            dirty = true;
        }
    } else if (screen == Drawer) {
        if (event == Buttons::Event::Next) {
            AppDrawer::next();
            dirty = true;
        } else if (event == Buttons::Event::Action) {
            const auto item = AppDrawer::selected();
            if (item == AppDrawer::Item::Clock) {
                screen = Clock;
            } else if (item == AppDrawer::Item::Calendar) {
                screen = Calendar;
                AppCalendar::resetToCurrentMonth(time);
            } else if (item == AppDrawer::Item::Agenda) {
                screen = Agenda;
            } else if (item == AppDrawer::Item::Todo) {
                screen = Todo;
            } else if (item == AppDrawer::Item::Hotspot) {
                screen = Hotspot;
            } else if (item == AppDrawer::Item::Status) {
                screen = Status;
            }
        }
    } else if (screen == Calendar) {
        if (AppCalendar::onButton(event, time)) dirty = true;
    } else if (screen == Agenda) {
        if (AppAgenda::onButton(event)) dirty = true;
    } else if (screen == Todo) {
        if (AppTodo::onButton(event)) dirty = true;
    } else if (screen == Hotspot) {
        if (AppPortal::onButton(event)) dirty = true;
    } else if (screen == Status) {
        if (AppStatus::onButton(event)) dirty = true;
    }

    if (before != screen) dirty = true;
    DebugLog::log("UI screen=%u -> %u (event=%s)", unsigned(before), unsigned(screen), Buttons::name(event));
}

void WatchUi::tick() {
    AppPortal::tick();

    if (WatchClock::now().unixtime() / 60 != shownMinute ||
        WatchClock::healthy() != shownRtcHealthy) {
        dirty = true;
    }
    // Limit repeated e-paper updates.
    if (dirty && uint32_t(millis() - lastFrameEnd) >= 750) {
        render();
    }
}
