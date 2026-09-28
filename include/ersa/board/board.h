#pragma once

#include "ersa/common/types.h"
#include "ersa/hal/display.h"
#include "ersa/hal/rtc.h"
#include "ersa/hal/battery.h"
#include "ersa/hal/input.h"

namespace ersa {
namespace board {

class Board {
public:
    virtual ~Board() = default;

    virtual Result<void> init() = 0;
    virtual const char* getName() const = 0;

    virtual hal::IDisplay& getDisplay() = 0;
    virtual hal::IRtc& getRtc() = 0;
    virtual hal::IBattery& getBattery() = 0;
    virtual hal::IInput& getInput() = 0;

    virtual uint32_t getUptimeMs() const = 0;
    virtual void delayMs(uint32_t ms) = 0;

    static Board& current();
    static void setCurrent(Board* board);
};

} // namespace board
} // namespace ersa
