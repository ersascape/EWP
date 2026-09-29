#if defined(ARDUINO)

#include "bsp/terra/board_terra.h"
#include "board_pins.h"
#include <Arduino.h>

namespace ersa {
namespace board {

const DeviceInfo& terraDeviceInfo();

BoardTerra& BoardTerra::instance() {
    static BoardTerra s_board;
    return s_board;
}

BoardTerra::BoardTerra()
    : display_(Pins::EPD_CS, Pins::EPD_DC, Pins::EPD_RST, Pins::EPD_BUSY,
               Pins::SCK, Pins::MISO, Pins::MOSI),
      rtc_(Pins::SDA, Pins::SCL),
      battery_(Pins::BATTERY_ADC),
      input_(Pins::BUTTON_1, Pins::BUTTON_2) {
    config_.name = getDeviceInfo().name;
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

const DeviceInfo& BoardTerra::getDeviceInfo() const {
    return terraDeviceInfo();
}

Result<void> BoardTerra::init() {
    Board::setCurrent(this);
    // The board owns construction and pin mapping. Service managers initialize
    // RTC, display, battery and BLE in the system boot sequence; initializing
    // those devices here too caused duplicate peripheral/radio startup.
    return input_.init();
}

uint32_t BoardTerra::getUptimeMs() const {
    return millis();
}

void BoardTerra::delayMs(uint32_t ms) {
    delay(ms);
}

} // namespace board
} // namespace ersa

#endif // ARDUINO
