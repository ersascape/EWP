#include <Arduino.h>
#include "core/watch_clock.h"
#include "core/watch_config.h"
#include "core/debug_log.h"
#include "core/usb_control.h"
#include "core/dvfs.h"
#include "ui/watch_ui.h"

#if defined(CONFIG_PM_ENABLE) && CONFIG_PM_ENABLE && defined(CONFIG_FREERTOS_USE_TICKLESS_IDLE) && CONFIG_FREERTOS_USE_TICKLESS_IDLE
#include <esp_pm.h>
#endif
#include <esp_log.h>

#if !defined(CONFIG_IDF_TARGET_ESP32C3)
#error "This project requires an ESP32-C3 board."
#endif

void setup() {
    // Arduino's millivolt ADC helper reapplies the GPIO mode for each sample;
    // the IDF GPIO driver's INFO message makes the periodic battery read look
    // like a reboot loop in serial logs. Keep warnings and errors visible.
    esp_log_level_set("gpio", ESP_LOG_WARN);
    // Logs use USB Serial/JTAG. UART0 GPIO20/21 remain assigned to the EPD.
    DebugLog::begin();
    DebugLog::log("BOOT starting config");
    WatchConfig::begin();
#if defined(CONFIG_PM_ENABLE) && CONFIG_PM_ENABLE && defined(CONFIG_FREERTOS_USE_TICKLESS_IDLE) && CONFIG_FREERTOS_USE_TICKLESS_IDLE
    const esp_pm_config_esp32c3_t pmConfig = {
        .max_freq_mhz = 160,
        .min_freq_mhz = 40,
        // BLE modem sleep keeps advertising/connections alive while the
        // FreeRTOS tickless idle task places the CPU in light sleep.
        .light_sleep_enable = true
    };
    const esp_err_t pmResult = esp_pm_configure(&pmConfig);
    DebugLog::log("PWR: BLE-compatible automatic light sleep status=0x%x", unsigned(pmResult));
    if (pmResult == ESP_OK && !Dvfs::begin())
        DebugLog::log("DVFS: lock manager unavailable; idle clock remains 40 MHz");
#else
    DebugLog::log("PWR: automatic light sleep unavailable (PM/tickless-idle config missing)");
#endif
    DebugLog::log("BOOT starting display");
    WatchUi::begin();
    UsbControl::begin();
    DebugLog::log("BOOT ready");
}

void loop() {
    WatchClock::tick();
    Dvfs::tick();
    DebugLog::tick();
    UsbControl::tick();
    WatchUi::tick();
    delay(5);
}
