#if defined(ARDUINO)

#include "hal/esp32/esp32_input.h"
#include "core/buttons.h"
#include <Arduino.h>

namespace ersa {
namespace hal {

Esp32Input::Esp32Input(int topPin, int bottomPin, events::EventBus& bus)
    : topPin_(topPin), bottomPin_(bottomPin), bus_(bus) {}

Result<void> Esp32Input::init() {
    Buttons::begin();
    return Result<void>();
}

void Esp32Input::poll() {
    Buttons::tick();
    const auto legacy = Buttons::takeEvent();
    if (legacy == Buttons::Event::None) return;

    events::ButtonId btn = events::ButtonId::Unknown;
    events::ButtonAction act = events::ButtonAction::Click;

    switch (legacy) {
        case Buttons::Event::Next:
            btn = events::ButtonId::Button1;
            act = events::ButtonAction::Click;
            break;
        case Buttons::Event::Previous:
            btn = events::ButtonId::Button1;
            act = events::ButtonAction::DoubleClick;
            break;
        case Buttons::Event::Home:
            btn = events::ButtonId::Button1;
            act = events::ButtonAction::LongPress;
            break;
        case Buttons::Event::Action:
            btn = events::ButtonId::Button2;
            act = events::ButtonAction::Click;
            break;
        case Buttons::Event::ActionAlt:
            btn = events::ButtonId::Button2;
            act = events::ButtonAction::DoubleClick;
            break;
        case Buttons::Event::ActionLong:
            btn = events::ButtonId::Button2;
            act = events::ButtonAction::LongPress;
            break;
        default:
            return;
    }

    events::Event evt = events::Event::createInput(btn, act, millis());
    bus_.post(evt);
}

bool Esp32Input::isPressed(events::ButtonId button) const {
    if (button == events::ButtonId::Button1) {
        return digitalRead(topPin_) == LOW;
    } else if (button == events::ButtonId::Button2) {
        return digitalRead(bottomPin_) == LOW;
    }
    return false;
}

} // namespace hal
} // namespace ersa

#endif // ARDUINO
