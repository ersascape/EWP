#pragma once

#include "core/buttons.h"
#include <Adafruit_GFX.h>

namespace AppPairing {

void begin();
bool onButton(Buttons::Event event);
void render(Adafruit_GFX& display);

} // namespace AppPairing
