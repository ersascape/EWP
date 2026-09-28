#include "watchface_clock.h"
#include "core/watch_clock.h"
#include "core/battery.h"
#include "core/net_sync.h"
#include "fonts/misans_fonts.h"
#include "ersa/config/ui_strings.h"
#include <Arduino.h>

namespace WatchfaceClock {

namespace {

const char* const hoursWords[] = {
    "twelve", "one", "two", "three", "four", "five",
    "six", "seven", "eight", "nine", "ten", "eleven", "twelve"
};

const char* const onesWords[] = {
    "", "one", "two", "three", "four", "five",
    "six", "seven", "eight", "nine"
};

const char* const teensWords[] = {
    "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen",
    "sixteen", "seventeen", "eighteen", "nineteen"
};

const char* const tensWords[] = {
    "", "", "twenty", "thirty", "forty", "fifty"
};

const char* const daysLower[] = {
    "sunday", "monday", "tuesday", "wednesday",
    "thursday", "friday", "saturday"
};

const char* const monthsLower[] = {
    "january", "february", "march", "april", "may", "june",
    "july", "august", "september", "october", "november", "december"
};

const char* getOrdinalSuffix(uint8_t day) {
    if (day >= 11 && day <= 13) return "th";
    switch (day % 10) {
        case 1: return "st";
        case 2: return "nd";
        case 3: return "rd";
        default: return "th";
    }
}

void timeToWords(uint8_t hour, uint8_t min, const char*& line1, const char*& line2, const char*& line3) {
    uint8_t h12 = hour % 12;
    if (h12 == 0) h12 = 12;
    line1 = hoursWords[h12];
    line2 = "";
    line3 = "";

    if (min == 0) {
        line2 = "o'clock";
    } else if (min < 10) {
        line2 = "oh";
        line3 = onesWords[min];
    } else if (min < 20) {
        line2 = teensWords[min - 10];
    } else {
        line2 = tensWords[min / 10];
        if (min % 10 != 0) {
            line3 = onesWords[min % 10];
        }
    }
}

void drawRightAlignedText(Adafruit_GFX& display, const char* text, int16_t rightX, int16_t y, const GFXfont* font) {
    display.setFont(font);
    display.setTextSize(1);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(rightX - int16_t(w) - x1, y);
    display.print(text);
}

} // namespace

void render(Adafruit_GFX& display, const DateTime& time) {
    // 1. Full solid black canvas
    display.fillScreen(0);      // 0 = GxEPD_BLACK
    display.setTextColor(1);    // 1 = GxEPD_WHITE

    // 2. Generate words for time
    const char* l1 = "";
    const char* l2 = "";
    const char* l3 = "";
    timeToWords(time.hour(), time.minute(), l1, l2, l3);

    constexpr int16_t leftX = 18;

    if (l3[0] != '\0') {
        // 3 lines layout
        display.setFont(&MiSansLatin_Bold17pt7b);
        display.setCursor(leftX, 48);
        display.print(l1);

        display.setFont(&MiSansLatin_Light17pt7b);
        display.setCursor(leftX, 80);
        display.print(l2);

        display.setCursor(leftX, 112);
        display.print(l3);
    } else {
        // 2 lines layout: elegant vertical centering
        display.setFont(&MiSansLatin_Bold17pt7b);
        display.setCursor(leftX, 58);
        display.print(l1);

        display.setFont(&MiSansLatin_Light17pt7b);
        display.setCursor(leftX, 94);
        display.print(l2);
    }

    // 3. Bottom-Right Date: e.g. "thursday" / "november 12th, 2020"
    const uint8_t dow = time.dayOfTheWeek() % 7;
    const uint8_t mon = (time.month() >= 1 && time.month() <= 12) ? (time.month() - 1) : 0;

    char dateBuf[36];
    snprintf(dateBuf, sizeof(dateBuf), "%s %u%s, %u",
             monthsLower[mon], unsigned(time.day()),
             getOrdinalSuffix(time.day()), unsigned(time.year()));

    drawRightAlignedText(display, daysLower[dow], 184, 168, &MiSansLatin_Regular8pt7b);
    drawRightAlignedText(display, dateBuf, 184, 184, &MiSansLatin_Regular8pt7b);

    // 4. Subtle, minimal status in bottom-left corner
    if (NetSync::isSyncing()) {
        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(leftX, 184);
        display.print(ersa::strings::MSG_SYNCING);
    } else if (Battery::isConnected() && Battery::percentage() <= 20) {
        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(leftX, 184);
        display.print(ersa::strings::MSG_LOW_BATT);
    }
}

} // namespace WatchfaceClock
