#pragma once
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include "core/buttons.h"

namespace AppCalendar {

void begin();
bool onButton(Buttons::Event event, const DateTime& now);
void render(Adafruit_GFX& display, const DateTime& now);
void resetToCurrentMonth(const DateTime& now);

} // namespace AppCalendar
