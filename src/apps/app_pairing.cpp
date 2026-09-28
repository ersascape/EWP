#include "app_pairing.h"
#include "ersa/services/bluetooth_manager.h"
#include "ersa/app/application_manager.h"
#include "fonts/misans_fonts.h"
#include "core/debug_log.h"
#include <Arduino.h>

namespace AppPairing {

void begin() {}

bool onButton(Buttons::Event event) {
    auto& bleMgr = ersa::services::BluetoothManager::instance();

    if (event == Buttons::Event::Next) {
        // B1 Click: Restart pairing / advertising
        DebugLog::log("BLE: B1 pressed -> Restart advertising");
        bleMgr.restartAdvertising();
        return true;
    } else if (event == Buttons::Event::Action || event == Buttons::Event::Home) {
        // B2 or Long B1: Return to drawer
        ersa::app::ApplicationManager::instance().switchTo("app_drawer");
        return true;
    }
    return false;
}

void render(Adafruit_GFX& display) {
    auto& bleMgr = ersa::services::BluetoothManager::instance();
    const bool connected = bleMgr.isConnected();
    const char* devName = bleMgr.getDeviceName();
    const char* devAddr = bleMgr.getDeviceAddress();

    display.fillScreen(0);   // Solid black
    display.setTextColor(1); // White

    // 1. Header
    display.setFont(&MiSansLatin_Bold10pt7b);
    display.setCursor(18, 22);
    display.print("bluetooth pairing");

    // 2. Status Badge
    if (connected) {
        display.fillRoundRect(16, 32, 168, 22, 4, 1);
        display.setTextColor(0); // Black text on white
        display.setFont(&MiSansLatin_Bold8pt7b);
        display.setCursor(26, 47);
        display.print("* CONNECTED");
        display.setTextColor(1);
    } else {
        display.drawRoundRect(16, 32, 168, 22, 4, 1);
        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(24, 47);
        display.print("> WAITING TO PAIR...");
    }

    // 3. Device Details Box
    display.drawRoundRect(16, 60, 168, 64, 5, 1);

    display.setFont(&MiSansLatin_Regular8pt7b);
    display.setCursor(24, 78);
    display.print("name: ");
    display.setFont(&MiSansLatin_Bold8pt7b);
    display.print(devName ? devName : "Ersa Wearable");

    display.setFont(&MiSansLatin_Regular8pt7b);
    display.setCursor(24, 96);
    display.print("addr: ");
    display.print(devAddr ? devAddr : "--");

    display.setCursor(24, 114);
    display.print("svc:  0000FFE0");

    // 4. Instructions
    display.setFont(&MiSansLatin_Regular8pt7b);
    display.setCursor(18, 140);
    display.print("1. Open Chrome or BLE app");
    display.setCursor(18, 154);
    display.print("2. Pair 'Ersa Wearable'");

    // 5. Button Hints
    display.setCursor(18, 174);
    display.print("b1: restart pairing");
    display.setCursor(18, 190);
    display.print("b2: back to drawer");
}

} // namespace AppPairing
