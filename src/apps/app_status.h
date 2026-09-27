#pragma once
#include <Adafruit_GFX.h>
#include "core/buttons.h"

namespace AppStatus {

void render(Adafruit_GFX& display);
bool onButton(Buttons::Event event);

} // namespace AppStatus
