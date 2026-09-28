#if defined(ARDUINO)

#include "hal/esp32/esp32_display.h"
#include "core/buttons.h"
#include <SPI.h>

namespace ersa {
namespace hal {

Esp32Display* Esp32Display::s_instance = nullptr;

Esp32Display::Esp32Display(int cs, int dc, int rst, int busy, int sck, int miso, int mosi)
    : display_(GxEPD2_154_GDEY0154D67(cs, dc, rst, busy)),
      cs_(cs), dc_(dc), rst_(rst), busy_(busy), sck_(sck), miso_(miso), mosi_(mosi) {
    s_instance = this;
}

void Esp32Display::staticBusyCallback(const void* p) {
    (void)p;
    Buttons::tick();
    if (s_instance && s_instance->busyCb_) {
        s_instance->busyCb_(s_instance->busyUserData_);
    }
    delay(1);
}

Result<void> Esp32Display::init() {
    SPI.begin(sck_, miso_, mosi_, cs_);
    display_.epd2.selectSPI(SPI, SPISettings(4000000, MSBFIRST, SPI_MODE0));
    display_.init(0, true, 10, false);
    display_.epd2.setBusyCallback(staticBusyCallback);
    display_.setRotation(0);
    powered_ = true;
    return Result<void>();
}

int16_t Esp32Display::width() const {
    return display_.width();
}

int16_t Esp32Display::height() const {
    return display_.height();
}

void Esp32Display::clear(Color color) {
    uint16_t c = (color == Color::White) ? GxEPD_WHITE : GxEPD_BLACK;
    display_.fillScreen(c);
}

void Esp32Display::drawPixel(int16_t x, int16_t y, Color color) {
    uint16_t c = (color == Color::White) ? GxEPD_WHITE : GxEPD_BLACK;
    display_.drawPixel(x, y, c);
}

void Esp32Display::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, Color color) {
    uint16_t c = (color == Color::White) ? GxEPD_WHITE : GxEPD_BLACK;
    display_.fillRect(x, y, w, h, c);
}

void Esp32Display::refresh(bool full) {
    if (full) {
        display_.setFullWindow();
        display_.display(false); // Full refresh with clearing waveform
    } else {
        display_.display(true);  // Fast partial refresh of full screen (differential, no flash)
    }
    powered_ = true;
}

void Esp32Display::refreshRect(const Rect& rect) {
    display_.displayWindow(rect.x, rect.y, rect.w, rect.h);
    powered_ = true;
}

bool Esp32Display::isBusy() const {
    return digitalRead(busy_) == HIGH;
}

void Esp32Display::setBusyCallback(DisplayBusyCallback cb, void* userData) {
    busyCb_ = cb;
    busyUserData_ = userData;
}

void Esp32Display::powerOff() {
    display_.powerOff();
    powered_ = false;
}

void Esp32Display::powerOn() {
    display_.init(0, false, 10, false);
    powered_ = true;
}

bool Esp32Display::isPowered() const {
    return powered_;
}

const uint8_t* Esp32Display::getBuffer() const {
    return nullptr;
}

uint8_t* Esp32Display::getBuffer() {
    return nullptr;
}

Esp32Display::GxDisplayType& Esp32Display::getGxDisplay() {
    return display_;
}

Adafruit_GFX& Esp32Display::getGfx() {
    return display_;
}

} // namespace hal
} // namespace ersa

#endif // ARDUINO
