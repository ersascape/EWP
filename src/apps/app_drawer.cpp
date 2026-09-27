#include "app_drawer.h"
#include "core/debug_log.h"
#include "fonts/misans_fonts.h"
#include <Arduino.h>

namespace AppDrawer {

namespace {
Item currentSelection = Item::Calendar;

const char* const labels[static_cast<size_t>(Item::Count)] = {
    "Watchface",
    "Calendar",
    "CalDAV Agenda",
    "CalDAV Tasks",
    "Wi-Fi Hotspot",
    "Status & Sync"
};

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

void render(Adafruit_GFX& display) {
    // Clean Header with MiSans, NO cutting divider lines
    drawCentered(display, "APPS", 20, &MiSansLatin_Bold10pt7b);

    display.setFont(nullptr);
    display.setTextSize(1);

    // List of apps (6 items)
    constexpr int16_t startY = 34;
    constexpr int16_t rowHeight = 21;

    for (uint8_t i = 0; i < static_cast<uint8_t>(Item::Count); ++i) {
        const int16_t itemY = startY + i * rowHeight;
        const bool isSelected = (i == static_cast<uint8_t>(currentSelection));

        if (isSelected) {
            display.fillRoundRect(12, itemY - 2, 176, 19, 4, 0);
            display.setTextColor(1); // White
            display.setCursor(20, itemY + 6);
            display.print("-> ");
            display.print(labels[i]);
            display.setTextColor(0); // Reset Black
        } else {
            display.setTextColor(0);
            display.setCursor(26, itemY + 6);
            display.print(labels[i]);
        }
    }

    // Single Bottom Divider
    display.drawFastHLine(14, 166, 172, 0);

    // Footer instructions
    drawCentered(display, "B1: SCROLL   B2: OK", 176);
    drawCentered(display, "Hold B1: Watchface", 188);
}

} // namespace AppDrawer
