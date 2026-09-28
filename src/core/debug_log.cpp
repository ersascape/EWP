#include "debug_log.h"
#include "board_pins.h"
#include "watch_clock.h"
#include <Arduino.h>
#include <esp_system.h>
#include <stdarg.h>
#include <Preferences.h>

// Serial must resolve to USB Serial/JTAG, never UART0 on EPD GPIO20/21.
#if !ARDUINO_USB_CDC_ON_BOOT || !ARDUINO_USB_MODE
#error "Watch logging requires ARDUINO_USB_MODE=1 and ARDUINO_USB_CDC_ON_BOOT=1"
#endif

namespace {
// One compact NVS write per boot, never once per heartbeat/refresh. Keep the
// previous causes so reopening USB (which can itself reset the board) does
// not erase evidence of the preceding battery reset.
struct BootHistory {
    uint32_t magic;
    uint32_t count;
    uint32_t causes[4]; // newest first
};
BootHistory history = {};
bool historySaved = false;

const char* reasonName(uint32_t reason) {
    switch (reason) {
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXTERNAL";
        case ESP_RST_SW: return "SOFTWARE";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "UNKNOWN";
    }
}

void recordBoot() {
    constexpr uint32_t magic = 0x57415431;
    Preferences prefs;
    const bool opened = prefs.begin("watch_diag", false);
    if (opened && prefs.getBytesLength("boots") == sizeof(history))
        prefs.getBytes("boots", &history, sizeof(history));
    if (history.magic != magic) history = {magic, 0, {0, 0, 0, 0}};
    ++history.count;
    for (unsigned i = 3; i > 0; --i) history.causes[i] = history.causes[i - 1];
    history.causes[0] = uint32_t(esp_reset_reason());
    if (opened) {
        historySaved = prefs.putBytes("boots", &history, sizeof(history)) == sizeof(history);
        prefs.end();
    }
    // Sudden power loss before this write completes may leave the preceding
    // record intact. This is diagnostic evidence, not a complete crash dump.
}
}

void DebugLog::begin() {
    recordBoot();
    Serial.setTxBufferSize(1024);
    Serial.begin(115200);
    Serial.setTxTimeoutMs(0);
    // No while (!Serial): the watch must run with no USB monitor attached.
}

void DebugLog::log(const char* format, ...) {
    if (!Serial) return;
    char text[240];
    const int prefix = snprintf(text, sizeof(text), "[%lu] ", (unsigned long)millis());
    va_list args;
    va_start(args, format);
    vsnprintf(text + prefix, sizeof(text) - prefix - 2, format, args);
    va_end(args);
    size_t length = strlen(text);
    text[length++] = '\n';
    // Drop a line rather than block buttons/display if the host stops reading.
    if (Serial.availableForWrite() >= static_cast<int>(length))
        Serial.write(reinterpret_cast<const uint8_t*>(text), length);
}

void DebugLog::tick() {
    static bool attached = false;
    static uint32_t lastReport = 0;
    const bool connected = bool(Serial);
    if (connected && !attached) {
        log("ErsaWearable boot=%lu reset=%s(%d) saved=%d; display shows HH:MM only",
            (unsigned long)history.count, resetReasonName(), int(esp_reset_reason()), historySaved);
        log("RESET history newest->oldest: %s, %s, %s, %s",
            reasonName(history.causes[0]), reasonName(history.causes[1]),
            reasonName(history.causes[2]), reasonName(history.causes[3]));
        log("PCB pins: upper S2/B1=GPIO%d lower S1/B2=GPIO%d; LOW=pressed",
            Pins::BUTTON_1, Pins::BUTTON_2);
    }
    attached = connected;
    if (uint32_t(millis() - lastReport) < 1000) return;
    lastReport = millis();
    const DateTime time = WatchClock::now();
    log("LOOP boot=%lu time=%02u:%02u:%02u rtc=%s B1=%d B2=%d EPD_BUSY=%d heap=%u",
        (unsigned long)history.count,
        unsigned(time.hour()), unsigned(time.minute()), unsigned(time.second()),
        WatchClock::healthy() ? "online" : "offline",
        digitalRead(Pins::BUTTON_1), digitalRead(Pins::BUTTON_2),
        digitalRead(Pins::EPD_BUSY), unsigned(ESP.getFreeHeap()));
}

void DebugLog::flush() {
    if (Serial) {
        Serial.flush();
    }
}

uint32_t DebugLog::bootCount() { return history.count; }
const char* DebugLog::resetReasonName() { return reasonName(uint32_t(esp_reset_reason())); }
