#pragma once

// Raw ESP32-C3 GPIO numbers, verified against the supplied PCB JSON.
namespace Pins {
constexpr int SDA = 6, SCL = 7;
constexpr int SCK = 8, MOSI = 10, MISO = -1;
constexpr int EPD_CS = 5, EPD_DC = 20, EPD_RST = 21, EPD_BUSY = 9;
// Upper S2: net U8_3 -> XIAO pad 3 (D2/A2) -> GPIO4.
// Lower S1: net U8_2 -> XIAO pad 2 (D1/A1) -> GPIO3.
// U3 in the old design is not populated on the user's assembled board.
constexpr int BUTTON_1 = 4, BUTTON_2 = 3;
constexpr int BATTERY_ADC = 2; // XIAO pad 1 (D0/A0) -> GPIO2
}
