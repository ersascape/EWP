#include "app_agenda.h"
#include "core/net_sync.h"
#include "core/debug_log.h"
#include "fonts/misans_fonts.h"
#include "ersa/config/ui_strings.h"
#include <Arduino.h>

namespace AppAgenda {

namespace {
size_t pageOffset = 0;

void drawRight(Adafruit_GFX& display, const char* text, int16_t rightX, int16_t y, const GFXfont* font) {
    display.setFont(font);
    display.setTextSize(1);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(rightX - int16_t(w) - x1, y);
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

void render(Adafruit_GFX& display, const DateTime& now, bool full) {
    (void)now;
    if (full) {
        display.fillScreen(0);   // Solid black
        display.setTextColor(1); // White

        // Clean minimal footer
        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(18, 186);
        const size_t total = NetSync::eventCount();
        display.print((total == 0) ? ersa::strings::NAV_AGENDA_EMPTY_FOOT : ersa::strings::NAV_AGENDA_FOOTER);
    } else {
        // Partial refresh: clear header and cards area (y: 0 to 172), leaving footer intact
        display.fillRect(0, 0, 200, 172, 0);
    }

    display.setTextColor(1);

    // Clean lowercase header
    display.setFont(&MiSansLatin_Bold10pt7b);
    display.setCursor(18, 24);
    display.print(ersa::strings::APP_TITLE_AGENDA);

    const size_t total = NetSync::eventCount();

    if (total > 0) {
        char countBuf[16];
        snprintf(countBuf, sizeof(countBuf), "%u of %u",
                 unsigned(pageOffset + 1), unsigned(total));
        drawRight(display, countBuf, 184, 24, &MiSansLatin_Regular8pt7b);
    } else {
        drawRight(display, NetSync::lastStatus(), 184, 24, &MiSansLatin_Regular8pt7b);
    }

    if (total == 0) {
        display.setFont(&MiSansLatin_Regular10pt7b);
        display.setCursor(18, 70);
        display.print(ersa::strings::MSG_NO_EVENTS_TODAY);

        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(18, 96);
        display.print(ersa::strings::MSG_PRESS_SYNC_CALDAV);
        return;
    }

    if (pageOffset >= total) pageOffset = 0;

    constexpr int16_t cardWidth = 172;
    constexpr int16_t cardHeight = 52;
    constexpr int16_t cardX = 14;

    for (size_t i = 0; i < 2; ++i) {
        const size_t idx = pageOffset + i;
        if (idx >= total) break;

        const int16_t cardY = 42 + i * 58;
        const auto& ev = NetSync::getEvent(idx);

        // Crisp white rounded card outline
        display.drawRoundRect(cardX, cardY, cardWidth, cardHeight, 4, 1);
        // Accent bar on left edge
        display.fillRoundRect(cardX + 2, cardY + 3, 3, cardHeight - 6, 1, 1);

        // Event Time in bold
        display.setFont(&MiSansLatin_Bold10pt7b);
        display.setCursor(cardX + 12, cardY + 20);
        display.print(ev.timeStr[0] != '\0' ? ev.timeStr : "all day");

        // Event Title in regular
        char titleBuf[20];
        strncpy(titleBuf, ev.title, sizeof(titleBuf) - 1);
        titleBuf[sizeof(titleBuf) - 1] = '\0';

        display.setFont(&MiSansLatin_Regular10pt7b);
        display.setCursor(cardX + 12, cardY + 42);
        display.print(titleBuf);
    }
}

} // namespace AppAgenda
