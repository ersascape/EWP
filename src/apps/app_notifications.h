#pragma once
#include <Adafruit_GFX.h>
#include "core/buttons.h"

namespace AppNotifications {

void begin();
bool onButton(Buttons::Event event);
void render(Adafruit_GFX& display, bool full = true);

} // namespace AppNotifications
