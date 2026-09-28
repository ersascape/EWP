#pragma once

#if defined(ARDUINO)

#include "ersa/hal/input.h"
#include "ersa/events/event_bus.h"

namespace ersa {
namespace hal {

class Esp32Input : public IInput {
public:
    Esp32Input(int topPin, int bottomPin, events::EventBus& bus = events::EventBus::instance());
    ~Esp32Input() override = default;

    Result<void> init() override;
    void poll() override;
    bool isPressed(events::ButtonId button) const override;

private:
    int topPin_, bottomPin_;
    events::EventBus& bus_;
};

} // namespace hal
} // namespace ersa

#endif // ARDUINO
