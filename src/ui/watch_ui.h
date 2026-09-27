#pragma once
#include "core/buttons.h"

namespace WatchUi {

void begin();
void onButton(Buttons::Event event);
void tick();

} // namespace WatchUi
