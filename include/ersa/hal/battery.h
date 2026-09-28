#pragma once

#include "ersa/common/types.h"
#include <stdint.h>

namespace ersa {
namespace hal {

class IBattery {
public:
    virtual ~IBattery() = default;

    virtual Result<void> init() = 0;
    virtual void sample() = 0;
    virtual uint16_t millivolts() const = 0;
    virtual uint8_t percentage() const = 0;
    virtual bool isConnected() const = 0;
    virtual bool isCharging() const = 0;
};

} // namespace hal
} // namespace ersa
