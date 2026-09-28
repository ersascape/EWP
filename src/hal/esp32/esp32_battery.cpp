#if defined(ARDUINO)

#include "hal/esp32/esp32_battery.h"
#include "core/battery.h"

namespace ersa {
namespace hal {

Esp32Battery::Esp32Battery(int adcPin)
    : adcPin_(adcPin) {}

Result<void> Esp32Battery::init() {
    Battery::begin();
    return Result<void>();
}

void Esp32Battery::sample() {
    Battery::tick();
}

uint16_t Esp32Battery::millivolts() const {
    return Battery::millivolts();
}

uint8_t Esp32Battery::percentage() const {
    return Battery::percentage();
}

bool Esp32Battery::isConnected() const {
    return Battery::isConnected();
}

bool Esp32Battery::isCharging() const {
    return Battery::isConnected() && Battery::millivolts() >= 4250;
}

} // namespace hal
} // namespace ersa

#endif // ARDUINO
