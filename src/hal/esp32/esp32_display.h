#pragma once

#if defined(ARDUINO)

#include "ersa/hal/display.h"
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>

namespace ersa {
namespace hal {

class Esp32Display : public IDisplay {
public:
    using GxDisplayType = GxEPD2_BW<GxEPD2_154_GDEY0154D67, GxEPD2_154_GDEY0154D67::HEIGHT>;

    Esp32Display(int cs, int dc, int rst, int busy, int sck, int miso, int mosi);
    ~Esp32Display() override = default;

    Result<void> init() override;
    int16_t width() const override;
    int16_t height() const override;

    void clear(Color color = Color::Black) override;
    void drawPixel(int16_t x, int16_t y, Color color) override;
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, Color color) override;

    void refresh(bool full = false) override;
    void refreshRect(const Rect& rect) override;
    bool isBusy() const override;
    void setBusyCallback(DisplayBusyCallback cb, void* userData = nullptr) override;

    void powerOff() override;
    void powerOn() override;
    bool isPowered() const override;

    const uint8_t* getBuffer() const override;
    uint8_t* getBuffer() override;

    // Direct access to underlying GxEPD2 / Adafruit_GFX for existing rendering code
    GxDisplayType& getGxDisplay();
    Adafruit_GFX& getGfx();

private:
    GxDisplayType display_;
    int cs_, dc_, rst_, busy_, sck_, miso_, mosi_;
    DisplayBusyCallback busyCb_{nullptr};
    void* busyUserData_{nullptr};
    bool powered_{false};

    static void staticBusyCallback(const void* p);
    static Esp32Display* s_instance;
};

} // namespace hal
} // namespace ersa

#endif // ARDUINO
