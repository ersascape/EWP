#include "app_agenda.h"
#include "core/net_sync.h"
#include "core/debug_log.h"
#include <Arduino.h>
#include <Fonts/FreeSansBold9pt7b.h>

namespace AppAgenda {

namespace {
size_t pageOffset = 0;

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

void begin() {
    pageOffset = 0;
}

bool onButton(Buttons::Event event) {
    const size_t total = NetSync::eventCount();
    if (event == Buttons::Event::Action) {
        if (total > 2 && (pageOffset + 2) < total) {
            pageOffset += 2;
            return true;
        } else if (total > 2 && (pageOffset + 2) >= total) {
            pageOffset = 0;
            return true;
        } else {
            // When few events or default card, B2 triggers direct CalDAV sync
            NetSync::syncAll();
            return true;
        }
    } else if (event == Buttons::Event::ActionLong) {
        NetSync::syncAll();
        return true;
    }
    return false;
}

void render(Adafruit_GFX& display, const DateTime& now) {
    drawCentered(display, "TODAY'S EVENTS", 16, &FreeSansBold9pt7b);

    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(0); // Black

    char dateBuf[32];
    snprintf(dateBuf, sizeof(dateBuf), "%02u/%02u/%04u (%s)",
             unsigned(now.day()), unsigned(now.month()), unsigned(now.year()),
             NetSync::lastStatus());
    drawCentered(display, dateBuf, 30);

    display.drawFastHLine(14, 38, 172, 0);

    const size_t total = NetSync::eventCount();

    if (total == 0) {
        display.drawRoundRect(14, 52, 172, 60, 4, 0);
        drawCentered(display, "No events today", 74);
        drawCentered(display, "Press B2 to sync", 92);
        return;
    }

    if (pageOffset >= total) pageOffset = 0;

    constexpr int16_t cardWidth = 172;
    constexpr int16_t cardHeight = 46;
    constexpr int16_t cardX = 14;

    for (size_t i = 0; i < 2; ++i) {
        const size_t idx = pageOffset + i;
        if (idx >= total) break;

        const int16_t cardY = 46 + i * 52;
        const auto& ev = NetSync::getEvent(idx);

        // Card border
        display.drawRoundRect(cardX, cardY, cardWidth, cardHeight, 4, 0);

        // Indicator bar on left
        display.fillRoundRect(cardX + 2, cardY + 2, 4, cardHeight - 4, 2, 0);

        char truncatedTitle[24];
        strncpy(truncatedTitle, ev.title, sizeof(truncatedTitle) - 1);
        truncatedTitle[sizeof(truncatedTitle) - 1] = '\0';

        display.setCursor(cardX + 12, cardY + 10);
        display.print(truncatedTitle);

        display.setCursor(cardX + 12, cardY + 26);
        display.print("@ ");
        display.print(ev.timeStr);
    }

    if (total > 2) {
        char pageBuf[16];
        snprintf(pageBuf, sizeof(pageBuf), "(%u-%u of %u)",
                 unsigned(pageOffset + 1),
                 unsigned(min(pageOffset + 2, total)),
                 unsigned(total));
        drawCentered(display, pageBuf, 154);
    }
}

} // namespace AppAgenda
