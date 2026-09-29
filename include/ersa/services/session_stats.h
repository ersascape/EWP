#pragma once

#include <stdint.h>

namespace ersa { namespace services {

class SessionStats {
public:
    static void begin();
    static void tick(uint32_t uptimeMs);
    static uint32_t previousSessionUptimeSeconds();
};

} }
