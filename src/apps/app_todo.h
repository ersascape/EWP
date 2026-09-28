#pragma once
#include <Adafruit_GFX.h>
#include "core/buttons.h"

namespace AppTodo {

void begin();
void render(Adafruit_GFX& display, bool full = true);
bool onButton(Buttons::Event event);

} // namespace AppTodo
