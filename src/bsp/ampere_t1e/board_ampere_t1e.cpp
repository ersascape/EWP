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
      input_(Pins::BUTTON_1, Pins::BUTTON_2) {
    config_.name = "Ampere Works T1E";
    config_.capabilities.wifi = true;
    config_.capabilities.bluetooth = true;
    config_.capabilities.rtc = true;
    config_.capabilities.batteryGauge = true;
    config_.capabilities.buttons = true;
    config_.capabilities.haptics = false;
    config_.capabilities.touch = false;

    config_.display.width = 200;
    config_.display.height = 200;
    config_.display.partialRefresh = true;
    config_.display.isEpaper = true;

    config_.pins.sda = Pins::SDA;
    config_.pins.scl = Pins::SCL;
    config_.pins.sck = Pins::SCK;
    config_.pins.mosi = Pins::MOSI;
    config_.pins.miso = Pins::MISO;
    config_.pins.epdCs = Pins::EPD_CS;
    config_.pins.epdDc = Pins::EPD_DC;
    config_.pins.epdRst = Pins::EPD_RST;
    config_.pins.epdBusy = Pins::EPD_BUSY;
    config_.pins.button1 = Pins::BUTTON_1;
    config_.pins.button2 = Pins::BUTTON_2;
    config_.pins.batteryAdc = Pins::BATTERY_ADC;
}

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
