#pragma once

#include <stdint.h>

namespace ersa {
namespace board {

struct BoardCapabilities {
    bool wifi{false};
    bool bluetooth{false};
    bool rtc{false};
    bool batteryGauge{false};
    bool haptics{false};
    bool touch{false};
    bool buttons{false};
};

struct DisplayConfig {
    uint16_t width{200};
    uint16_t height{200};
    bool partialRefresh{true};
    bool isEpaper{true};
};

struct PinConfig {
    int sda{-1};
    int scl{-1};
    int sck{-1};
    int mosi{-1};
    int miso{-1};
    int epdCs{-1};
    int epdDc{-1};
    int epdRst{-1};
    int epdBusy{-1};
    int button1{-1};
    int button2{-1};
    int batteryAdc{-1};
};

struct BoardConfig {
    const char* name{"Generic Board"};
    BoardCapabilities capabilities;
    DisplayConfig display;
    PinConfig pins;
};

} // namespace board
} // namespace ersa
