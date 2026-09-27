#include "app_status.h"
#include "core/watch_clock.h"
#include "core/debug_log.h"
#include "core/battery.h"
#include "core/net_sync.h"
#include "fonts/misans_fonts.h"
#include <Arduino.h>

namespace AppStatus {

void render(Adafruit_GFX& display) {
    display.fillScreen(0);   // Solid black
    display.setTextColor(1); // White

    // Clean lowercase header
    display.setFont(&MiSansLatin_Bold10pt7b);
    display.setCursor(18, 24);
    display.print("status");

    display.setFont(&MiSansLatin_Regular8pt7b);

    constexpr int16_t leftX = 18;
    constexpr int16_t valX = 86;
    constexpr int16_t startY = 48;
    constexpr int16_t rowHeight = 20;

    // 1. RTC
    display.setCursor(leftX, startY);
    display.print("rtc");
    display.setCursor(valX, startY);
    display.print(WatchClock::healthy() ? "online" : "offline");

    // 2. Battery
    display.setCursor(leftX, startY + rowHeight);
    display.print("battery");
    display.setCursor(valX, startY + rowHeight);
    if (Battery::isConnected()) {
        char battBuf[20];
        snprintf(battBuf, sizeof(battBuf), "%u%% (%u mV)",
                 Battery::percentage(), Battery::millivolts());
        display.print(battBuf);
    } else {
        display.print("USB power");
    }

    // 3. Reset
    display.setCursor(leftX, startY + rowHeight * 2);
    display.print("reset");
    display.setCursor(valX, startY + rowHeight * 2);
    display.print(DebugLog::resetReasonName());

    // 4. Boot
    display.setCursor(leftX, startY + rowHeight * 3);
    display.print("boot");
    display.setCursor(valX, startY + rowHeight * 3);
    display.print(DebugLog::bootCount());

    // 5. Heap
    display.setCursor(leftX, startY + rowHeight * 4);
    display.print("heap");
    display.setCursor(valX, startY + rowHeight * 4);
    char heapBuf[16];
    snprintf(heapBuf, sizeof(heapBuf), "%u KB", unsigned(ESP.getFreeHeap() / 1024));
    display.print(heapBuf);

    // 6. Net Sync
    display.setCursor(leftX, startY + rowHeight * 5);
    display.print("sync");
    display.setCursor(valX, startY + rowHeight * 5);
    display.print(NetSync::lastStatus());

    // Clean footer
    display.setCursor(leftX, 186);
    display.print("sync B2   menu B1");
}

bool onButton(Buttons::Event event) {
    if (event == Buttons::Event::Action || event == Buttons::Event::ActionLong) {
        NetSync::syncNtp();
        return true;
    }
    return false;
}

} // namespace AppStatus
