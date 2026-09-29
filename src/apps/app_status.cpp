#include "app_status.h"
#include "core/watch_clock.h"
#include "core/debug_log.h"
#include "core/battery.h"
#include "core/net_sync.h"
#include "ersa/services/bluetooth_manager.h"
#include "ui/text_layout.h"
#include "fonts/misans_fonts.h"
#include "ersa/config/ui_strings.h"
#include <Arduino.h>

namespace AppStatus {

void render(Adafruit_GFX& display) {
    display.fillScreen(0);   // Solid black
    display.setTextColor(1); // White

    // Clean lowercase header
    display.setFont(&MiSansLatin_Bold10pt7b);
    display.setCursor(18, 24);
    display.print(ersa::strings::APP_TITLE_STATUS);

    display.setFont(&MiSansLatin_Regular8pt7b);

    constexpr int16_t leftX = 18;
    constexpr int16_t valX = 86;
    constexpr int16_t startY = 48;
    constexpr int16_t rowHeight = 18;

    // 1. RTC
    display.setCursor(leftX, startY);
    display.print("rtc");
    display.setCursor(valX, startY);
    display.print(WatchClock::healthy() ? ersa::strings::MSG_ONLINE : ersa::strings::MSG_OFFLINE);

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
        display.print(ersa::strings::MSG_USB_POWER);
    }

    // 3. Reset
    display.setCursor(leftX, startY + rowHeight * 2);
    display.print("reset");
    display.setCursor(valX, startY + rowHeight * 2);
    display.print(DebugLog::resetReasonName());

    auto& ble = ersa::services::BluetoothManager::instance();
    display.setCursor(leftX, startY + rowHeight * 3);
    display.print("ble");
    WatchText::line(display, ble.isConnected() ? "connected" :
                    ble.isAdvertising() ? "advertising" : "starting", valX, startY + rowHeight * 3, 98);
    display.setCursor(leftX, startY + rowHeight * 4);
    display.print("apple");
    WatchText::line(display, ble.notificationsReady() && ble.mediaReady() ? "ready" :
                    ble.notificationsReady() ? "alerts ready" :
                    ble.mediaReady() ? "music ready" : "waiting", valX, startY + rowHeight * 4, 98);

    // 6. Net Sync
    display.setCursor(leftX, startY + rowHeight * 5);
    display.print("sync");
    display.setCursor(valX, startY + rowHeight * 5);
    WatchText::line(display, NetSync::lastStatus(), valX, startY + rowHeight * 5, 98);

    // Clean footer
    WatchText::line(display, ble.isConnected() ? "ble connected" : "b1: reconnect ble", leftX, 168, 166);
    WatchText::line(display, "b2: sync / hold b1: back", leftX, 186, 166);
}

bool onButton(Buttons::Event event) {
    if (event == Buttons::Event::Next) {
        ersa::services::BluetoothManager::instance().restartAdvertising();
        return true;
    }
    if (event == Buttons::Event::Action || event == Buttons::Event::ActionLong) {
        NetSync::syncNtp();
        return true;
    }
    return false;
}

} // namespace AppStatus
