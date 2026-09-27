#pragma once
#include <stdint.h>

namespace DebugLog {
void begin();
void log(const char* format, ...) __attribute__((format(printf, 1, 2)));
void tick();
uint32_t bootCount();
const char* resetReasonName();
}
