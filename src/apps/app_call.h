#pragma once

#include "core/buttons.h"
#include <Adafruit_GFX.h>

namespace AppCall {

void begin();
bool onButton(Buttons::Event event);
void render(Adafruit_GFX& display);
bool isCallActiveOrIncoming();

} // namespace AppCall
