#include "app_agenda.h"
#include "core/net_sync.h"
#include "core/debug_log.h"
#include "fonts/misans_fonts.h"
#include <Arduino.h>

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
    if (event == Buttons::Event::Next) {
        // B1 = SCROLL through event cards
        if (total > 2) {
            pageOffset = (pageOffset + 2 < total) ? (pageOffset + 2) : 0;
            return true;
        }
        return false;
    } else if (event == Buttons::Event::Action || event == Buttons::Event::ActionLong) {
        // B2 = OK / ACTION: Trigger CalDAV sync
        NetSync::syncAll();
        return true;
    }
    return false;
}

void render(Adafruit_GFX& display, const DateTime& now) {
    drawCentered(display, "CALDAV AGENDA", 18, &MiSansLatin_Bold10pt7b);

    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(0); // Black

    char dateBuf[32];
    snprintf(dateBuf, sizeof(dateBuf), "%02u/%02u (%s)",
             unsigned(now.day()), unsigned(now.month()),
             NetSync::lastStatus());
    drawCentered(display, dateBuf, 30);

    display.drawFastHLine(14, 38, 172, 0);

    const size_t total = NetSync::eventCount();

    if (total == 0) {
        display.drawRoundRect(14, 56, 172, 56, 4, 0);
        drawCentered(display, "No events found", 76);
        drawCentered(display, "Press B2 to Sync", 94);
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
