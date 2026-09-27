#pragma once
#include <Adafruit_GFX.h>
#include <RTClib.h>
#include "core/buttons.h"

namespace AppAgenda {

void begin();
void render(Adafruit_GFX& display, const DateTime& now);
bool onButton(Buttons::Event event);

} // namespace AppAgenda
