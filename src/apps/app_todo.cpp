#include "app_todo.h"
#include "core/net_sync.h"
#include "core/debug_log.h"
#include <Arduino.h>
#include <Fonts/FreeSansBold9pt7b.h>

namespace AppTodo {

namespace {
size_t selectedIndex = 0;
size_t topVisibleIndex = 0;
constexpr size_t VISIBLE_ITEMS = 4;

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
    selectedIndex = 0;
    topVisibleIndex = 0;
}

bool onButton(Buttons::Event event) {
    const size_t total = NetSync::todoCount();
    if (total == 0) {
        if (event == Buttons::Event::Action || event == Buttons::Event::ActionLong) {
            NetSync::syncAll();
            return true;
        }
        return false;
    }

    if (event == Buttons::Event::Action) {
        // Toggle the selected task
        NetSync::toggleTodo(selectedIndex);

        // Move to the next task
        selectedIndex = (selectedIndex + 1) % total;
        if (selectedIndex >= topVisibleIndex + VISIBLE_ITEMS) {
            topVisibleIndex = selectedIndex - VISIBLE_ITEMS + 1;
        } else if (selectedIndex < topVisibleIndex) {
            topVisibleIndex = selectedIndex;
        }
        return true;
    } else if (event == Buttons::Event::ActionLong) {
        // Trigger CalDAV & NTP sync
        NetSync::syncAll();
        return true;
    }
    return false;
}

void render(Adafruit_GFX& display) {
    drawCentered(display, "CALDAV TASKS", 16, &FreeSansBold9pt7b);

    display.setFont(nullptr);
    display.setTextSize(1);
    display.setTextColor(0); // Black

    const size_t total = NetSync::todoCount();

    char countBuf[32];
    snprintf(countBuf, sizeof(countBuf), "%u tasks (%s)",
             unsigned(total), NetSync::lastStatus());
    drawCentered(display, countBuf, 30);

    display.drawFastHLine(14, 38, 172, 0);

    if (total == 0) {
        drawCentered(display, "No tasks cached", 76);
        drawCentered(display, "Press B2 to sync", 96);
        return;
    }

    if (selectedIndex >= total) selectedIndex = 0;
    if (topVisibleIndex >= total) topVisibleIndex = 0;

    constexpr int16_t startY = 46;
    constexpr int16_t rowHeight = 24;

    for (size_t i = 0; i < VISIBLE_ITEMS; ++i) {
        const size_t idx = topVisibleIndex + i;
        if (idx >= total) break;

        const int16_t rowY = startY + i * rowHeight;
        const auto& item = NetSync::getTodo(idx);
        const bool isSelected = (idx == selectedIndex);

        if (isSelected) {
            display.drawRoundRect(14, rowY - 2, 172, 22, 3, 0);
        }

        // Checkbox: [ ] or [X]
        display.setCursor(20, rowY + 3);
        if (item.completed) {
            display.print("[X] ");
        } else {
            display.print("[ ] ");
        }

        char truncated[24];
        strncpy(truncated, item.title, sizeof(truncated) - 1);
        truncated[sizeof(truncated) - 1] = '\0';
        display.print(truncated);
    }

    // Scrollbar indicator
    if (total > VISIBLE_ITEMS) {
        constexpr int16_t barX = 190;
        constexpr int16_t barY = 46;
        constexpr int16_t barH = 92;
        display.drawFastVLine(barX, barY, barH, 0);

        const int16_t thumbH = max(8, int((VISIBLE_ITEMS * barH) / total));
        const int16_t thumbY = barY + int((topVisibleIndex * (barH - thumbH)) / (total - VISIBLE_ITEMS));
        display.fillRect(barX - 1, thumbY, 3, thumbH, 0);
    }
}

} // namespace AppTodo
