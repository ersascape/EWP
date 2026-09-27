#include "watchface_clock.h"
#include "ui/watch_icons.h"
#include "core/watch_clock.h"
#include "core/watch_config.h"
#include "core/battery.h"
#include "core/net_sync.h"
#include <Arduino.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

namespace WatchfaceClock {

namespace {
const char* const weekdaysShort[] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

const char* const weekdaysFull[] = {
    "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY",
    "THURSDAY", "FRIDAY", "SATURDAY"
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
    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(0); // Black

    // 1. TOP BAR: Date Badge (Left) & Battery Meter (Right)
    const uint8_t dow = time.dayOfTheWeek() % 7;
    const uint8_t mon = (time.month() >= 1 && time.month() <= 12) ? (time.month() - 1) : 0;

    char datePill[20];
    snprintf(datePill, sizeof(datePill), "%s %u %s",
             weekdaysShort[dow],
             unsigned(time.day()),
             monthsShort[mon]);

    int16_t dx, dy;
    uint16_t dw, dh;
    display.getTextBounds(datePill, 0, 0, &dx, &dy, &dw, &dh);
    const int16_t pillW = dw + 12;
    display.fillRoundRect(10, 7, pillW, 18, 4, 0);
    display.setTextColor(1); // White
    display.setCursor(16, 12);
    display.print(datePill);
    display.setTextColor(0); // Black

    // Battery readout
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

    // Header divider line
    display.drawFastHLine(10, 30, 180, 0);

    // 2. HERO TIME: Large bold FreeSansBold24pt7b
    const auto& cfg = WatchConfig::get();
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

    // 12-hour AM/PM badge
    if (!cfg.militaryTime) {
        display.setFont(nullptr);
        display.setTextSize(1);
        display.fillRoundRect(curX + tw + 4, curY + ty1 + 2, 20, 11, 2, 0);
        display.setTextColor(1);
        display.setCursor(curX + tw + 6, curY + ty1 + 4);
        display.print(time.hour() >= 12 ? "PM" : "AM");
        display.setTextColor(0);
    }

    // Day progress gauge (0 - 1440 minutes)
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

    // Mid divider line
    display.drawFastHLine(10, 114, 180, 0);

    // 3. BOTTOM INFO: Full Day of Week & Year, Task glance, and Navigation hint
    char fullDayBuf[32];
    snprintf(fullDayBuf, sizeof(fullDayBuf), "%s, %u",
             weekdaysFull[dow], unsigned(time.year()));
    drawCenteredText(display, fullDayBuf, 100, 130, &FreeSansBold9pt7b);

    // Task glance summary
    display.setFont(nullptr);
    display.setTextSize(1);

    const size_t totalTodos = NetSync::todoCount();
    size_t openCount = 0;
    for (size_t i = 0; i < totalTodos; ++i) {
        if (!NetSync::getTodo(i).completed) ++openCount;
    }

    char taskSummary[36];
    if (totalTodos == 0) {
        snprintf(taskSummary, sizeof(taskSummary), "CalDAV: %s", NetSync::lastStatus());
    } else if (openCount == 0) {
        snprintf(taskSummary, sizeof(taskSummary), "All %u tasks completed", (unsigned)totalTodos);
    } else {
        snprintf(taskSummary, sizeof(taskSummary), "%u of %u tasks pending",
                 (unsigned)openCount, (unsigned)totalTodos);
    }
    drawCenteredText(display, taskSummary, 100, 150);

    // Bottom divider line
    display.drawFastHLine(10, 168, 180, 0);

    // Clean button actions footer
    display.setCursor(14, 178);
    display.print("B1: APPS");

    display.setCursor(126, 178);
    display.print("B2: SYNC");
}

} // namespace WatchfaceClock
