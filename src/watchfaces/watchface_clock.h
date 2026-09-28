#pragma once
#include <RTClib.h>
#include <Adafruit_GFX.h>

namespace WatchfaceClock {

void render(Adafruit_GFX& display, const DateTime& time, bool full = true);

} // namespace WatchfaceClock
