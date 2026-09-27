#include "app_drawer.h"
#include "core/debug_log.h"
#include <Arduino.h>
#include <Fonts/FreeSansBold9pt7b.h>

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
    // Top Title
    drawCentered(display, "APP DRAWER", 16, &FreeSansBold9pt7b);

    // Divider line
    display.drawFastHLine(10, 26, 180, 0);

    display.setFont(nullptr);
    display.setTextSize(1);

    // List of apps
    constexpr int16_t startY = 32;
    constexpr int16_t rowHeight = 21;

    for (uint8_t i = 0; i < static_cast<uint8_t>(Item::Count); ++i) {
        const int16_t itemY = startY + i * rowHeight;
        const bool isSelected = (i == static_cast<uint8_t>(currentSelection));

        if (isSelected) {
            display.fillRoundRect(10, itemY, 180, 19, 4, 0);
            display.setTextColor(1); // White
            display.setCursor(18, itemY + 6);
            display.print("-> ");
            display.print(labels[i]);
            display.setTextColor(0); // Reset Black
        } else {
            display.setTextColor(0);
            display.setCursor(24, itemY + 6);
            display.print(labels[i]);
        }
    }

    // Bottom Divider
    display.drawFastHLine(10, 162, 180, 0);

    // Footer instruction
    drawCentered(display, "B1: NEXT   B2: OPEN", 173);
    drawCentered(display, "Hold B1: Watchface", 187);
}

} // namespace AppDrawer
