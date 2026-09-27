#include "app_status.h"
#include "core/watch_clock.h"
#include "core/debug_log.h"
#include "core/battery.h"
#include "core/net_sync.h"
#include <Arduino.h>
#include <Fonts/FreeSansBold12pt7b.h>

namespace AppStatus {

namespace {
void drawCentered(Adafruit_GFX& display, const char* text, int16_t y, const GFXfont* font = nullptr) {
    display.setFont(font);
    display.setTextSize(1);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((display.width() - int16_t(w)) / 2 - x1, y - y1);
    display.print(text);
}
} // namespace

void render(Adafruit_GFX& display) {
    drawCentered(display, "STATUS", 14, &FreeSansBold12pt7b);

    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(0); // Black

    display.setCursor(12, 42);
    display.print("RTC: ");
    display.print(WatchClock::healthy() ? "online" : "offline / invalid");

    display.setCursor(12, 60);
    display.print("Battery: ");
    if (Battery::isConnected()) {
        display.print(Battery::percentage());
        display.print("% (");
        display.print(Battery::millivolts());
        display.print(" mV)");
    } else {
        display.print("USB power");
    }

    display.setCursor(12, 78);
    display.print("Reset: ");
    display.print(DebugLog::resetReasonName());

    display.setCursor(12, 96);
    display.print("Boot count: ");
    display.print(DebugLog::bootCount());

    display.setCursor(12, 114);
    display.print("Free heap: ");
    display.print(ESP.getFreeHeap() / 1024);
    display.print(" KB");

    display.setCursor(12, 132);
    display.print("Net Sync: ");
    display.print(NetSync::lastStatus());

    drawCentered(display, "B2: SYNC NTP   Hold B1: CLOCK", 152);
}

bool onButton(Buttons::Event event) {
    if (event == Buttons::Event::Action || event == Buttons::Event::ActionLong) {
        NetSync::syncNtp();
        return true;
    }
    return false;
}

} // namespace AppStatus
