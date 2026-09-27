#include "watchface_clock.h"
#include "ui/watch_icons.h"
#include "core/watch_clock.h"
#include "core/watch_config.h"
#include "core/battery.h"
#include "core/net_sync.h"
#include <Arduino.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <ctype.h>

namespace WatchfaceClock {

namespace {
const char* const weekdaysShort[] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

const char* const monthsShort[] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

void drawCenteredText(Adafruit_GFX& display, const char* text, int16_t cx, int16_t cy, const GFXfont* font = nullptr) {
    display.setFont(font);
    display.setTextSize(1);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(cx - int16_t(w / 2) - x1, cy - int16_t(h / 2) - y1);
    display.print(text);
}
} // namespace

void render(Adafruit_GFX& display, const DateTime& time) {
    const auto& cfg = WatchConfig::get();
    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(0); // Black

    // 1. TOP BAR: Date & AM/PM Badge (Left) & Battery Meter (Right)
    const uint8_t dow = time.dayOfTheWeek() % 7;
    const uint8_t mon = (time.month() >= 1 && time.month() <= 12) ? (time.month() - 1) : 0;
    const char* const ampm = (time.hour() >= 12) ? "PM" : "AM";

    char datePill[24];
    if (!cfg.militaryTime) {
        snprintf(datePill, sizeof(datePill), "%s %u %s  %s",
                 weekdaysShort[dow],
                 unsigned(time.day()),
                 monthsShort[mon],
                 ampm);
    } else {
        snprintf(datePill, sizeof(datePill), "%s %u %s",
                 weekdaysShort[dow],
                 unsigned(time.day()),
                 monthsShort[mon]);
    }

    int16_t dx, dy;
    uint16_t dw, dh;
    display.getTextBounds(datePill, 0, 0, &dx, &dy, &dw, &dh);
    const int16_t pillW = dw + 12;
    display.fillRoundRect(10, 7, pillW, 18, 4, 0);
    display.setTextColor(1); // White
    display.setCursor(16, 12);
    display.print(datePill);
    display.setTextColor(0); // Black

    // Battery readout (Right margin: 10px)
    char battStr[10];
    if (Battery::isConnected()) {
        snprintf(battStr, sizeof(battStr), "%u%%", Battery::percentage());
    } else {
        snprintf(battStr, sizeof(battStr), "USB");
    }

    int16_t bx, by;
    uint16_t bw, bh;
    display.getTextBounds(battStr, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor(162 - int16_t(bw), 12);
    display.print(battStr);
    display.drawBitmap(166, 8, Battery::iconBitmap(), 24, 16, 0);

    // Header divider line (10px margins on both sides: 10 to 190)
    display.drawFastHLine(10, 30, 180, 0);

    // 2. HERO TIME: Large bold FreeSansBold24pt7b, perfectly centered at X = 100
    char timeBuf[16];
    if (cfg.militaryTime) {
        snprintf(timeBuf, sizeof(timeBuf), "%02u:%02u",
                 unsigned(time.hour()), unsigned(time.minute()));
    } else {
        uint8_t h = time.hour() % 12;
        if (h == 0) h = 12;
        snprintf(timeBuf, sizeof(timeBuf), "%u:%02u", h, unsigned(time.minute()));
    }

    display.setFont(&FreeSansBold24pt7b);
    display.setTextSize(1);
    int16_t tx1, ty1;
    uint16_t tw, th;
    display.getTextBounds(timeBuf, 0, 0, &tx1, &ty1, &tw, &th);
    const int16_t curX = 100 - int16_t(tw / 2) - tx1;
    const int16_t curY = 74 - int16_t(th / 2) - ty1;
    display.setCursor(curX, curY);
    display.print(timeBuf);

    // Day progress gauge (Centered: 24 to 176, width 152)
    const uint16_t minuteOfDay = time.hour() * 60 + time.minute();
    constexpr int16_t barX = 24;
    constexpr int16_t barY = 98;
    constexpr int16_t barW = 152;
    constexpr int16_t barH = 5;
    const int16_t fillW = (minuteOfDay * barW) / 1440;

    display.drawRoundRect(barX, barY, barW, barH, 2, 0);
    if (fillW > 0) {
        display.fillRoundRect(barX, barY, max((int16_t)3, fillW), barH, 2, 0);
    }

    // Mid divider line (Centered: 10 to 190)
    display.drawFastHLine(10, 114, 180, 0);

    // 3. BOTTOM INFO: Calendar Name, Event / Task glance, and Footer
    char calTitle[32] = "MURENA TEAM";
    if (cfg.caldavCalendar[0] != '\0') {
        strncpy(calTitle, cfg.caldavCalendar, sizeof(calTitle) - 1);
        calTitle[sizeof(calTitle) - 1] = '\0';
        for (char* p = calTitle; *p; ++p) {
            if (*p == '-' || *p == '_') *p = ' ';
            else *p = toupper((unsigned char)*p);
        }
    }
    drawCenteredText(display, calTitle, 100, 130, &FreeSansBold9pt7b);

    // Event or task glance summary
    display.setFont(nullptr);
    display.setTextSize(1);

    char glanceBuf[36] = "";
    const size_t totalEvents = NetSync::eventCount();
    const size_t totalTodos = NetSync::todoCount();

    if (totalEvents > 0) {
        const auto& ev = NetSync::getEvent(0);
        snprintf(glanceBuf, sizeof(glanceBuf), "%.16s @ %s", ev.title, ev.timeStr);
    } else if (totalTodos > 0) {
        size_t openCount = 0;
        for (size_t i = 0; i < totalTodos; ++i) {
            if (!NetSync::getTodo(i).completed) ++openCount;
        }
        if (openCount == 0) {
            snprintf(glanceBuf, sizeof(glanceBuf), "All %u tasks completed", (unsigned)totalTodos);
        } else {
            snprintf(glanceBuf, sizeof(glanceBuf), "%u of %u tasks pending",
                     (unsigned)openCount, (unsigned)totalTodos);
        }
    } else {
        snprintf(glanceBuf, sizeof(glanceBuf), "CalDAV: %s", NetSync::lastStatus());
    }
    drawCenteredText(display, glanceBuf, 100, 150);

    // Bottom divider line (Centered: 10 to 190)
    display.drawFastHLine(10, 166, 180, 0);

    // Symmetrical button actions footer (14px margin on both sides)
    display.setCursor(14, 178);
    display.print("B1: APPS");

    display.setCursor(138, 178);
    display.print("B2: SYNC");
}

} // namespace WatchfaceClock
