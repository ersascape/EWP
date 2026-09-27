#pragma once
#include <Adafruit_GFX.h>

namespace AppDrawer {

enum class Item : uint8_t {
    Clock = 0,
    Calendar,
    Agenda,
    Todo,
    Hotspot,
    Status,
    Count
};

void begin();
void next();
void previous();
Item selected();
void setSelected(Item item);
void render(Adafruit_GFX& display);

} // namespace AppDrawer
