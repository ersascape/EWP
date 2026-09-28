#include "app_drawer.h"
#include "core/debug_log.h"
#include "fonts/misans_fonts.h"
#include "ersa/config/ui_strings.h"
#include <Arduino.h>

namespace AppDrawer {

namespace {
Item currentSelection = Item::Calendar;

const char* const labels[static_cast<size_t>(Item::Count)] = {
    ersa::strings::APP_TITLE_CLOCK,
    ersa::strings::APP_TITLE_CALENDAR,
    ersa::strings::APP_TITLE_AGENDA,
    ersa::strings::APP_TITLE_TASKS,
    ersa::strings::APP_TITLE_HOTSPOT,
    ersa::strings::APP_TITLE_STATUS
};

} // namespace

void begin() {
    currentSelection = Item::Calendar;
}

void next() {
    uint8_t val = static_cast<uint8_t>(currentSelection);
    val = (val + 1) % static_cast<uint8_t>(Item::Count);
    currentSelection = static_cast<Item>(val);
    DebugLog::log("DRAWER next selection: %u (%s)", val, labels[val]);
}

void previous() {
    uint8_t val = static_cast<uint8_t>(currentSelection);
    val = (val == 0) ? (static_cast<uint8_t>(Item::Count) - 1) : (val - 1);
    currentSelection = static_cast<Item>(val);
    DebugLog::log("DRAWER prev selection: %u (%s)", val, labels[val]);
}

Item selected() {
    return currentSelection;
}

void setSelected(Item item) {
    if (static_cast<uint8_t>(item) < static_cast<uint8_t>(Item::Count)) {
        currentSelection = item;
    }
}

void render(Adafruit_GFX& display, bool full) {
    if (full) {
        display.fillScreen(0);   // Solid black
        display.setTextColor(1); // White

        // Clean left-aligned lowercase header
        display.setFont(&MiSansLatin_Bold10pt7b);
        display.setCursor(18, 24);
        display.print(ersa::strings::APP_DRAWER_HEADER);

        // Minimal footer without harsh dividing lines
        display.setFont(&MiSansLatin_Regular8pt7b);
        display.setCursor(18, 186);
        display.print(ersa::strings::NAV_DRAWER_FOOTER);
    } else {
        // Partial refresh: clear ONLY the items region (y: 32 to 170)
        display.fillRect(0, 32, 200, 138, 0);
    }

    display.setTextColor(1);

    constexpr int16_t startY = 46;
    constexpr int16_t rowHeight = 21;

    for (uint8_t i = 0; i < static_cast<uint8_t>(Item::Count); ++i) {
        const int16_t itemY = startY + i * rowHeight;
        const bool isSelected = (i == static_cast<uint8_t>(currentSelection));

        if (isSelected) {
            display.fillRoundRect(14, itemY - 14, 172, 19, 4, 1);
            display.setTextColor(0); // Black text on white pill
            display.setFont(&MiSansLatin_Bold10pt7b);
            display.setCursor(24, itemY);
            display.print(labels[i]);
            display.setTextColor(1); // Reset White
        } else {
            display.setFont(&MiSansLatin_Regular10pt7b);
            display.setCursor(24, itemY);
            display.print(labels[i]);
        }
    }
}

} // namespace AppDrawer
