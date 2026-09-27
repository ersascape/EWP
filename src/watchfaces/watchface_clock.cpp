#include "watchface_clock.h"
#include "ui/watch_icons.h"
#include "core/watch_clock.h"
#include "core/watch_config.h"
#include "core/battery.h"
#include "core/net_sync.h"
#include <Arduino.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <ctype.h>

namespace WatchfaceClock {

namespace {
const char* const weekdays[] = {
    "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

const char* const months[] = {
    "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
    "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

void drawCenteredTime(Adafruit_GFX& display, const char* str, int16_t targetCenterX, int16_t targetCenterY, int16_t& outEndX, int16_t& outTopY) {
    display.setFont(&FreeSansBold24pt7b);
    display.setTextSize(1);
    display.setTextColor(0);

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);

    const int16_t curX = targetCenterX - int16_t(w / 2) - x1;
    const int16_t curY = targetCenterY - int16_t(h / 2) - y1;
    display.setCursor(curX, curY);
    display.print(str);

    outEndX = curX + x1 + w;
    outTopY = curY + y1;
}
} // namespace

void render(Adafruit_GFX& display, const DateTime& time) {
    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(0); // Black

    // 1. TOP BAR: Date Badge (Left) & Battery Meter (Right)
    char dateBuf[20];
    const uint8_t dow = time.dayOfTheWeek() % 7;
    const uint8_t mon = (time.month() >= 1 && time.month() <= 12) ? (time.month() - 1) : 0;
    snprintf(dateBuf, sizeof(dateBuf), "%s %u %s",
             weekdays[dow],
             unsigned(time.day()),
             months[mon]);

    // Inverted pill badge for date
    int16_t dx, dy;
    uint16_t dw, dh;
    display.getTextBounds(dateBuf, 0, 0, &dx, &dy, &dw, &dh);
    const int16_t pillW = dw + 12;
    display.fillRoundRect(8, 6, pillW, 18, 4, 0);
    display.setTextColor(1); // White
    display.setCursor(14, 11);
    display.print(dateBuf);
    display.setTextColor(0); // Reset to Black

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
    display.setCursor(164 - int16_t(bw), 11);
    display.print(battStr);
    display.drawBitmap(168, 7, Battery::iconBitmap(), 24, 16, 0);

    // Subtle header divider
    display.drawFastHLine(8, 30, 184, 0);

    // 2. TIME DISPLAY: Large FreeSansBold24pt7b
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

    int16_t timeEndX = 0, timeTopY = 0;
    drawCenteredTime(display, timeBuf, 100, 68, timeEndX, timeTopY);

    // AM/PM badge if 12-hour mode
    if (!cfg.militaryTime) {
        display.setFont(nullptr);
        display.setTextSize(1);
        display.fillRoundRect(timeEndX + 3, timeTopY + 2, 20, 11, 2, 0);
        display.setTextColor(1);
        display.setCursor(timeEndX + 5, timeTopY + 4);
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

    // Mid divider
    display.drawFastHLine(8, 112, 184, 0);

    // 3. SMART GLANCE CARD
    constexpr int16_t cardX = 8;
    constexpr int16_t cardY = 118;
    constexpr int16_t cardW = 184;
    constexpr int16_t cardH = 72;

    display.drawRoundRect(cardX, cardY, cardW, cardH, 5, 0);

    // Inverted header tab
    display.fillRoundRect(cardX, cardY, 56, 13, 3, 0);
    display.setTextColor(1);
    display.setCursor(cardX + 6, cardY + 3);
    display.print("AGENDA");
    display.setTextColor(0);

    if (!WatchClock::healthy()) {
        display.fillRoundRect(cardX + cardW - 74, cardY, 74, 13, 3, 0);
        display.setTextColor(1);
        display.setCursor(cardX + cardW - 70, cardY + 3);
        display.print("RTC ERROR");
        display.setTextColor(0);
    } else if (NetSync::isSyncing()) {
        display.setCursor(cardX + cardW - 56, cardY + 3);
        display.print("SYNCING");
    }

    // Row 1: Next event title & time
    display.setFont(nullptr);
    display.setTextSize(1);
    const size_t totalEvents = NetSync::eventCount();
    if (totalEvents > 0) {
        const auto& ev = NetSync::getEvent(0);
        char titleBuf[25];
        strncpy(titleBuf, ev.title, sizeof(titleBuf) - 1);
        titleBuf[sizeof(titleBuf) - 1] = '\0';

        display.setCursor(cardX + 8, cardY + 18);
        display.print(titleBuf);

        display.setCursor(cardX + 8, cardY + 31);
        display.print("@ ");
        display.print(ev.timeStr);
    } else {
        display.setCursor(cardX + 8, cardY + 18);
        display.print("No events scheduled");
        display.setCursor(cardX + 8, cardY + 31);
        display.print("B1: Apps  B2: Actions");
    }

    // Inner horizontal divider inside glance card
    display.drawFastHLine(cardX + 6, cardY + 44, cardW - 12, 0);

    // Row 2: Tasks summary
    const size_t totalTodos = NetSync::todoCount();
    size_t openCount = 0;
    const char* firstOpenTitle = nullptr;

    for (size_t i = 0; i < totalTodos; ++i) {
        const auto& td = NetSync::getTodo(i);
        if (!td.completed) {
            ++openCount;
            if (!firstOpenTitle) firstOpenTitle = td.title;
        }
    }

    display.setCursor(cardX + 8, cardY + 54);
    if (totalTodos == 0) {
        display.print("[ ] No tasks in CalDAV");
    } else if (openCount == 0) {
        display.print("[X] All tasks completed!");
    } else {
        char taskBuf[25];
        if (firstOpenTitle) {
            snprintf(taskBuf, sizeof(taskBuf), "[ ] %.17s", firstOpenTitle);
        } else {
            snprintf(taskBuf, sizeof(taskBuf), "[ ] %u task%s pending",
                     (unsigned)openCount, openCount > 1 ? "s" : "");
        }
        display.print(taskBuf);
    }

    // Right-aligned task completion count badge (e.g. "1/3")
    if (totalTodos > 0) {
        char countStr[12];
        snprintf(countStr, sizeof(countStr), "%u/%u",
                 (unsigned)(totalTodos - openCount), (unsigned)totalTodos);
        int16_t cx1, cy1;
        uint16_t cw, ch;
        display.getTextBounds(countStr, 0, 0, &cx1, &cy1, &cw, &ch);
        display.setCursor(cardX + cardW - 8 - int16_t(cw), cardY + 54);
        display.print(countStr);
    }
}

} // namespace WatchfaceClock
