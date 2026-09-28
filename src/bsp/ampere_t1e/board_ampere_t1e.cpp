#if defined(ARDUINO)

#include "bsp/ampere_t1e/board_ampere_t1e.h"
#include "board_pins.h"
#include <Arduino.h>

namespace ersa {
namespace board {

BoardAmpereT1e& BoardAmpereT1e::instance() {
    static BoardAmpereT1e s_board;
    return s_board;
}

BoardAmpereT1e::BoardAmpereT1e()
    : display_(Pins::EPD_CS, Pins::EPD_DC, Pins::EPD_RST, Pins::EPD_BUSY,
               Pins::SCK, Pins::MISO, Pins::MOSI),
      rtc_(Pins::SDA, Pins::SCL),
      battery_(Pins::BATTERY_ADC),
      input_(Pins::BUTTON_1, Pins::BUTTON_2) {}

Result<void> BoardAmpereT1e::init() {
    Board::setCurrent(this);
    input_.init();
    battery_.init();
    rtc_.init();
    display_.init();
    return Result<void>();
}

uint32_t BoardAmpereT1e::getUptimeMs() const {
    return millis();
}

void BoardAmpereT1e::delayMs(uint32_t ms) {
    delay(ms);
}

} // namespace board
} // namespace ersa

#endif // ARDUINO
