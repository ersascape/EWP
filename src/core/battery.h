#pragma once
#include <stdint.h>

namespace Battery {

void begin();
void tick();
uint16_t millivolts();
uint8_t percentage();
bool isConnected();
const uint8_t* iconBitmap();

} // namespace Battery
