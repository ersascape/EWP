#include <Arduino.h>
#include "core/buttons.h"
#include "core/watch_clock.h"
#include "core/watch_config.h"
#include "core/battery.h"
#include "core/debug_log.h"
#include "ui/watch_ui.h"

#if !defined(CONFIG_IDF_TARGET_ESP32C3)
#error "This project requires an ESP32-C3 board."
#endif

void setup() {
    // Logs use USB Serial/JTAG. UART0 GPIO20/21 remain assigned to the EPD.
    DebugLog::begin();
    DebugLog::log("BOOT starting config");
    WatchConfig::begin();
    DebugLog::log("BOOT starting battery");
    Battery::begin();
    DebugLog::log("BOOT starting buttons");
    Buttons::begin();
    DebugLog::log("BOOT starting RTC");
    WatchClock::begin();
    DebugLog::log("BOOT starting display");
    WatchUi::begin();
    DebugLog::log("BOOT ready");
}

void loop() {
    Buttons::tick();
    WatchClock::tick();
    Battery::tick();
    DebugLog::tick();
    const auto event = Buttons::takeEvent();
    if (event != Buttons::Event::None) {
        WatchUi::onButton(event);
    }
    WatchUi::tick();
    delay(5);
}
