#pragma once
#include <Adafruit_GFX.h>
#include "core/buttons.h"

namespace AppPortal {

void begin();
void stop();
void tick();
void render(Adafruit_GFX& display);
bool onButton(Buttons::Event event);
bool isActive();

} // namespace AppPortal
