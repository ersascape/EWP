#include "app_calendar.h"
#include "core/debug_log.h"
#include <Arduino.h>
#include <Fonts/FreeSansBold9pt7b.h>

namespace AppCalendar {

namespace {
uint16_t viewYear = 0;
uint8_t viewMonth = 0;
bool userInteracted = false;

const char* const monthNames[] = {
    "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
    "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
};

const char* const dowHeaders[] = {
    "SU", "MO", "TU", "WE", "TH", "FR", "SA"
};

uint8_t daysInMonth(uint16_t year, uint8_t month) {
    if (month == 2) {
        const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
    return 31;
}

void nextMonth() {
    if (++viewMonth > 12) {
        viewMonth = 1;
        if (++viewYear > 2099) viewYear = 2099;
    }
}

void prevMonth() {
    if (--viewMonth < 1) {
        viewMonth = 12;
        if (--viewYear < 2000) viewYear = 2000;
    }
}

void drawCentered(Adafruit_GFX& display, const char* text, int16_t y, const GFXfont* font = nullptr) {
    display.setFont(font);
    display.setTextSize(1);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((display.width() - int16_t(w)) / 2 - x1, y - y1);
    display.print(text);
}

void syncWithTime(const DateTime& now) {
    const uint16_t y = now.year();
    const uint8_t m = now.month();
    viewYear = (y >= 2000 && y <= 2099) ? y : 2026;
    viewMonth = (m >= 1 && m <= 12) ? m : 1;
}
} // namespace

void begin() {
    userInteracted = false;
    viewYear = 0;
    viewMonth = 0;
}

void resetToCurrentMonth(const DateTime& now) {
    syncWithTime(now);
    userInteracted = false;
    DebugLog::log("CAL reset to %04u-%02u", unsigned(viewYear), unsigned(viewMonth));
}

bool onButton(Buttons::Event event, const DateTime& now) {
    if (!userInteracted || viewYear == 0) {
        syncWithTime(now);
    }

    if (event == Buttons::Event::Action) {
        nextMonth();
        userInteracted = true;
        DebugLog::log("CAL next month: %04u-%02u", unsigned(viewYear), unsigned(viewMonth));
        return true;
    } else if (event == Buttons::Event::ActionAlt || event == Buttons::Event::ActionLong) {
        resetToCurrentMonth(now);
        return true;
    }
    return false;
}

void render(Adafruit_GFX& display, const DateTime& now) {
    // Pick up real clock time dynamically from RTC / NTP
    if (!userInteracted || viewYear == 0) {
        syncWithTime(now);
    }

    // Defensive clamping to prevent any memory out-of-bounds
    if (viewMonth < 1) viewMonth = 1;
    if (viewMonth > 12) viewMonth = 12;
    if (viewYear < 2000) viewYear = 2000;
    if (viewYear > 2099) viewYear = 2099;

    // Title: Month & Year (e.g. "SEPTEMBER 2026")
    char title[32];
    snprintf(title, sizeof(title), "%s %u", monthNames[viewMonth - 1], unsigned(viewYear));
    drawCentered(display, title, 18, &FreeSansBold9pt7b);

    // Weekday Headers (SU, MO, TU, WE, TH, FR, SA)
    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(0); // Black

    constexpr int16_t startX = 16;
    constexpr int16_t colWidth = 24;

    for (uint8_t c = 0; c < 7; ++c) {
        int16_t x = startX + c * colWidth + 6;
        display.setCursor(x, 34);
        display.print(dowHeaders[c]);
    }

    // Top divider under headers
    display.drawFastHLine(14, 46, 172, 0);

    // Calculate first day of week and days in month
    DateTime firstDay(viewYear, viewMonth, 1, 0, 0, 0);
    const uint8_t startDow = firstDay.dayOfTheWeek(); // 0 = Sunday
    const uint8_t totalDays = daysInMonth(viewYear, viewMonth);

    const bool isCurrentMonth = (viewYear == now.year() && viewMonth == now.month());
    const uint8_t currentDay = now.day();

    // Day numbers matrix (6 rows of 18px height)
    constexpr int16_t startY = 50;
    constexpr int16_t rowHeight = 18;

    for (uint8_t d = 1; d <= totalDays; ++d) {
        const uint8_t slot = startDow + d - 1;
        const uint8_t col = slot % 7;
        const uint8_t row = slot / 7;

        const int16_t cellX = startX + col * colWidth;
        const int16_t cellY = startY + row * rowHeight;

        const bool isToday = (isCurrentMonth && d == currentDay);

        if (isToday) {
            // Highlight today with filled rounded rectangle and white text
            display.fillRoundRect(cellX + 1, cellY, 22, 15, 3, 0); // Black fill
            display.setTextColor(1); // White
        } else {
            display.setTextColor(0); // Black
        }

        const int16_t numX = (d < 10) ? (cellX + 9) : (cellX + 6);
        display.setCursor(numX, cellY + 4);
        display.print(d);
    }

    // Reset text color
    display.setTextColor(0);
}

} // namespace AppCalendar
