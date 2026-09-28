#include "app_notifications.h"
#include "ersa/services/bluetooth_manager.h"
#include "ersa/app/application_manager.h"
#include "fonts/misans_fonts.h"
#include "core/debug_log.h"
#include <Arduino.h>

namespace AppNotifications {

namespace {
size_t currentIndex = 0;

void printWrapped(Adafruit_GFX& display, const char* text, int16_t x, int16_t startY, int16_t lineSpacing, int maxLines, int maxCharsPerLine) {
    if (!text || text[0] == '\0') return;

    int line = 0;
    const char* p = text;

    while (*p && line < maxLines) {
        // Find line break or word wrap point
        int len = 0;
        int lastSpace = -1;
        while (p[len] && len < maxCharsPerLine) {
            if (p[len] == ' ') lastSpace = len;
            if (p[len] == '\n') {
                lastSpace = len;
                break;
            }
            len++;
        }

        int printLen = len;
        if (p[len] != '\0' && p[len] != '\n' && lastSpace > 0) {
            printLen = lastSpace;
        }

        char lineBuf[32];
        int copyLen = (printLen < int(sizeof(lineBuf) - 1)) ? printLen : (int(sizeof(lineBuf) - 1));
        memcpy(lineBuf, p, copyLen);
        lineBuf[copyLen] = '\0';

        display.setCursor(x, startY + line * lineSpacing);
        display.print(lineBuf);
        line++;

        p += printLen;
        while (*p == ' ' || *p == '\n') p++; // skip delimiter
    }
}
} // namespace

void begin() {
    currentIndex = 0;
}

bool onButton(Buttons::Event event) {
    auto& bleMgr = ersa::services::BluetoothManager::instance();
    const size_t total = bleMgr.getNotificationCount();

    if (event == Buttons::Event::Next) {
        if (total > 1) {
            currentIndex = (currentIndex + 1) % total;
            DebugLog::log("NOTIF: Next notification -> index %u/%u", unsigned(currentIndex + 1), unsigned(total));
            return true;
        }
    } else if (event == Buttons::Event::Action || event == Buttons::Event::ActionLong || event == Buttons::Event::Home) {
        // B2 = Dismiss back to clock
        DebugLog::log("NOTIF: Dismiss -> back to clock");
        ersa::app::ApplicationManager::instance().switchTo("watchface_clock");
        return true;
    }

    return false;
}

void render(Adafruit_GFX& display, bool full) {
    (void)full;
    auto& bleMgr = ersa::services::BluetoothManager::instance();
    const size_t total = bleMgr.getNotificationCount();

    display.fillScreen(0);   // Solid black
    display.setTextColor(1); // White

    if (total == 0) {
        // Empty state
        display.setFont(&MiSansLatin_Bold10pt7b);
        display.setCursor(18, 24);
        display.print("notifications");

        display.drawRoundRect(14, 38, 172, 114, 6, 1);

        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(32, 80);
        display.print("no new messages");
        display.setCursor(32, 102);
        display.print("alerts from iphone");
        display.setCursor(32, 120);
        display.print("will appear here");

        // Footer button
        display.fillRoundRect(14, 162, 172, 26, 4, 1);
        display.setTextColor(0);
        display.setFont(&MiSansLatin_Bold8pt7b);
        display.setCursor(44, 179);
        display.print("B2: BACK TO CLOCK");
        return;
    }

    if (currentIndex >= total) {
        currentIndex = 0;
    }

    const auto& notif = bleMgr.getNotification(currentIndex);

    // 1. Header
    display.setFont(&MiSansLatin_Bold10pt7b);
    display.setCursor(18, 24);
    display.print("notifications");

    // Page indicator (e.g. "1/3")
    if (total > 1) {
        char countBuf[16];
        snprintf(countBuf, sizeof(countBuf), "%u/%u", unsigned(currentIndex + 1), unsigned(total));
        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(160, 24);
        display.print(countBuf);
    }

    // 2. Notification Box
    display.drawRoundRect(14, 36, 172, 118, 6, 1);
    display.drawRoundRect(15, 37, 170, 116, 5, 1);

    // Title / Sender
    display.setFont(&MiSansLatin_Bold10pt7b);
    display.setCursor(24, 58);
    display.print((notif.title && notif.title[0]) ? notif.title : "Notification");

    // Subtle divider
    display.drawFastHLine(24, 68, 152, 1);

    // Message Body wrapped
    display.setFont(&MiSansLatin_Regular8pt7b);
    printWrapped(display, notif.message, 24, 88, 16, 4, 21);

    // 3. Action pill button
    display.fillRoundRect(14, 162, 172, 26, 4, 1);
    display.setTextColor(0);
    display.setFont(&MiSansLatin_Bold8pt7b);

    if (total > 1) {
        display.setCursor(24, 179);
        display.print("B1: NEXT   B2: DISMISS");
    } else {
        display.setCursor(50, 179);
        display.print("B2: DISMISS");
    }
}

} // namespace AppNotifications
