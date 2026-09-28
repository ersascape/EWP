#pragma once

#include "ersa/common/types.h"
#include <stdint.h>
#include <stddef.h>

namespace ersa {
namespace hal {

enum class Color : uint8_t {
    Black = 0,
    White = 1,
    Inverse = 2
};

using DisplayBusyCallback = void (*)(void* userData);

class IDisplay {
public:
    virtual ~IDisplay() = default;

    virtual Result<void> init() = 0;
    virtual int16_t width() const = 0;
    virtual int16_t height() const = 0;

    // Buffer manipulation
    virtual void clear(Color color = Color::Black) = 0;
    virtual void drawPixel(int16_t x, int16_t y, Color color) = 0;
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, Color color) = 0;

    // Refresh control
    virtual void refresh(bool full = false) = 0;
    virtual void refreshRect(const Rect& rect) = 0;
    virtual bool isBusy() const = 0;
    virtual void setBusyCallback(DisplayBusyCallback cb, void* userData = nullptr) = 0;

    // Power management
    virtual void powerOff() = 0;
    virtual void powerOn() = 0;
    virtual bool isPowered() const = 0;

    // Direct framebuffer access if available
    virtual const uint8_t* getBuffer() const = 0;
    virtual uint8_t* getBuffer() = 0;
};

} // namespace hal
} // namespace ersa
