#pragma once
#include <RTClib.h>

namespace WatchClock {

void begin();
void tick();
DateTime now();
bool healthy();
bool setAtBoot();

// Real-time synchronization (NTP or BLE phone time sync)
void setEpoch(uint32_t epochSeconds);
void adjust(const DateTime& time);

} // namespace WatchClock
